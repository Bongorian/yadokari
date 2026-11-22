#pragma once
#include "alloc_policy.h"
#include "chimera_geometry.h"
#include "chimera_state.h"
#include "chimera_speech.h"
#include <stdint.h>

namespace groove {
struct ChimeraDsp;
extern "C" ChimeraDsp* yadokari_chimera_new(unsigned char* arena,size_t bytes,unsigned body,bool wait);
extern "C" void yadokari_chimera_controls(ChimeraDsp*,float hz,float gate,const float* values,const float* detail,bool score);
extern "C" unsigned yadokari_chimera_hit(ChimeraDsp*,unsigned note,float velocity);
extern "C" unsigned yadokari_chimera_speak(ChimeraDsp*,const ChimeraPhrase*,float elapsed,bool timed);
extern "C" float yadokari_chimera_sample(ChimeraDsp*,float score_time);
extern "C" void yadokari_chimera_reset(ChimeraDsp*);
extern "C" void yadokari_chimera_geometry(const ChimeraDsp*,ChimeraGeometry*);

extern "C" bool yadokari_chimera_tongue(const ChimeraDsp*,float*);
class ChimeraVoice {
 static constexpr unsigned arena_size=512*1024;
 static constexpr unsigned release_frames=2400;
 unsigned char* arena_=nullptr;
 ChimeraDsp* dsp_=nullptr;
 ChimeraGeometry resting_[7]{};
 ChimeraState detail_{};
 unsigned body_=0;
 unsigned phase_=0,control_=0,tail_=0,model_fade_=0;
 unsigned fading_to_=0;
 float previous_=0,current_=0;
 unsigned note_=0,score_frames_=0;
 float sing_time_=0,sing_end_=0;
 bool active_=false,releasing_=false,attack_=false,singing_=false,rendered_=false;
 const ChimeraPhrase* current_phrase_=nullptr;
 struct HeldKey {uint16_t channel=65535;uint8_t input=0,pitch=0;float frequency=0,level=0;};
 HeldKey held_[32]{};
 unsigned held_count_=0;
public:
 ChimeraVoice()=default;
 ChimeraVoice(const ChimeraVoice&)=delete;
 ChimeraVoice& operator=(const ChimeraVoice&)=delete;
 ~ChimeraVoice(){delete[] arena_;}
 void init(){
  arena_=alloc_uninitialized_bytes(arena_size);
  if(!arena_)return;
  // Capture each resting shape, then leave the default male model in the arena.
  const unsigned models[]={1,2,3,4,5,6,0};
  for(unsigned body:models){
   auto* model=yadokari_chimera_new(arena_,arena_size,body,true);
   if(!model)return;
   yadokari_chimera_geometry(model,&resting_[body]);
   resting_[body].model=body;
   if(body==0)dsp_=model;
  }
 }
 bool ready()const{return dsp_!=nullptr;}
 bool active()const{return active_;}
 unsigned model()const{return body_;}
 bool continuing(const ChimeraPhrase* phrase,unsigned note)const{return singing_ && note_==note && phrase && phrase==current_phrase_;}
 void remember(uint16_t channel,uint8_t input,uint8_t pitch,float frequency,float level){
  forget(channel,input);
  if(held_count_<32)held_[held_count_++]={channel,input,pitch,frequency,level};
 }
 void forget(uint16_t channel,unsigned note){
  unsigned remaining=0;
  for(unsigned i=0;i<held_count_;++i)if(held_[i].channel!=channel || (note<128 && held_[i].input!=note))
   held_[remaining++]=held_[i];
  held_count_=remaining;
 }
 bool previous_key(uint16_t& channel,uint8_t& input,uint8_t& pitch,float& frequency,float& level){
  if(!held_count_)return false;
  const auto key=held_[--held_count_];
  channel=key.channel;input=key.input;pitch=key.pitch;frequency=key.frequency;level=key.level;
  return true;
 }
#ifdef TEST
 float sing_time()const{return singing_?sing_time_:-1;}
#endif
 void trigger(unsigned note=60,bool keep_score=false){
  active_=true;note_=note;control_=0;tail_=0;releasing_=false;attack_=!keep_score;
 }
 void stop(){
  if(active_ && dsp_)yadokari_chimera_reset(dsp_);
  active_=rendered_=false;phase_=0;control_=0;previous_=current_=0;
  score_frames_=model_fade_=0;attack_=singing_=false;current_phrase_=nullptr;
  held_count_=0;fading_to_=body_;
 }
 void update_model(const ChimeraState& detail){
  const unsigned selected=chimera_model(detail);
  if(selected==body_){model_fade_=0;fading_to_=body_;return;}
  if(rendered_ && fading_to_!=selected){fading_to_=selected;model_fade_=48;return;}
  if(model_fade_)return;
  auto* built=yadokari_chimera_new(arena_,arena_size,selected,false);
  if(!built)return;
  dsp_=built;body_=fading_to_=selected;rendered_=false;
  attack_=true;score_frames_=0;singing_=false;phase_=0;previous_=current_=0;
 }
 bool tongue(float* xy)const{return active_ && dsp_ && yadokari_chimera_tongue(dsp_,xy);}
 bool control_due()const{return control_==0;}
 float next(float hz,float velocity,bool gate,const float* values,const ChimeraState* detail=nullptr,const ChimeraPhrase* phrase=nullptr,
            float initial_sing_time=0,float sing_increment=0,float aligned_sing_time=-1){
  if(detail)detail_=*detail;
  if(!active_ || !dsp_)return 0;
  auto* dsp=dsp_;
  if(attack_){
   attack_=false;
   unsigned program=unsigned(detail_.values[ChimeraProgram-kChimeraMacroCount]);
   singing_=false;
   if(rendered_)yadokari_chimera_reset(dsp);
   score_frames_=0;
   if(program==ChimeraBeatbox)score_frames_=yadokari_chimera_hit(dsp,note_,velocity);
   else if(program==ChimeraSpeak || program==ChimeraSing){
    if(!phrase){stop();return 0;}
    singing_=program==ChimeraSing;current_phrase_=phrase;
    sing_time_=singing_?initial_sing_time:0;
    sing_end_=phrase->seconds;
    score_frames_=yadokari_chimera_speak(dsp,phrase,sing_time_,singing_);
   }
  }
  if(singing_ && aligned_sing_time>=0)sing_time_=aligned_sing_time;
  if(!gate && !score_frames_ && !releasing_){releasing_=true;tail_=release_frames;control_=0;}
  if(!control_){
   yadokari_chimera_controls(dsp,hz,gate && !score_frames_?velocity:0.0f,values,detail_.values,score_frames_!=0);
   control_=48;
  }
  --control_;
  // Advance the 32 kHz model by two samples for every three 48 kHz frames.
  phase_+=2;
  if(phase_>=3){
   phase_-=3;previous_=current_;current_=yadokari_chimera_sample(dsp,singing_?sing_time_:-1);
   if(!__builtin_isfinite(current_)){stop();return 0;}
  }
  float result=previous_+(current_-previous_)*(float(phase_)/3);
  if(singing_){
   sing_time_+=sing_increment;
   if(sing_time_>=sing_end_)stop();
  }else if(score_frames_ && !--score_frames_)stop();
  if(releasing_){result*=float(tail_)/release_frames;if(!--tail_)stop();}
  rendered_=active_;
  if(model_fade_)result*=float(--model_fade_)/48;
  return result*.5f;
 }
 void geometry(ChimeraGeometry& out,unsigned body=0,bool live=true)const{
  if(live && active_){yadokari_chimera_geometry(dsp_,&out);out.model=body_;}
  else out=resting_[body<7?body:0];
 }
};
}
