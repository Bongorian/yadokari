#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <vector>
#include <unistd.h>
using namespace groove;
static bool decode(const ChunkBuffer& b,ProjectState& p,PreservedStore* store=nullptr){ChunkSource r{b.data,b.size,0};return decode_project(r,p,store);}
static void preset(ProjectState& p){make_empty_project(p);p.content_kind=ContentKind::Preset;p.tracks[0].instrument=Instrument::Formant;p.tracks[0].preset_identity=builtin_factory_identity(7001);}
static void codec(){
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();preset(*p);
 ChunkBuffer unity;assert(encode_project_buffer(unity,*p) && decode(unity,*out));assert(out->tracks[0].output_trim==1);
 ChunkBuffer again;assert(encode_project_buffer(again,*out) && unity.size==again.size && !std::memcmp(unity.data,again.data,unity.size));
 for(float gain:{kPresetOutputTrimMin,.5f,1.f,2.f,kPresetOutputTrimMax}){
  p->tracks[0].output_trim=gain;ChunkBuffer b;assert(encode_project_buffer(b,*p) && decode(b,*out) && out->tracks[0].output_trim==gain && kFormatVersion==13);
  auto sound=std::make_unique<SoundState>();snapshot_sound(*sound,track_at(*p,0,0));assert(sound->output_trim==gain);
  auto song=std::make_unique<ProjectState>();make_empty_project(*song);song->sounds[255]=*sound;mount_sound(track_at(*song,0,7),*sound);song->tracks[7].preset_slot=255;
  song->tracks[7].level=.23f;ChunkBuffer bytes;assert(encode_project_buffer(bytes,*song) && decode(bytes,*out));assert(out->sounds[255].output_trim==gain && out->tracks[7].output_trim==gain && out->tracks[7].level==.23f);
 }
 // Trim changes participate in identity/revision conflict and lineage detection.
 auto a=std::make_unique<SoundState>(),b=std::make_unique<SoundState>();snapshot_sound(*a,track_at(*p,0,0));*b=*a;b->output_trim=.5f;assert(!same_preset_content(*a,p->assets,*b,p->assets));
 // Typed root, track and pool scopes reject duplicates, flags and invalid payloads.
 for(unsigned scope=0;scope<3;++scope)for(unsigned bad=0;bad<8;++bad){
  if(scope==0)preset(*p);else make_empty_project(*p);
  p->tracks[0].instrument=Instrument::Formant;p->tracks[0].output_trim=2;
  if(scope==2){snapshot_sound(*a,track_at(*p,0,0));p->sounds[0]=*a;}
  ChunkBuffer extra;uint32_t frame;assert(extra.begin_frame(kChunkPtrm,1,frame,bad==1?1:0));ChunkWriter<ChunkBuffer> w(extra);
  float value=bad==2?0:bad==3?4.01f:bad==4?std::numeric_limits<float>::infinity():bad==5?std::numeric_limits<float>::quiet_NaN():.5f;
  if(bad==7)assert(w.u16(0));else assert(w.f32(value));if(bad==6)assert(w.u8(0));assert(extra.end_frame(frame));
  PreservedStore foreign;
  if(scope==0)assert(foreign.add_chunk(kChunkPtrm,1,bad==1?1:0,extra.data+12,extra.size-12));
  else{auto& entry=scope==1?foreign.tracks[0]:foreign.sounds[0];assert(entry.auxiliary.set(extra.data,extra.size));}
  // bad0 is a valid but duplicate gain. Others are invalid even in isolation.
  if(bad!=0){p->tracks[0].output_trim=1;p->sounds[0].output_trim=1;}
  ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p,&foreign));out->tracks[0].output_trim=.75f;assert(!decode(bytes,*out) && out->tracks[0].output_trim==.75f);
 }
 preset(*p);p->tracks[0].output_trim=.1f;ChunkBuffer invalid;assert(!encode_project_buffer(invalid,*p));
 // Future revisions use the established opaque-preservation path and unity.
 preset(*p);PreservedStore foreign,loaded;const uint8_t future[]={7,4,9};assert(foreign.add_chunk(kChunkPtrm,2,0x42,future,sizeof future));ChunkBuffer old,newer;
 assert(encode_project_buffer(old,*p,&foreign) && decode(old,*out,&loaded) && out->tracks[0].output_trim==1 && encode_project_buffer(newer,*out,&loaded));assert(old.size==newer.size && !std::memcmp(old.data,newer.data,old.size));
 // A non-unity preceding destination cannot leak into a trim-less decode.
 out->tracks[0].output_trim=2;assert(decode(unity,*out) && out->tracks[0].output_trim==1);
}
static std::vector<float> render(float gain,bool layer,bool returns=false,bool frozen=false,bool kit=false,bool preview=false){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::Formant;t.level=.015f;t.output_bus=3;t.output_trim=gain;
 t.insert_fx[0]=11;t.insert_fx[1]=.65f;t.insert_fx[2]=.75f;t.insert_fx[3]=1; // Nonlinear insert must precede trim.
 if(layer){t.instrument=Instrument::Layer;t.layer.count=2;for(unsigned i=0;i<2;++i){auto& part=t.layer.parts[i];part.sound.instrument=Instrument::Formant;part.level=.25f;part.detune=i?7:-7;part.fx.present=true;part.fx.inserts[0]=11;part.fx.delay=.3f;part.fx.reverb=.4f;}}
 if(returns){p->effects.delay_mix=.3f;p->effects.reverb_mix=.2f;t.send_prefader=3;t.delay_send=.2f;t.reverb_send=.3f;}
 // A captured file is before the trim; frozen_chain bypasses common inserts.
 std::vector<uint8_t> wav(44+48000*4);WavView views[kSampleAssets]{};
 if(frozen || kit){assert(wav_header(wav.data(),48000));for(unsigned i=0;i<48000;++i){int16_t v=int16_t(1000*std::sin(i*.07));for(unsigned ch=0;ch<2;++ch){unsigned at=44+4*i+ch*2;wav[at]=uint8_t(v);wav[at+1]=uint8_t(uint16_t(v)>>8);}}assert(parse_wav(wav.data(),wav.size(),views[0]));if(frozen){t.frozen_asset=0;t.steps[0].enabled=true;}else{t.instrument=Instrument::DrumKit;auto& pad=t.kit.pads[0];pad.sample[0]=0;pad.delay_send=.3f;pad.reverb_send=.4f;pad.insert_fx[0]=11;}}
 // Exercise actual preset bytes -> mount -> DSP, not just an in-memory gain.
 auto file=std::make_unique<ProjectState>(),decoded=std::make_unique<ProjectState>();make_empty_project(*file);file->content_kind=kit?ContentKind::Kit:ContentKind::Preset;
 auto ft=track_at(*file,0,0);copy_engine_instance(ft,t);ft.preset_identity=builtin_factory_identity(7002);file->preset_fx.capture(t);ChunkBuffer bytes;assert(encode_project_buffer(bytes,*file) && decode(bytes,*decoded));copy_engine_instance(t,track_at(*decoded,0,0));
 stmlib::Random::Seed(p->random_seed);auto e=std::make_unique<Engine>();assert(e->stage_project(*p,views));float main[48],cue[48];e->render(main,0);
 if(preview){auto sound=std::make_unique<SoundState>();snapshot_sound(*sound,t);sound->output_trim=3;assert(e->preview_preset(0,*sound,views,p->assets,0,72));e->render(main,0);assert(e->preview_state()==145);assert(e->submit({CommandType::StopPreview,0,0,0}));e->render(main,0);assert(e->preview_state()==144);}
 // Existing Freeze retains the source voice velocity; initialize it as in
 // the live freeze workflow (fresh-load velocity initialization is separate).
 if(frozen){Command init{CommandType::Trigger,0,0,100};init.scalar=60;assert(e->submit(init));e->render(main,0);assert(e->submit({CommandType::Start,0,0,0}));}else{Command c{CommandType::Trigger,0,0,100};c.parameter=kit?4:1;c.scalar=kit?0:60;assert(e->submit(c));}
 std::vector<float> result;result.reserve(48000*4);
 for(unsigned at=0;at<48000;at+=24){e->render_ports(main,cue,nullptr,24);result.insert(result.end(),main,main+48);result.insert(result.end(),cue,cue+48);EngineEvent event;while(e->pop_event(event)){}MidiEvent midi;while(e->pop_midi(midi)){}ClockEvent clock;while(e->pop_clock(clock)){}AudioVisualFrame v;while(e->pop_visual(v)){}}
 return result;
}
static void audio(){
 for(unsigned mode=0;mode<5;++mode){auto base=render(1,mode==1 || mode==2,mode==2 || mode==4,mode==3,mode==4);double baseline=0;for(float f:base)baseline+=double(f)*f;assert(baseline>1e-9);
  for(float gain:{kPresetOutputTrimMin,.5f,2.f,kPresetOutputTrimMax}){auto changed=render(gain,mode==1 || mode==2,mode==2 || mode==4,mode==3,mode==4);double energy=0,error=0;for(unsigned i=0;i<base.size();++i){energy+=double(changed[i])*changed[i];error=std::fmax(error,std::fabs(double(changed[i])-double(base[i])*gain));}double ratio=std::sqrt(energy/baseline);std::printf("trim mode=%u gain=%.6f RMS-ratio=%.12f max-error=%.12g\n",mode,gain,ratio,error);// Shared returns have an absolute 1e-4 sleep/wake gate, so their
   // onset need not be sample-proportional at different gains.
   assert(std::fabs(ratio-gain)<gain*2e-5);if(mode!=2 && mode!=4)assert(error<2e-6);}
 }
 // A stopped PREVIEW restores the original root trim on the audio owner.
 auto reference=render(.5f,false);auto restored=render(.5f,false,false,false,false,true);assert(reference==restored);
}
static void files(){
 const auto cwd=std::filesystem::current_path();char dir[]="build/preset-output-XXXXXX";assert(mkdtemp(dir) && !chdir(dir));DesktopFiles fs;assert(fs.directory("PRESETS") && fs.directory("KITS") && fs.directory("PROJECTS"));DesktopSampleAssets library;library.files(&fs);
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Formant;p->tracks[0].output_trim=.5f;p->tracks[0].level=.21f;
 auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);auto files=std::make_unique<SessionFiles>(fs,library,*e,*p,*ui);float scratch[2]{};auto flush=[&]{e->render(scratch,0);ui->poll();};assert(e->stage_project(*p));ui->project_loaded();flush();
 for(bool fx:{false,true}){const char* path=fx?"PRESETS/Wet.mgxp":"PRESETS/Dry.mgxp";assert(files->save_preset(path,0,fx,true));flush();assert(read_project_file(fs,path,*out));assert(out->tracks[0].output_trim==.5f && out->preset_fx.present==fx && p->tracks[0].output_trim==.5f);}
 // New engine starts neutral; LOAD restores trim without changing track LEVEL.
 assert(ui->set_instrument(Instrument::FM));flush();assert(p->tracks[0].output_trim==1);
 assert(files->load_preset("PRESETS/Dry.mgxp"));flush();assert(p->tracks[0].output_trim==.5f && p->tracks[0].level==.21f);assert(ui->undo());flush();assert(p->tracks[0].output_trim==1);assert(ui->redo());flush();assert(p->tracks[0].output_trim==.5f);
 assert(files->save_current_preset());flush();assert(read_project_file(fs,"PRESETS/Dry.mgxp",*out));assert(out->tracks[0].output_trim==.5f);
 // Pool attachment, preview restore and grouped replacement share root gain.
 auto sound=std::make_unique<SoundState>();snapshot_sound(*sound,track_at(*p,0,0));sound->output_trim=2;WavView views[kSampleAssets]{};assert(e->preview_preset(0,*sound,views,p->assets,0,71));flush();assert(e->preview_state()==143);assert(e->submit({CommandType::StopPreview,0,0,0}));flush();assert(e->preview_state()==142);
 std::filesystem::current_path(cwd);std::filesystem::remove_all(dir);
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);codec();std::puts("codec passed");audio();std::puts("audio passed");files();std::puts("Preset root trim: codec scopes/validation, PCM ratios post-insert + Layer returns + frozen playback, save/overwrite/Undo and preview restoration passed");}
