#pragma once
#include "wavetable_bank.h"
#include "Control/adsr.h"
#include <math.h>
namespace groove {
// Original phase accumulator, mip selection and two-dimensional interpolation.
class WavetableVoice {
 float rate_=48000,phase_=0,scan_=0,frequency_=261.626f,velocity_=0,level_=0;
 float values_[8]{},target_[8]{};daisysp::Adsr envelope_;bool started_=false;
 static float table(unsigned wave,unsigned mip,float phase){float x=phase*256;unsigned i=unsigned(x)&255;float a=kWavetableData[wave][mip][i],b=kWavetableData[wave][mip][(i+1)&255];return (a+(b-a)*(x-floorf(x)))/32767;}
 public:
 void init(unsigned rate){rate_=float(rate);envelope_.Init(rate_);started_=false;}
 void parameters(const float* values){for(unsigned i=0;i<8;++i)target_[i]=values[i];envelope_.SetTime(daisysp::ADSR_SEG_ATTACK,values[3]);envelope_.SetTime(daisysp::ADSR_SEG_DECAY,values[5]);envelope_.SetSustainLevel(values[6]);envelope_.SetTime(daisysp::ADSR_SEG_RELEASE,values[4]);}
 void trigger(float hz,float velocity,const float* values){if(!wavetable_bank_ready()){started_=false;return;}frequency_=hz;velocity_=velocity;phase_=scan_=0;envelope_.Init(rate_);parameters(values);for(unsigned i=0;i<8;++i)values_[i]=values[i];started_=true;}
 bool active()const{return started_;}
 float level()const{return level_;}
 void visual_controls(float* out)const{for(unsigned i=0;i<3;++i)out[i]=values_[i];}
 void visual_envelope(float* out)const{out[0]=values_[3];out[1]=values_[5];out[2]=values_[6];out[3]=values_[4];}
 float scan_phase()const{return scan_;}
 float position()const{float p=values_[0]+values_[1]*sinf(scan_*6.283185307f);return fmaxf(0,fminf(127,p))/127;}
 float next(float hz,bool gate){if(!started_)return 0;frequency_=hz;for(unsigned i=0;i<8;++i)values_[i]+=(target_[i]-values_[i])*.002f;
  float step=frequency_/rate_;if(step>.45f)step=.45f;if(step<0)step=0;
  // Fade between two safe mip levels BEFORE the upper table reaches Nyquist.
  float allowed=step>0?.45f/step:128;allowed*=.015625f+.984375f*values_[7];unsigned mip=0;unsigned harmonics=64;
  while(mip<6 && float(harmonics)>allowed){++mip;harmonics>>=1;}
  unsigned next=mip<6?mip+1:mip;float blend=2-allowed/float(harmonics);if(blend<0)blend=0;if(blend>1)blend=1;
  float position=values_[0]+values_[1]*sinf(scan_*6.283185307f);if(position<0)position=0;if(position>127)position=127;unsigned a=unsigned(position),b=a<127?a+1:a;
  float x=table(a,mip,phase_),y=table(b,mip,phase_);x+=(y-x)*(position-a);
  if(blend>0){float lo=table(a,next,phase_),hi=table(b,next,phase_);lo+=(hi-lo)*(position-a);x+=(lo-x)*blend;}
  phase_+=step;if(phase_>=1)phase_-=1;scan_+=values_[2]/rate_;if(scan_>=1)scan_-=1;
  level_=envelope_.Process(gate);if(!gate && !envelope_.IsRunning())started_=false;return x*level_*velocity_*.35f;
 }
};
}
