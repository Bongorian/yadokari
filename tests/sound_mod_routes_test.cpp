#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unistd.h>

using namespace groove;
constexpr Instrument wt=Instrument::Wavetable;
static SoundModRoute row(unsigned local,ModSource source){
 return {0,.95f,engine_target(wt,local),source,LayerMode::Replace,MacroCurve::Linear};
}
static MacroRoute project_row(const SoundModRoute& r){
 return {true,r.parameter,1,r.low,r.high,r.mode,r.curve,r.source};
}
struct Reading {unsigned note,oscillators,cost;float warp,pm;};
struct Trace {
 std::array<Reading,16> notes{};
 std::array<float,4*1024> pcm{};
};
// Four held notes fit the dry path. Only two can promote on this track.
static Trace held(unsigned local,bool portable){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 auto t=track_at(*p,0,0);t.instrument=wt;t.wavetable.values[WavetableWarpMode]=1;t.wavetable.values[WavetablePmRatio]=2;
 // An unrelated project route remains active in both sessions.
 p->macros[1].active=true;
 p->macros[1].routes[0]={true,engine_target(wt,7),1,.8f,.8f,LayerMode::Replace,MacroCurve::Linear,ModSource::Macro};
 auto route=row(local,ModSource::Pressure);
 if(portable){t.mod_routes.count=1;t.mod_routes.rows[0]=route;}
 else{p->macros[0].active=true;p->macros[0].routes[0]=project_row(route);}
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));Trace result;float scratch[256];e->render(scratch,0);
 for(unsigned i=0;i<4;++i){Command key{CommandType::MidiInput,0,uint8_t(119+i),0x90};key.scalar=100;assert(e->submit(key));e->render(scratch,128);}
 for(unsigned stage=0;stage<4;++stage){
  const uint8_t pressure=stage==1?127:0;
  assert(e->submit({CommandType::MidiInput,0,pressure,0xd0}));
  // MIDI pressure decays smoothly. A later exact-zero lock ends the wet
  // crossfade and returns the budget while all four keys remain held.
  if(stage==3)e->runtime_[0].active_locks[0]={route.parameter,0,true};
  for(unsigned b=0;b<4;++b)e->render(result.pcm.data()+1024*stage+256*b,128);
  unsigned count=0,wet=0,units=0;
  for(const auto& slot:e->poly_voices_)if(slot.owner==0){
   auto& v=slot.voice;auto map=v.wavetable.visual_phase_map(v.frequency);
   assert(!v.released && v.wt_reserved.oscillators==1 && count<4);
   result.notes[stage*4+count++]={v.note,v.wt_reserved.oscillators,v.wt_reserved.cost(),map.warp,map.pm};
   const float effective=local==WavetableWarpAmount?map.warp:map.pm;
   if(v.wt_reserved.extended){assert((stage==1 || stage==2) && effective>0 && effective<v.wavetable.values_[local]);++wet;}
   if(stage==1 && !v.wt_reserved.extended)assert(effective==0);
   units+=v.wt_reserved.cost();
  }
  assert(count==4 && wet==(stage==1 || stage==2?2u:0u) && units==(stage==1 || stage==2?26u:8u));
 }
 return result;
}
static void verify_held(){
 for(unsigned local:{unsigned(WavetableWarpAmount),unsigned(WavetablePmAmount)}){
  const auto a=held(local,false),b=held(local,true);
  assert(a.pcm==b.pcm);
  for(unsigned i=0;i<a.notes.size();++i){
   const auto& x=a.notes[i];const auto& y=b.notes[i];
   assert(x.note==y.note && x.oscillators==y.oscillators && x.cost==y.cost && x.warp==y.warp && x.pm==y.pm);
   if(i>=4 && i<8)std::printf("%s note %u: %u oscillator, %u units, effective warp %.6f pm %.6f (SOUND = MACR)\n",
     kWavetableParameters[local].name,x.note,x.oscillators,x.cost,x.warp,x.pm);
  }
 }
}
static void legacy_engine_refresh(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 auto t=track_at(*p,0,0);t.instrument=Instrument::FM;t.lfo_rate=8;
 SoundModRoute route{0,1,engine_target(Instrument::FM,0),ModSource::Lfo};
 auto trace=[&](){
  auto e=std::make_unique<Engine>();assert(e->stage_project(*p));
  std::array<float,12> controls{};float pcm[128];e->render(pcm,0);
  Command key{CommandType::MidiInput,0,62,0x90};key.scalar=100;assert(e->submit(key));
  for(float& control:controls){
   e->render(pcm,64);
   const auto* voice=&e->voices_[0];
   for(const auto& slot:e->poly_voices_)if(slot.owner==0)voice=&slot.voice;
   control=voice->targets[0];
  }
  return controls;
 };
 const auto dry=trace();t.mod_routes.count=1;t.mod_routes.rows[0]=route;const auto sound=trace();
 t.mod_routes.count=0;p->macros[0].active=true;p->macros[0].routes[0]=project_row(route);
 assert(trace()==sound && dry!=sound && sound.front()!=sound.back());
}
static void precedence(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=wt;
 auto r=row(WavetablePmAmount,ModSource::Velocity);t.mod_routes.count=1;t.mod_routes.rows[0]=r;
 auto& macro=p->macros[0];macro.active=true;auto& project=macro.routes[0];project=project_row(r);project.source=ModSource::Macro;project.low=project.high=.2f;
 ModRuntime m;m.velocity=1;
 auto read=[&]{return resolve_target_value(*p,t,0,r.parameter,0,nullptr,0,false,0,0,false,nullptr,0,nullptr,&m);};
 for(auto mode:{LayerMode::Multiply,LayerMode::Add,LayerMode::Replace}){
  project.mode=mode;auto out=read();assert(out.sources==(FromSoundRoute|FromMacro));
  assert(out.value==clamp_target(r.parameter,apply_layer(.95f,.2f,mode)));
 }
 project.tracks=2;assert(read().value==.95f && read().sources==FromSoundRoute);
}
static void codec(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 SoundState sound;sound.instrument=wt;auto& routes=sound.mod_routes;routes.count=4;
 for(unsigned i=0;i<4;++i)routes.rows[i]=row(WavetableWarpAmount,ModSource(i+1));
 ChunkBuffer payload;assert(encode_sound_routes(payload,engine_state_source(sound)) && payload.size==101);
 auto dest=std::make_unique<SoundState>();dest->instrument=wt;
 auto decode=[&](){ChunkSource src{payload.data,payload.size,0};return decode_sound_routes(src,engine_state_view(*dest));};
 assert(decode() && dest->mod_routes.count==4);
 const unsigned flags=25;const auto original=payload.data[flags];
 for(unsigned invalid:{0u,5u,15u}){payload.data[flags]=uint8_t(invalid<<4);assert(!decode());}
 payload.data[flags]=original|3;assert(!decode());payload.data[flags]=original;
 for(unsigned bad:{0u,5u}){payload.data[0]=bad;assert(!decode());}payload.data[0]=4;
 auto valid=routes.rows[0];auto bad=valid;
 for(auto id:{ParameterTarget(3),engine_target(Instrument::Formant,0),engine_target(wt,WavetableUnisonCount)}){
  bad.parameter=id;assert(!valid_sound_route(bad,wt));
 }
 for(auto mode:{LayerMode::Replace,LayerMode::Multiply,LayerMode::Add}){
  bad=valid;bad.mode=mode;bad.high=1000;assert(!valid_sound_route(bad,wt));
 }
 for(unsigned engine=0;engine<instrument_count();++engine){
  const auto& contract=engine_descriptor(engine)->state;unsigned common=0;
  for(unsigned i=0;i<contract.count;++i)common+=contract.chunks[i].fourcc==kChunkSmod;
  assert(common==1);
 }
 // Mutate a valid file (including its CRC) so the public reader must reject
 // the source itself, including in a nested leaf, and preserve the live sound.
 for(bool nested:{false,true}){
  auto t=track_at(*p,0,0);t.instrument=nested?Instrument::Layer:wt;
  if(nested){t.mod_routes.count=0;t.layer.count=1;t.layer.parts[0].sound=sound;}else t.mod_routes=routes;
  ChunkBuffer file;assert(encode_project_buffer(file,*p));
  unsigned found=0;for(unsigned i=0;i+113<=file.size;++i)if(!memcmp(file.data+i,"SMOD",4)){
   file.data[i+12+flags]=0;++found;
  }
  assert(found==1);ProjectCrc crc;for(unsigned i=4;i<file.size-4;++i)crc.add(file.data[i]);
  for(unsigned i=0;i<4;++i)file.data[file.size-4+i]=uint8_t(crc.value()>>(i*8));
  ChunkSource src{file.data,file.size,0};const char* why=nullptr;
  assert(!decode_project_impl(src,p.get(),nullptr,nullptr,&why) && why && strstr(why,"SMOD"));
  assert(t.instrument==(nested?Instrument::Layer:wt));
 }
}
static void verify_ui(ProjectState& p,Performance& ui){
 ui.set_mode(UiMode::Synth);ui.animate(1000);
 const auto* button=mk3::button("plugin");ui.control(button->report_byte,button->mask);ui.animate(1450);
 assert(ui.help_visible());std::array<bool,3> seen{};
 auto view=std::make_unique<UiRenderer>();
 view->text_observer_context=&seen;view->text_observer=[](const SoftwareCanvas::TextPlacement& t,void* context){
  auto& found=*static_cast<std::array<bool,3>*>(context);
  if(strstr(t.text,"SOUND MOD ROUTES 2"))found[0]=true;
  if(strstr(t.text,"LFO") && strstr(t.text,"WARP"))found[1]=true;
  if(strstr(t.text,"Envelope") && strstr(t.text,"PM"))found[2]=true;
 };
 view->draw(p,ui);assert(seen[0] && seen[1] && seen[2]);ui.control(button->report_byte,0);
 ui.set_mode(UiMode::Project);assert(ui.encoder(2) && ui.project_list_cursor()==2);
 auto& slot=p.sounds[ui.sound_slot()];const auto prior=std::make_unique<SoundState>(slot);
 snapshot_sound(slot,track_at(p,0,0));slot.mod_routes.count=4;
 slot.mod_routes.rows[2]=row(WavetableDetune,ModSource::Pressure);
 slot.mod_routes.rows[3]=row(WavetableSpread,ModSource::Velocity);
 unsigned mask=0;view->text_observer_context=&mask;
 view->text_observer=[](const SoftwareCanvas::TextPlacement& text,void* context){
  const char* labels[]={"SOUND MOD ROUTES 4","LFO","Envelope","Pressure","Velocity","WARP AMT","PM AMT","DETUNE","SPREAD"};
  if(text.side==1)for(unsigned i=0;i<9;++i)if(!strcmp(labels[i],text.text))*static_cast<unsigned*>(context)|=1u<<i;
 };
 view->invalidate();view->draw(p,ui);assert(mask==511);slot=*prior;
}
static void preset_and_leaf(){
 char dir[]="build/sound-mod-XXXXXX";assert(mkdtemp(dir) && chdir(dir)==0);
 DesktopFiles fs;DesktopSampleAssets lib;lib.files(&fs);
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();make_empty_project(*p);
 auto t=track_at(*p,0,0);t.instrument=wt;t.mod_routes.count=2;
 t.mod_routes.rows[0]=row(WavetableWarpAmount,ModSource::Lfo);t.mod_routes.rows[1]=row(WavetablePmAmount,ModSource::Envelope);
 auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);SessionFiles files(fs,lib,*e,*p,*ui);
 float pcm[256];assert(e->stage_project(*p));ui->project_loaded();e->render(pcm,0);ui->poll();
 assert(files.save_preset("Routed.mgxp"));e->render(pcm,0);ui->poll();
 assert(files.load_preset("Routed.mgxp"));e->render(pcm,0);ui->poll();
 assert(files.save_preset("Resaved.mgxp",0,false,true));assert(read_project_file(fs,"Resaved.mgxp",*out));
 const auto& loaded=out->tracks[0].mod_routes;assert(loaded.count==2);
 for(unsigned i=0;i<2;++i){auto& a=t.mod_routes.rows[i];auto& b=loaded.rows[i];assert(a.source==b.source && a.parameter==b.parameter && a.high==b.high);}
 verify_ui(*p,*ui);
 Command key{CommandType::MidiInput,0,76,0x90};key.scalar=100;
 auto sound=std::make_unique<SoundState>();snapshot_sound(*sound,t);
 auto block=[&](Engine& engine){assert(engine.submit(key));std::array<float,4096> audio{};for(unsigned i=0;i<16;++i)engine.render(audio.data()+i*256,128);return audio;};
 block(*e);bool audible=false;
 for(auto& v:e->poly_voices_)if(v.owner==0){auto map=v.voice.wavetable.visual_phase_map(v.voice.frequency);audible=map.warp>0 && map.pm>0;}
 assert(audible);
 make_empty_project(*out);auto track=track_at(*out,0,0);track.instrument=wt;
 out->macros[0].active=true;
 for(unsigned i=0;i<2;++i)out->macros[0].routes[i]=project_row(sound->mod_routes.rows[i]);
 // Use fresh engines for sample-clock and oscillator-phase aligned comparisons.
 auto render=[&](){auto fresh=std::make_unique<Engine>();assert(fresh->stage_project(*out));fresh->render(pcm,0);return block(*fresh);};
 const auto routed=render();out->macros[0].active=false;track.mod_routes=sound->mod_routes;assert(render()==routed);
 track.mod_routes.count=0;assert(render()!=routed);
 track.instrument=Instrument::Layer;track.layer.count=1;auto& child=track.layer.parts[0].sound;child=*sound;
 ChunkBuffer file;assert(encode_project_buffer(file,*out));ChunkSource src{file.data,file.size,0};assert(decode_project(src,*out));
 assert(child.mod_routes.count==2);const auto dormant=render();child.mod_routes.count=0;assert(render()==dormant);
 out->macros[0].active=true;assert(render()!=dormant);
}
int main(){codec();verify_held();legacy_engine_refresh();precedence();preset_and_leaf();std::puts("SMOD: preset re-save, direct LFO/ENV, dormant leaf, MACR modes, held WARP/PM effective maps and 8/26/8 admission units passed");}
