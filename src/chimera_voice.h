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

class ChimeraVoice {
 static constexpr unsigned arena_size=512*1024;
 static constexpr unsigned release_frames=2400;
 unsigned char* arena_=nullptr;
 ChimeraDsp* dsp_=nullptr;
 ChimeraGeometry resting_[7]{};
 ChimeraState detail_{};
 unsigned body_=0;
 unsigned owner_=16,phase_=0,control_=0,tail_=0;
 float previous_=0,current_=0;
 unsigned note_=0,score_frames_=0;
 float sing_time_=0,sing_end_=0;
 bool releasing_=false,attack_=false,singing_=false;
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
 unsigned owner()const{return owner_;}
 unsigned note()const{return note_;}
 bool can_play(unsigned track)const{return ready() && (owner_==16 || owner_==track);}
 bool attack_pending()const{return attack_;}
 bool held_legato()const{
  const float mode=detail_.values[ChimeraProgram-kChimeraMacroCount];
  return mode==ChimeraPlay || mode==ChimeraBestiary;
 }
#ifdef TEST
 float sing_time()const{return singing_?sing_time_:-1;}
#endif
 void trigger(unsigned track,unsigned note=60,bool keep_score=false){
  owner_=track;note_=note;control_=0;tail_=0;releasing_=false;attack_=!keep_score;
 }
 void stop(){
  if(owner_<16)yadokari_chimera_reset(dsp_);
  owner_=16;phase_=0;previous_=current_=0;score_frames_=0;attack_=false;singing_=false;
 }
 bool control_due()const{return control_==0;}
 float next(float hz,float velocity,bool gate,const float* values,const ChimeraState* detail=nullptr,const ChimeraPhrase* phrase=nullptr,
            float initial_sing_time=0,float sing_increment=0,float aligned_sing_time=-1){
  if(detail)detail_=*detail;
  const unsigned selected=chimera_model(detail_);
  if(selected!=body_){
   auto* built=yadokari_chimera_new(arena_,arena_size,selected,false);
   if(!built)return 0;
   dsp_=built;body_=selected;
   if(score_frames_)attack_=true;
   phase_=0;control_=0;previous_=current_=0;
   return 0;
  }
  auto* dsp=dsp_;
  if(attack_){
   attack_=false;
   unsigned program=unsigned(detail_.values[ChimeraProgram-kChimeraMacroCount]);
   singing_=false;
   if(program==ChimeraBeatbox)score_frames_=yadokari_chimera_hit(dsp,note_,velocity);
   else if(program==ChimeraSpeak || program==ChimeraSing){
    if(!phrase){stop();return 0;}
    singing_=program==ChimeraSing;
    sing_time_=singing_?initial_sing_time:0;
    sing_end_=phrase->seconds;
    score_frames_=yadokari_chimera_speak(dsp,phrase,sing_time_,singing_);
   }
   else if(score_frames_){yadokari_chimera_reset(dsp);score_frames_=0;}
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
  return result*.5f;
 }
 void geometry(unsigned track,ChimeraGeometry& out,unsigned body=0)const{
  if(owner_==track){yadokari_chimera_geometry(dsp_,&out);out.model=body_;}
  else out=resting_[body<7?body:0];
 }
};
}
