// Exercise the same knobs, engine command queue and stereo output as the synth editor.
#include "performance.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace groove;

static unsigned select_control(Performance& ui,unsigned local){
 ui.set_mode(UiMode::Synth);
 if(local<ChimeraBody)return local;
 if(local<=ChimeraBreathiness || (local>=ChimeraProgram && local<=ChimeraVoiceMode)){
  assert(ui.soft_key(3));
  if(local<=ChimeraBreathiness)return local-ChimeraBody;
  if(local==ChimeraProgram)return 6;
  ui.set_shift(local==ChimeraVoiceMode);return 7;
 }
 assert(ui.soft_key(2));
 if(local<ChimeraVelum)return local-ChimeraTipX;
 assert(ui.soft_key(2));
 if(local<=ChimeraDorsum)return local-ChimeraVelum;
 if(local<=ChimeraSpread)return 6+local-ChimeraUnison;
 ui.set_shift(true);return 4+local-ChimeraSceneSpeak;
}

static void note(Engine& e,unsigned pitch){
 Command c{CommandType::MidiInput,0,uint8_t(pitch),0x90};c.scalar=100;assert(e.submit(c));
}
static void render(Engine& e,unsigned blocks,std::vector<float>* output=nullptr,bool right_only=false){
 float pcm[48];
 for(unsigned b=0;b<blocks;++b){
  e.render(pcm,24);
  for(unsigned i=0;i<48;++i){
   assert(std::isfinite(pcm[i]));
   if(output && (!right_only || i%2))output->push_back(pcm[i]);
  }
 }
}

struct Result {std::vector<float> samples;float setting=0;unsigned tracts=0;bool follower=false;const char* label=nullptr;};
enum class StepSource { Live, Lock, Preset };
static Result take(unsigned local,bool edit,int creature=-1,bool condition=true,StepSource source=StepSource::Live){
 auto song=std::make_unique<ProjectState>();make_empty_project(*song);
 auto& track=song->tracks[0];track.instrument=Instrument::Chimera;
 if(source!=StepSource::Live){
  song->tempo=60;song->clips[0]={0,true};
  auto& step=song->patterns[0].tracks[0].steps[0];step.enabled=true;step.gate=240;
  if(source==StepSource::Lock)step.locks[0]={engine_target(Instrument::Chimera,local),.18f,true};
  else{auto& preset=song->sounds[0];preset.instrument=Instrument::Chimera;preset.parameters[local]=.18f;step.sound=0;}
 }
 auto detail=[&](unsigned key)->float&{return track.chimera.values[key-kChimeraMacroCount];};
 // The lateral channel only opens around an alveolar tongue-tip closure.
 if(condition && local==ChimeraLateral){
  track.engine_parameters[ChimeraJaw]=.2f;detail(ChimeraTipX)=.70f;
  detail(ChimeraTipY)=2.30f;detail(ChimeraTipCurl)=.6f;
 }
 if(condition && local==ChimeraVibratoRate)detail(ChimeraVibratoDepth)=75;
 if(local==ChimeraGlide)detail(ChimeraVoiceMode)=ChimeraLegato;
 if(condition && local==ChimeraSpread)detail(ChimeraUnison)=2;
 if(local==ChimeraSpecies || creature>=0)detail(ChimeraProgram)=ChimeraBestiary;
 if(creature>=0)detail(ChimeraSpecies)=float(creature);
 if(local==ChimeraSceneSpeak || local==ChimeraTalkOutput){
  constexpr char phrase[]="60\ta i\n";std::memcpy(track.chimera_phrase.data,phrase,sizeof phrase-1);
  track.chimera_phrase.size=sizeof phrase-1;
 }
 if(local==ChimeraTalkOutput){
  track.pan=-1;auto& target=song->tracks[1];target.instrument=Instrument::VirtualAnalog;target.pan=1;
  target.insert_fx[0]=kTalkFx;target.insert_fx[1]=1;target.insert_fx[2]=1;target.insert_fx[3]=1;
 }
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*song));render(*engine,1);
 auto ui=std::make_unique<Performance>(*engine,*song);
 const unsigned knob=select_control(*ui,local);const auto id=engine_target(Instrument::Chimera,local);
 assert(ui->knob_parameter(knob)==id);
 if(creature>=0){
  static const char* first[]={"DRIVE","PULSE DEPTH","CHAOS","TRUMPET"};
  assert(std::strcmp(ui->knob_label(knob),target_descriptor(id)->name)!=0);
  if(local==0)assert(std::strcmp(ui->knob_label(knob),first[creature])==0);
  if(creature==3 && local==2)assert(std::strcmp(ui->knob_label(knob),"TRUNK FINE")==0);
 }
 bool setup=chimera_sound_attribute(local);
 if(source!=StepSource::Live)assert(engine->submit({CommandType::Start,0,0,0}));
 else if(!setup)note(*engine,60);
 render(*engine,100);
 Result result;result.label=ui->knob_label(knob);const float prior=ui->knob_value(knob);
 if(edit){
  int ticks=24;
  if(local==ChimeraProgram || local==ChimeraVoiceMode || local==ChimeraSceneSpeak || local==ChimeraBody || local==ChimeraSpecies)ticks=1;
  if(local==ChimeraTalkOutput)ticks=2;
  if(local==ChimeraUnison)ticks=3;
  const auto* d=target_descriptor(id);if(prior>=d->maximum)ticks=-ticks;
  assert(ui->knob(knob,int16_t(ticks*1000)));
 }
 result.setting=ui->knob_value(knob);
 assert(!edit || result.setting!=prior);
 assert(parameter_base(*song,track_at(*song,0,0),id)==result.setting);
 ui->set_shift(false);
 if(local==ChimeraSceneSpeak){
  assert(engine->submit({CommandType::LaunchScene,0,0,0}));
 }else if(setup)note(*engine,60);
 if(local==ChimeraVoiceMode || local==ChimeraGlide){render(*engine,25);note(*engine,67);}
 if(local==ChimeraTalkOutput){
  Command carrier{CommandType::Trigger,1,0,100};carrier.parameter=1;carrier.scalar=48;assert(engine->submit(carrier));
 }
 render(*engine,local==ChimeraVibratoRate || local==ChimeraVibratoDepth?600:300,&result.samples,local==ChimeraTalkOutput);
 result.tracts=engine->active_tracts();result.follower=engine->talk_following(1);
 return result;
}

static bool compare(unsigned local,int creature=-1){
 const auto base=take(local,false,creature),repeat=take(local,false,creature),edited=take(local,true,creature);
 assert(base.samples==repeat.samples);
 double sum=0;unsigned changed=0;
 for(size_t i=0;i<base.samples.size();++i){
  double diff=double(edited.samples[i])-base.samples[i];sum+=diff*diff;
  if(std::lround(32767*edited.samples[i])!=std::lround(32767*base.samples[i]))++changed;
 }
 std::printf("%u\t%s\t%s\t%g\t%g\t%.9g\t%u\t%u\t%u\t%u\t%u\n",local,base.label,
  creature<0?"STANDARD":kChimeraSpecies[creature],base.setting,edited.setting,std::sqrt(sum/base.samples.size()),
  changed,base.tracts,edited.tracts,base.follower,edited.follower);
 std::fflush(stdout);
 if(local==ChimeraVoiceMode)assert(base.tracts==2 && edited.tracts==1);
 if(local==ChimeraUnison)assert(base.tracts==1 && edited.tracts==4);
 if(local==ChimeraTalkOutput)assert(!base.follower && edited.follower);
 // Reject a rounding-only difference or a single mismatching sample.
 return std::sqrt(sum/base.samples.size())>1.0/32767 && changed>base.samples.size()/100;
}

// Compare the render model published by the audio owner with the variant
// charged by the shared voice pool and recorded in DSPTEST.
static uint32_t ticks(){static uint32_t t=0;return ++t;}
static void model_accounting(){
 auto song=std::make_unique<ProjectState>();make_empty_project(*song);
 auto& track=song->tracks[0];track.instrument=Instrument::Chimera;
 track.chimera.values[ChimeraProgram-kChimeraMacroCount]=ChimeraBestiary;
 track.chimera.values[ChimeraSceneSpeak-kChimeraMacroCount]=1;
 constexpr char text[]="60\taiuaiu\n";std::memcpy(track.chimera_phrase.data,text,sizeof text-1);
 track.chimera_phrase.size=sizeof text-1;
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*song));render(*engine,1);
 auto ui=std::make_unique<Performance>(*engine,*song);engine->visual_focus(0);
 auto check=[&](unsigned model,const char* name){
  auto& profile=engine->dsp_profile();assert(profile.begin(ticks));profile.begin_measurement();
  AudioVisualFrame frame{};float pcm[48];bool got=false;
  for(unsigned b=0;b<220;++b){
   engine->render(pcm,24);while(engine->pop_visual(frame))got=true;
  }
  profile.stop();engine->render(pcm,24);
  assert(got && frame.chimera_available && engine->active_tracts() && frame.chimera.gate>0);
  unsigned charged=7;
  for(unsigned m=0;m<7;++m)if(profile.counter(DspProfile::voice(unsigned(Instrument::Chimera),m)).frames){
   assert(charged==7);charged=m;
  }
  std::fprintf(stderr,"%s: actual model %u, charged model %u, expected %u\n",name,frame.chimera.model,charged,model);
  return frame.chimera.model==model && charged==model;
 };
 note(*engine,60);assert(check(3,"Bird note"));
 assert(ui->set_parameter(engine_target(Instrument::Chimera,ChimeraSpecies),2));
 bool held=check(3,"Bird note held during species edit");
 assert(engine->submit({CommandType::Panic,0,0,0}));render(*engine,2);
 note(*engine,60);assert(check(5,"Next alligator note"));
 assert(engine->submit({CommandType::Panic,0,0,0}));render(*engine,2);
 assert(engine->submit({CommandType::LaunchScene,0,0,0}));
 bool scene=check(0,"Human scene announcement on alligator track");
 assert(engine->submit({CommandType::Panic,0,0,0}));render(*engine,2);
 assert(ui->set_parameter(engine_target(Instrument::Chimera,ChimeraProgram),ChimeraPlay));
 note(*engine,60);render(*engine,50);
 assert(ui->set_parameter(engine_target(Instrument::Chimera,ChimeraBody),1));
 render(*engine,50); // Wait for the live BODY crossfade before measuring.
 bool body=check(1,"Live human body change");
 assert(held && scene && body);
}

int main(){
 const auto saved=hardware::active_profile;
 hardware::set_active_profile(hardware::decode((1u<<23)|(0x17u<<4)));
 model_accounting();
 std::puts("id\tcontrol\tmodel\tA\tB\tPCM_delta_RMS\tchanged_PCM16\ttracts_A\ttracts_B\tfollow_A\tfollow_B");
 bool ok=true;
 for(unsigned i=0;i<kChimeraParameterCount;++i)ok=compare(i) && ok;
 for(int species=0;species<4;++species)for(unsigned i=0;i<kChimeraMacroCount;++i)ok=compare(i,species) && ok;
 for(unsigned local:{ChimeraVibratoRate,ChimeraSpread,ChimeraLateral}){
  assert(take(local,false,-1,false).samples==take(local,true,-1,false).samples);
 }
 for(auto source:{StepSource::Lock,StepSource::Preset}){
  const auto a=take(ChimeraTongueX,false,-1,true,source),b=take(ChimeraTongueX,true,-1,true,source);
  assert(*std::max_element(a.samples.begin(),a.samples.end())>.001f && a.samples==b.samples);
 }
 hardware::set_active_profile(saved);return ok?0:1;
}
