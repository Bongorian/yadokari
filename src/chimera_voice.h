#pragma once
#include "alloc_policy.h"
#include "chimera_geometry.h"
#include <stdint.h>

namespace groove {
struct ChimeraDsp;
extern "C" ChimeraDsp* yadokari_chimera_new(unsigned char* arena,size_t bytes);
extern "C" void yadokari_chimera_controls(ChimeraDsp*,float hz,float gate,const float* values);
extern "C" float yadokari_chimera_sample(ChimeraDsp*);
extern "C" void yadokari_chimera_reset(ChimeraDsp*);
extern "C" void yadokari_chimera_geometry(const ChimeraDsp*,ChimeraGeometry*);

class ChimeraVoice {
 static constexpr unsigned arena_size=512*1024;
 static constexpr unsigned release_frames=2400;
 unsigned char* arena_=nullptr;
 ChimeraDsp* dsp_=nullptr;
 ChimeraGeometry resting_{};
 unsigned owner_=16,phase_=0,control_=0,tail_=0;
 float previous_=0,current_=0;
 bool releasing_=false;
public:
 ChimeraVoice()=default;
 ChimeraVoice(const ChimeraVoice&)=delete;
 ChimeraVoice& operator=(const ChimeraVoice&)=delete;
 ~ChimeraVoice(){delete[] arena_;}
 void init(){
  arena_=alloc_uninitialized_bytes(arena_size);
  if(arena_)dsp_=yadokari_chimera_new(arena_,arena_size);
  if(dsp_)yadokari_chimera_geometry(dsp_,&resting_);
 }
 bool ready()const{return dsp_!=nullptr;}
 unsigned owner()const{return owner_;}
 bool can_play(unsigned track)const{return ready() && (owner_==16 || owner_==track);}
 void trigger(unsigned track){
  owner_=track;control_=0;tail_=0;releasing_=false;
 }
 void stop(){
  if(owner_<16)yadokari_chimera_reset(dsp_);
  owner_=16;phase_=0;previous_=current_=0;
 }
 float next(float hz,float velocity,bool gate,const float* values){
  if(!gate && !releasing_){releasing_=true;tail_=release_frames;control_=0;}
  if(!control_){
   yadokari_chimera_controls(dsp_,hz,gate?velocity:0.0f,values);
   control_=48;
  }
  --control_;
  // Advance the 32 kHz model by two samples for every three 48 kHz frames.
  phase_+=2;
  if(phase_>=3){
   phase_-=3;previous_=current_;current_=yadokari_chimera_sample(dsp_);
   if(!__builtin_isfinite(current_)){stop();return 0;}
  }
  float result=previous_+(current_-previous_)*(float(phase_)/3);
  if(releasing_){result*=float(tail_)/release_frames;if(!--tail_)stop();}
  return result*.5f;
 }
 void geometry(unsigned track,ChimeraGeometry& out)const{
  if(owner_==track)yadokari_chimera_geometry(dsp_,&out);
  else out=resting_;
 }
};
}
