#pragma once
#include "factory_drum_assets.h"
#include "future_factory_presets.h"
#include "soundfont_fixture.h"
#include "benchmark_project.h"
#include "project_files.h"
#include "data_layout.h"
#include "factory_library.h"
#include "wav_writer.h"
namespace groove {
inline constexpr unsigned kFactoryDemoCount=0; // Composition is deferred; never migrate songs into the pool.
struct FactoryInstallReport {unsigned checked=0,repaired=0,retired=0;};
#include "generated/factory_synth_drums.inc"
inline bool restore_factory_bytes(IFileAccess& fs,const char* path,const uint8_t* bytes,unsigned length,FactoryInstallReport& report){
 ++report.checked;if(cached_file_matches(fs,path,bytes,length))return true;
 char temp[192];if(!copy_text(temp,sizeof temp,path))return false;unsigned n=0;while(temp[n])++n;if(!copy_text(temp+n,sizeof temp-n,".tmp"))return false;
 FileWriter out(fs,temp);if(!out.write(bytes,length) || !out.finish() || !cached_file_matches(fs,temp,bytes,length))return false;
 if(!fs.remove(path) || !fs.move(temp,path))return false;
 ++report.repaired;return true;
}
inline bool restore_factory_project(IFileAccess& fs,const char* path,ProjectState& p,FactoryInstallReport& report){
 for(auto& asset:p.assets)if(asset.path[0] && !asset.metadata.content_crc && !file_checksum(fs,asset.path,asset.metadata.content_crc))return false;
 if(!project_paths(p,path,false))return false;
 ChunkBuffer encoded;if(!encode_project_buffer(encoded,p))return false;
 // A CRC-appended file has a fixed CRC residue. Compare actual bytes, not
 // the CRC of the complete container, to detect same-size valid edits too.
 return restore_factory_bytes(fs,path,encoded.data,encoded.size,report);
}
// Called before the audio worker starts. Firmware code is the source of truth;
// DATA copies are a browser-visible cache, never the factory master.
inline bool install_factory_content(IFileAccess& fs,const uint8_t* sample,unsigned sample_bytes,FactoryInstallReport& report){
 if(kFactoryLibraryFormatVersion!=kFormatVersion)return false;
 for(const char* path:kCanonicalDataDirectories)if(!fs.directory(path))return false;
 if(!sample || sample_bytes!=96044 || !restore_factory_bytes(fs,"FACTORY/DEBUG/demo-slices.wav",sample,sample_bytes,report))return false;
 auto* project=alloc_new<ProjectState>();if(!project)return false;bool ok=true;
 // A single reusable 48 KB PCM buffer, never on an audio or caller stack.
 auto* drum=alloc_array<uint8_t>(48044);if(!drum){delete project;return false;}
 for(unsigned kit=0;kit<kFactoryKitCount && ok;++kit){
  for(unsigned pad=0;pad<kFactoryKitPads && ok;++pad){char path[128];ok=factory_drum_path(path,sizeof path,kit,pad);unsigned bytes=factory_drum_pcm(drum,kit,pad);if(ok)ok=restore_factory_bytes(fs,path,drum,bytes,report);}
  if(ok){make_empty_project(*project);project->content_kind=ContentKind::Kit;factory_assign_kit(*project,track_at(*project,0,0),kit);project->tracks[0].preset_identity=builtin_factory_identity(0x200+kit);copy_text(project->tracks[0].preset_name,96,factory_kit_name(kit));char name[64],path[128];copy_text(name,sizeof name,factory_kit_name(kit));unsigned n=0;while(name[n])++n;copy_text(name+n,sizeof name-n,".mgxk");ok=join_path(path,sizeof path,"FACTORY/KITS",name) && restore_factory_project(fs,path,*project,report);}
 }
 delete[] drum;
 if(ok)ok=fs.directory("FACTORY/PRESETS/DRUMS");
 for(unsigned i=0;i<kFutureFactoryPresets && ok;++i){char name[112],path[160];ok=make_future_factory_preset(*project,i);if(!ok)break;copy_text(name,sizeof name,future_factory_preset_name(i));unsigned n=0;while(name[n])++n;copy_text(name+n,sizeof name-n,".mgxp");ok=join_path(path,sizeof path,i>=16?"FACTORY/PRESETS/DRUMS":"FACTORY/PRESETS",name) && restore_factory_project(fs,path,*project,report);}
 // Authored masters are restored losslessly; external audio stays on DATA.
 if(ok)ok=fs.directory("FACTORY/PRESETS/LIBRARY") && fs.directory("FACTORY/KITS/LIBRARY");
 auto* library=ok && allocation_admitted(kFactoryLibraryMaxBytes)?alloc_array<uint8_t>(kFactoryLibraryMaxBytes):nullptr;
 FactoryLibraryBank bank;if(kFactoryLibraryCount && (!library || !bank.ready()))ok=false;
 for(const auto& entry:kFactoryLibrary){if(!entry.path)continue;if(!ok)break;ok=bank.unpack(entry,library,kFactoryLibraryMaxBytes) && restore_factory_bytes(fs,entry.path,library,entry.bytes,report);}
 delete[] library;
 // Offline-rendered VA/FM originals: native presets plus a baked six-piece kit.
 if(ok)ok=fs.directory("FACTORY/PRESETS/DRUMS") && fs.directory("FACTORY/DRUMS/VA-FM");
 auto* synth=ok && allocation_admitted(kFactorySynthDrumMaxBytes)?alloc_array<uint8_t>(kFactorySynthDrumMaxBytes):nullptr;
 if(!synth)ok=false;
 for(const auto& entry:kFactorySynthDrums){if(!ok)break;ok=entry.bytes<=kFactorySynthDrumMaxBytes && entry.offset<=sizeof kFactorySynthDrumPacked && entry.packed<=sizeof kFactorySynthDrumPacked-entry.offset && unpack_factory_library(kFactorySynthDrumPacked+entry.offset,entry.packed,synth,entry.bytes,entry.sha256) && restore_factory_bytes(fs,entry.path,synth,entry.bytes,report);}
 delete[] synth;
 if(ok)ok=restore_factory_bytes(fs,"FACTORY/DEBUG/Sine.sf2",kSoundFontFixture,sizeof kSoundFontFixture,report);
 if(ok){make_empty_project(*project);project->content_kind=ContentKind::Preset;auto t=track_at(*project,0,0);t.instrument=Instrument::SoundFont;t.preset_identity=builtin_factory_identity(0x300);copy_text(t.preset_name,96,"SF2 Debug Sine");copy_text(project->assets[0].path,96,"FACTORY/DEBUG/Sine.sf2");ok=file_checksum(fs,project->assets[0].path,project->assets[0].metadata.content_crc) && restore_factory_project(fs,"FACTORY/DEBUG/SF2-Sine.mgxp",*project,report);}
 // Operational debug score only. New hardware load qualification is deferred
 // until the user and developer define its workload after the migration.
 if(ok){project->song=SongState{};make_benchmark_project(*project);ok=restore_factory_project(fs,"BENCH.MGXS",*project,report);}
 if(ok){make_benchmark_project(*project);ok=restore_factory_project(fs,"FACTORY/DEBUG/Benchmark.mgxs",*project,report);}
 // Editable debug synth patch: simple sine macro, no external routing.
 if(ok){make_empty_project(*project);project->content_kind=ContentKind::Preset;auto tr=track_at(*project,0,0);tr.instrument=Instrument::PlaitsMacro;tr.preset_identity=builtin_factory_identity(0x301);copy_text(tr.preset_name,96,"Test Tone");tr.plaits_model=0;tr.level=.2f;tr.root_note=69;tr.engine_parameters[0]=0;tr.engine_parameters[1]=0;tr.engine_parameters[2]=0;ok=restore_factory_project(fs,"FACTORY/DEBUG/Test-Tone.mgxp",*project,report);}
 if(ok){
  const char* drums[]={"808-Kick","808-Snare","Closed-Hat","Open-Hat","Clap","Noise-Rim","FM-Tom-Low","FM-Tom-High","FM-Cowbell","FM-Metal","Chip-Perc","Acid-Zap","VA-Hit","Noise-Sweep","909-Kick","909-Snare"};
  make_empty_project(*project);project->content_kind=ContentKind::Kit;auto tr=track_at(*project,0,0);tr.instrument=Instrument::DrumKit;tr.preset_identity=builtin_factory_identity(0x302);copy_text(tr.preset_name,96,"Techno 01");bool available=true;
  for(unsigned pad=0;pad<16 && available;++pad){auto& a=project->assets[pad];char name[64];copy_text(name,sizeof name,drums[pad]);unsigned n=0;while(name[n])++n;copy_text(name+n,sizeof name-n,".wav");join_path(a.path,sizeof a.path,"assets/factory",name);WavView view;uint64_t offset=0;
   available=sample_file_header(fs,a.path,view,offset) && file_checksum(fs,a.path,a.metadata.content_crc);if(!available)break;
   a.metadata.frames=view.frames;a.metadata.rate=view.rate;a.metadata.channels=uint8_t(view.channels);a.metadata.bits=view.bits;a.metadata.root=60;
   auto& sound=tr.kit.pads[pad];static constexpr uint8_t gm[]={36,38,42,46,39,37,45,50,56,51,75,76,77,55,35,40};sound.gm_note=gm[pad];sound.sample[0]=uint8_t(pad);sound.level=pad==0?.6f:.4f;sound.edit.choke=pad==2 || pad==3?1:0;
  }
  if(available)ok=restore_factory_project(fs,"FACTORY/KITS/Techno-01.mgxk",*project,report);
 }
 delete project;
 // Exact PCM16 440 Hz at -18 dBFS, one second, for sample/route diagnostics.
 const unsigned bytes=96044;auto* tone=alloc_array<uint8_t>(bytes);if(!tone)return false;
 wav_header_ch(tone,48000,48000,1);for(unsigned i=0;i<48000;++i){int16_t s=int16_t(4125.0*sin(6.283185307179586*440.0*double(i)/48000.0));tone[44+2*i]=uint8_t(s);tone[45+2*i]=uint8_t(uint16_t(s)>>8);}
 if(ok){ok=restore_factory_bytes(fs,"FACTORY/DEBUG/440Hz.wav",tone,bytes,report);}
 delete[] tone;
 // Retire exact factory aliases only after their replacements are ready.
 // The renamed FluidR3 GM patch keeps its GUID; user shelf copies are untouched.
 if(ok){const char* aliases[]={"BENCH.MGX","FACTORY/DEBUG/Benchmark.mgx","FACTORY/DEBUG/Test-Tone.mgx","FACTORY/KITS/Techno-01.mgx","FACTORY/PRESETS/LIBRARY/M1 Piano.mgxp","FACTORY/PRESETS/LIBRARY/DX7 Lately Bass.mgxp"};
  for(const char* path:aliases)if(fs.exists(path)){if(!fs.remove(path))return false;++report.retired;}
  for(unsigned kit=0;kit<kFactoryKitCount;++kit){char name[64],path[160];if(!copy_text(name,sizeof name,factory_kit_name(kit)))return false;unsigned n=0;while(name[n])++n;if(!copy_text(name+n,sizeof name-n,".mgx") || !join_path(path,sizeof path,"FACTORY/KITS",name))return false;if(fs.exists(path)){if(!fs.remove(path))return false;++report.retired;}}
 }
 // Retire the named old demo collections after operational content is ready.
 if(ok)for(const char* file:kRetiredFactoryDemos){char path[160];if(!join_path(path,sizeof path,"FACTORY/DEMOS",file))return false;if(fs.exists(path)){if(!fs.remove(path))return false;++report.retired;}}
 return ok;
}
}
