#include "engine.h"
#include "project_codec.h"
#include "note_transform.h"
#include "ui_renderer.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace groove;

static void note(Engine& engine,unsigned track,unsigned pitch,bool on){
 Command input{CommandType::MidiInput,uint8_t(track),uint8_t(pitch),uint16_t(on?0x90:0x80)};
 input.scalar=on?100:0;
 assert(engine.submit(input));
}

static float render(Engine& engine,unsigned frames,AudioVisualFrame& visual){
 assert(frames%24==0);
 float peak=0;
 for(unsigned i=0;i<frames;i+=24){
  float pcm[48];
  engine.render(pcm,24);
  for(float sample:pcm){assert(std::isfinite(sample));peak=std::fmax(peak,std::fabs(sample));}
  while(engine.pop_visual(visual)){}
 }
 return peak;
}

static uint64_t cutaway_pixels(const UiRenderer& display){
 uint64_t signature=1469598103934665603ull;
 for(unsigned y=103;y<200;++y)for(unsigned x=24;x<281;++x){
  signature=(signature^display.pixels[1][y*480+x])*1099511628211ull;
 }
 return signature;
}

int main(){
 auto project=std::make_unique<ProjectState>();
 make_empty_project(*project);
 project->tracks[0].instrument=project->tracks[1].instrument=Instrument::Chimera;
 project->tracks[0].engine_parameters[0]=.61f;
 project->tracks[0].engine_parameters[4]=.67f;
 ChunkBuffer encoded;
 assert(encode_project_buffer(encoded,*project));
 auto restored=std::make_unique<ProjectState>();
 ChunkSource input{encoded.data,encoded.size,0};
 assert(decode_project(input,*restored));
 assert(restored->tracks[0].instrument==Instrument::Chimera);
 assert(restored->tracks[0].engine_parameters[0]==.61f && restored->tracks[0].engine_parameters[4]==.67f);
 restored->ui.motion=0;

 auto engine=std::make_unique<Engine>();
 assert(engine->stage_project(*restored));
 AudioVisualFrame visual{};
 engine->visual_focus(0);
 assert(render(*engine,4800,visual)==0);
 assert(visual.chimera_available && visual.chimera.gate==0);
 const auto rest=visual.chimera;
 note(*engine,0,57,true);
 assert(render(*engine,9600,visual)>.001f);
 assert(visual.chimera.gate>.2f && !visual.invalid_audio);
 assert(std::fabs(visual.chimera.tongue[12].x-rest.tongue[12].x)>.01f);

 auto ui=std::make_unique<Performance>(*engine,*restored);
 float pcm[48];
 for(unsigned i=0;i<120;++i){engine->render(pcm,24);ui->poll();}
 assert(ui->visual_fresh() && ui->visual().chimera.gate>.2f);
 auto display=std::make_unique<UiRenderer>();
 bool found_cutaway=false;
 display->text_observer=[](const SoftwareCanvas::TextPlacement& text,void* context){
  if(text.side==1 && std::strcmp(text.text,"VOCAL TRACT")==0)*static_cast<bool*>(context)=true;
 };
 display->text_observer_context=&found_cutaway;
 constexpr UiMode views[]={UiMode::Synth,UiMode::Pad,UiMode::Keyboard,UiMode::Chord,UiMode::Step};
 uint64_t sounding[5]{};
 auto draw_cutaway=[&](UiMode mode){
  ui->set_mode(mode);
  found_cutaway=false;
  display->draw(*restored,*ui);
  assert(found_cutaway);
  return cutaway_pixels(*display);
 };
 for(unsigned i=0;i<5;++i)sounding[i]=draw_cutaway(views[i]);
 ui->set_mode(UiMode::Synth);
 assert(ui->knob_parameter(0)==engine_target(Instrument::Chimera,0));

 note(*engine,0,57,false);
 for(unsigned i=0;i<300;++i){engine->render(pcm,24);ui->poll();}
 assert(ui->visual_fresh() && ui->visual().chimera.gate==0);
 for(unsigned i=0;i<5;++i)assert(draw_cutaway(views[i])!=sounding[i]);

 note(*engine,0,57,true);
 assert(render(*engine,4800,visual)>.001f);
 engine->visual_focus(1);
 note(*engine,1,67,true);
 assert(render(*engine,4800,visual)>.001f);
 assert(visual.chimera_busy && visual.chimera.gate==0);
 note(*engine,0,57,false);
 render(*engine,4800,visual);
 assert(!visual.chimera_busy && render(*engine,4800,visual)==0);

 note(*engine,1,67,true);
 assert(render(*engine,4800,visual)>.001f && visual.chimera.gate>.2f);
 assert(engine->submit({CommandType::Panic,0,0,0}));
 assert(render(*engine,4800,visual)==0 && visual.chimera.gate==0);

 // The physical pad reports enter the same transport path used by the MK3.
 assert(ui->select_track(0));
 for(UiMode mode:{UiMode::Pad,UiMode::Keyboard,UiMode::Chord}){
  ui->set_mode(mode);
  assert(ui->pad(12,0x10,3600));
  assert(render(*engine,4800,visual)>.001f);
  assert(ui->pad(12,0x20,0));
  render(*engine,4800,visual);ui->poll();
 }
 auto last_note=[&](){
  EngineEvent event{};
  unsigned last=255;
  while(engine->pop_event(event))if(event.type==EngineEventType::Note && event.track==0)last=event.value;
  return last;
 };
 note(*engine,0,60,true);render(*engine,240,visual);assert(last_note()==60);
 note(*engine,0,67,true);render(*engine,240,visual);assert(last_note()==67);
 note(*engine,0,67,false);render(*engine,240,visual);assert(last_note()==60);
 note(*engine,0,60,false);render(*engine,4800,visual);last_note();
 restored->patterns[0].tracks[0].steps[0].enabled=true;
 restored->patterns[0].tracks[0].steps[0].chord[0]=64;
 restored->patterns[0].tracks[0].steps[0].chord[1]=67;
 assert(engine->stage_project(*restored));render(*engine,24,visual);
 assert(engine->submit({CommandType::Start,0,0,0}));
 assert(render(*engine,4800,visual)>.001f && last_note()==67);
 assert(engine->submit({CommandType::Stop,0,0,0}));
 render(*engine,24,visual);

 // Every engine-specific detail has a physical knob; the macro ids stay first.
 ui->set_mode(UiMode::Synth);
 assert(ui->soft_key(2));
 for(unsigned i=0;i<8;++i)assert(ui->knob_parameter(i)==engine_target(Instrument::Chimera,ChimeraTipX+i));
 assert(ui->soft_key(2));
 for(unsigned i=0;i<6;++i)assert(ui->knob_parameter(i)==engine_target(Instrument::Chimera,ChimeraVelum+i));
 assert(ui->soft_key(3));
 for(unsigned i=0;i<6;++i)assert(ui->knob_parameter(i)==engine_target(Instrument::Chimera,ChimeraBody+i));
 assert(ui->knob_parameter(6)==engine_target(Instrument::Chimera,ChimeraProgram));
 assert(ui->knob_parameter(7)==engine_target(Instrument::Chimera,ChimeraSpecies));
 auto& details=restored->tracks[0].chimera;
 for(unsigned i=0;i<kChimeraDetailCount;++i)details.values[i]=kChimeraDetails[i].maximum;
 encoded.clear();
 assert(encode_project_buffer(encoded,*restored));
 auto details_restored=std::make_unique<ProjectState>();
 ChunkSource detail_source{encoded.data,encoded.size,0};
 assert(decode_project(detail_source,*details_restored));
 assert(std::memcmp(&details,&details_restored->tracks[0].chimera,sizeof details)==0);
 SoundState captured;snapshot_sound(captured,track_at(static_cast<const ProjectState&>(*details_restored),0,0));
 assert(std::memcmp(&details,&captured.chimera,sizeof details)==0);
 details=ChimeraState{};

 // Beatbox uses the same dispatch for held pads and steps. A short tap still
 // plays the entire scored transient.
 details.values[ChimeraProgram-kChimeraMacroCount]=ChimeraBeatbox;
 assert(engine->stage_project(*restored));render(*engine,24,visual);
 ui->set_mode(UiMode::Pad);
 assert(ui->pad(12,0x10,3600));assert(ui->pad(12,0x20,0));
 assert(render(*engine,4800,visual)>.001f);
 assert(render(*engine,14400,visual)<.8f);
 assert(engine->submit({CommandType::Start,0,0,0}));
 assert(render(*engine,4800,visual)>.001f);
 assert(engine->submit({CommandType::Stop,0,0,0}));render(*engine,24,visual);

 const char ipa[]="60\ta\n62\tʃi\n1/2\tka\n2/1\tka\n";
 details.values[ChimeraProgram-kChimeraMacroCount]=ChimeraSing;
 restored->patterns[0].tracks[0].length=2;
 restored->patterns[0].tracks[0].steps[0]=StepState{};
 restored->patterns[0].tracks[0].steps[1].enabled=true;
 restored->patterns[0].tracks[0].steps[1].note=60;
 assert(engine->stage_project(*restored));render(*engine,24,visual);
 assert(!engine->stage_chimera_speech(0,"60\t%\n",5));
 assert(engine->stage_chimera_speech(0,ipa,sizeof ipa-1));
 assert(!engine->stage_chimera_speech(0,ipa,sizeof ipa-1)); // one slot in flight
 float single[2]{};
 auto sing_hit=[&](unsigned maximum,bool check_pickup){
  bool pickup=false;
  for(unsigned i=0;i<maximum;++i){
   engine->render(single,1);
   float age=engine->chimera_sing_time();
   pickup|=age>=0 && age<.055f;
   EngineEvent event{};
   while(engine->pop_event(event))if(event.type==EngineEventType::Note && event.track==0 &&
       event.generation==engine->staged_generation()){
    if(event.value!=60 || std::fabs(age-.060f)>=.001f)
     std::fprintf(stderr,"sing note %u at sample %llu, score time %.7f, pickup %d, frame %u\n",
                  event.value,static_cast<unsigned long long>(event.sample),age,pickup,i);
    assert(event.value==60 && std::fabs(age-.060f)<.001f);
    assert(!check_pickup || pickup);
    return;
   }
  }
  assert(false && "Missing sung step");
 };
 assert(engine->submit({CommandType::Start,0,0,0}));
 render(*engine,3360,visual); // Change tempo after the pickup has started.
 assert(engine->chimera_sing_time()>=0 && engine->chimera_sing_time()<.060f);
 assert(engine->submit({CommandType::SetTempo,0,0,150}));
 sing_hit(5000,true);
 sing_hit(13000,true); // pattern loop
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(single,0);
 assert(engine->submit({CommandType::SeekSong,0,0,1}));
 assert(engine->submit({CommandType::Continue,0,2,0}));
 sing_hit(1000,false); // seek starts directly at the vowel
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(single,0);

 // An arrangement seek chooses the saved section and its step assignment.
 restored->song.enabled=true;restored->song.loop=true;restored->song.length=2;
 restored->song.sections[0].ticks=kSongTicksPerBeat/4;
 restored->song.sections[0].playing_mask=1;
 restored->song.sections[1].ticks=kSongTicksPerBeat/2;
 restored->song.sections[1].playing_mask=1;
 restored->song.sections[1].patterns[0]=1;
 restored->patterns[1].tracks[0].steps[0].enabled=true;
 restored->patterns[1].tracks[0].steps[0].note=60;
 restored->chimera_ipa[0].size=sizeof ipa-1;
 std::memcpy(restored->chimera_ipa[0].data,ipa,sizeof ipa-1);
 assert(engine->stage_project(*restored));engine->render(single,0);
 assert(engine->submit({CommandType::SeekSong,0,0,1}));
 assert(engine->submit({CommandType::Continue,0,2,0}));
 engine->render(single,1);
 assert(std::fabs(engine->chimera_sing_time()-.060f)<.001f);
 EngineEvent seek_event{};bool correct_section=false;
 while(engine->pop_event(seek_event))if(seek_event.type==EngineEventType::ClipLaunched &&
       seek_event.track==0 && seek_event.value==1)correct_section=true;
 assert(correct_section);
 assert(engine->submit({CommandType::Stop,0,0,0}));engine->render(single,0);
 restored->song=SongState{};

 details.values[ChimeraProgram-kChimeraMacroCount]=ChimeraSpeak;
 restored->chimera_ipa[0].size=sizeof ipa-1;
 std::memcpy(restored->chimera_ipa[0].data,ipa,sizeof ipa-1);
 encoded.clear();
 assert(encode_project_buffer(encoded,*restored));
 ChunkSource ipa_source{encoded.data,encoded.size,0};
 assert(decode_project(ipa_source,*details_restored));
 assert(details_restored->chimera_ipa[0].size==sizeof ipa-1);
 assert(engine->stage_project(*details_restored));render(*engine,24,visual);
 note(*engine,0,62,true);note(*engine,0,62,false);
 assert(render(*engine,8400,visual)>.001f && !visual.invalid_audio);
 assert(render(*engine,24000,visual)<.8f);

 // Sweep the full physical control extremes with the firmware's allocator.
 // A port that tries to grow any workspace during rendering fails here.
 ChimeraVoice direct;
 direct.init();assert(direct.ready());direct.trigger(0);
 for(unsigned shape=0;shape<32;++shape){
  float controls[5];for(unsigned p=0;p<5;++p)controls[p]=float((shape>>p)&1);
  for(unsigned i=0;i<2048;++i)assert(std::isfinite(direct.next(196,.9f,true,controls)));
 }
 float neutral[]={.5f,.5f,.5f,.5f,.65f};
 ChimeraState state;
 direct.stop();
 float previous_lip=100;
 for(unsigned body=0;body<3;++body){
  ChimeraGeometry idle{},active{};
  direct.geometry(0,idle,body);
  assert(idle.gate==0 && idle.outer[31].x<previous_lip);
  previous_lip=idle.outer[31].x;
  state.values[0]=float(body);
  state.values[ChimeraTipY-kChimeraMacroCount]=1;
  state.values[ChimeraVelum-kChimeraMacroCount]=1;
  direct.trigger(0);
  if(body)assert(direct.next(220,1,true,neutral,&state)==0);
  float peak=0;
  for(unsigned frame=0;frame<7200;++frame)peak=std::fmax(peak,std::fabs(direct.next(220,1,true,neutral,&state)));
  assert(peak>.001f);
  direct.geometry(0,active);
  assert(active.gate>.3f && std::fabs(active.velum[16].y-idle.velum[16].y)>.01f);
  direct.stop();
 }
 state.values[ChimeraProgram-kChimeraMacroCount]=ChimeraBestiary;
 float trachea=0;
 for(unsigned species=0;species<4;++species){
  ChimeraGeometry idle{};
  direct.geometry(0,idle,species+3);
  assert(idle.model==species+3 && idle.animal[0].length>trachea);
  trachea=idle.animal[0].length;
  state.values[ChimeraSpecies-kChimeraMacroCount]=float(species);
  direct.trigger(0);
  float peak=0;
  for(unsigned frame=0;frame<9600;++frame){
   float sample=direct.next(261.6256f,1,true,neutral,&state);
   assert(std::isfinite(sample));peak=std::fmax(peak,std::fabs(sample));
  }
  assert(peak>.001f);
  direct.stop();
 }
 std::puts("Chimera: held notes, step chords, detail round trips, body changes, Beatbox, IPA, Sing alignment and four species passed");
}
