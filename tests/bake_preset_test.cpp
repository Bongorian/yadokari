#include "preset_functional_check.h"
#include "soundfont_fixture.h"
#include "future_factory_presets.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cstring>
#include <unistd.h>
using namespace groove;
using CleanFiles=DesktopFiles;
int main(){
 char root[]="/tmp/mk3-bake-XXXXXX";assert(mkdtemp(root));auto cwd=std::filesystem::current_path();assert(chdir(root)==0);
 CleanFiles fs;DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Wavetable;p->patterns[0].tracks[0].steps[0].enabled=true;
 auto engine=std::make_unique<Engine>(48000);auto ui=std::make_unique<Performance>(*engine,*p);SessionFiles files(fs,library,*engine,*p,*ui);float out[128]{};
 assert(engine->stage_project(*p));ui->project_loaded();auto run=[&](unsigned frames){while(frames){unsigned n=frames>64?64:frames;engine->render(out,n);ui->poll();files.service();frames-=n;}};run(1);
 assert(ui->begin_bake());run(1);assert(ui->bake_current() && p->routing.capture_bus==3);assert(ui->play());run(1);ui->set_shift(true);assert(ui->soft_key(5));ui->set_shift(false);
 RecordingResult r;bool done=false;for(unsigned i=0;i<600000;i+=64){run(64);if(files.take_recording_result(r)){done=true;break;}}
 assert(done && r.saved && r.assigned && r.verified);run(1);assert(ui->bake_stage_==Performance::BakeStage::Trim && ui->bake_current());assert(ui->stop());run(1);
 assert(ui->soft_key(7));run(1);assert(ui->sampler_page()==4);assert(ui->knob(7,1000));run(1);auto source=track_at(*p,0,0);assert(source.sample_edit.zone_count==1 && source.sample_edit.zones[0].root==61);assert(ui->knob(5,1000));run(1);
 assert(ui->soft_key(7));copy_text(ui->browser().edit.name,sizeof ui->browser().edit.name,"My Glass");assert(ui->soft_key(0));run(1);assert(fs.exists("PRESETS/My Glass.mgxp") && ui->bake_stage_==Performance::BakeStage::Collect);
 assert(ui->mode()==UiMode::Project && ui->soft_key(4));run(1);assert(ui->bake_stage_==Performance::BakeStage::Done);const char* collected="PRESETS/MY GLASS-EXPORT/PRESETS/preset.mgxp";assert(fs.exists(collected));
 auto saved=std::make_unique<ProjectState>();assert(read_project_file(fs,collected,*saved) && saved->content_kind==ContentKind::Preset && project_paths(*saved,collected,true));unsigned slot=saved->tracks[0].sample_slot;assert(saved->assets[slot].metadata.content_crc && fs.exists(saved->assets[slot].path));assert(saved->tracks[0].sample_edit.zones[0].root==61);
 assert(ui->soft_key(7));assert(ui->set_instrument(Instrument::VirtualAnalog));run(1);assert(files.load_preset(collected));run(1);assert(p->tracks[0].instrument==Instrument::Sampler && p->tracks[0].sample_edit.zones[0].root==61);
 assert(fs.directory("GROOVE"));assert(fs.directory("FACTORY"));assert(fs.directory("FACTORY/DEBUG"));assert(fs.write("FACTORY/DEBUG/Sine.sf2",0,kSoundFontFixture,sizeof kSoundFontFixture,true));{auto fixture=std::make_unique<ProjectState>();make_empty_project(*fixture);fixture->content_kind=ContentKind::Preset;fixture->tracks[0].preset_identity=builtin_factory_identity(1);fixture->tracks[0].instrument=Instrument::SoundFont;copy_text(fixture->assets[0].path,96,"Sine.sf2");assert(file_checksum(fs,"FACTORY/DEBUG/Sine.sf2",fixture->assets[0].metadata.content_crc));assert(save_recoverable(fs,"FACTORY/DEBUG/SF2-Sine.mgxp",*fixture));}{auto tone=std::make_unique<ProjectState>(*saved);assert(project_paths(*tone,"FACTORY/DEBUG/Test-Tone.mgxp",false));assert(save_recoverable(fs,"FACTORY/DEBUG/Test-Tone.mgxp",*tone));}assert(copy_file(fs,saved->assets[slot].path,"FACTORY/DEBUG/440Hz.wav"));auto before_check=std::make_unique<ProjectState>(*p);PresetFunctionalCheck check(files,fs,*engine,*p,*ui);assert(check.start());for(unsigned i=0;i<6000 && check.busy();++i){run(64);check.service();}unsigned passed=0;bool checked=false;bool ready=check.result(passed,checked);std::fprintf(stderr,"Preset check: ready=%u ok=%u passed=%u busy=%u progress=%u status=%s\n",unsigned(ready),unsigned(checked),passed,unsigned(check.busy()),check.progress(),ui->browser().status);assert(ready && checked && passed==PresetFunctionalCheck::kCases);assert(!__builtin_memcmp(p->sounds,before_check->sounds,sizeof p->sounds) && !__builtin_memcmp(p->assets,before_check->assets,sizeof p->assets));
 // The hardware probe must also qualify a synth-only song with no assets.
 auto populated=std::make_unique<ProjectState>(*p);auto bindings=std::make_unique<WavView[]>(kSampleAssets);files.copy_bindings(bindings.get());auto empty=std::make_unique<ProjectState>();make_empty_project(*empty);empty->tracks[0].instrument=Instrument::FM;
 assert(engine->stage_project(*empty));*p=*empty;files.set_bindings(nullptr);ui->project_loaded();run(1);
 PresetFunctionalCheck no_assets(files,fs,*engine,*p,*ui);assert(no_assets.start());for(unsigned i=0;i<6000 && no_assets.busy();++i){run(64);no_assets.service();}passed=0;checked=false;ready=no_assets.result(passed,checked);std::fprintf(stderr,"No-assets preset check: ready=%u ok=%u passed=%u\n",unsigned(ready),unsigned(checked),passed);assert(ready && checked && passed==PresetFunctionalCheck::kCases);
 assert(p->tracks[0].instrument==Instrument::FM);for(const auto& a:p->assets)assert(!a.path[0]);for(const auto& sound:p->sounds)assert(sound.instrument==Instrument::Empty);
 assert(engine->stage_project(*populated,bindings.get()));*p=*populated;files.set_bindings(bindings.get());ui->project_loaded();run(1);
 assert(fs.remove(saved->assets[slot].path));assert(files.load_preset(collected));run(1);auto erase_material=[&](const char* path,const FileEntry&){uint32_t crc=0;if(file_checksum(fs,path,crc) && crc==saved->assets[slot].metadata.content_crc)assert(fs.remove(path));};ShelfScan removed;walk_content_shelf(fs,"",erase_material,removed);assert(ui->set_instrument(Instrument::VirtualAnalog));run(1);assert(!files.load_preset(collected));assert(p->tracks[0].instrument==Instrument::VirtualAnalog);
 assert(ui->set_instrument(Instrument::DrumKit));run(1);assert(!ui->begin_bake());assert(ui->set_instrument(Instrument::VirtualAnalog));run(1);
 assert(ui->begin_bake());assert(ui->select_track(1));assert(!ui->bake_current());assert(ui->select_track(0));assert(!ui->bake_current());assert(ui->begin_bake());ui->bake_recorded(0,false);assert(ui->bake_stage_==Performance::BakeStage::Capture);ui->project_loaded();assert(!ui->bake_current());
 for(unsigned i=0;i<8;++i){assert(make_future_factory_preset(*saved,i));assert(valid_project(*saved));for(const auto& sound:saved->sounds)assert(sound.instrument==Instrument::Empty);}
 assert(chdir(cwd.c_str())==0);std::filesystem::remove_all(root);puts("Bake: real resample/assign, trim/root/zone, named save, preset-only collect, remount, missing CRC asset refusal, target invalidation and empty factory pools passed");
}
