#pragma once
#include "spectral_voices.h"
#include "Synthesis/formantosc.h"
#include "Synthesis/vosim.h"
namespace groove {
class FormantVoice:public SpectralEnvelope {
 daisysp::FormantOscillator first_,second_;daisysp::VosimOscillator vosim_;
 float values_[9]{},vowel_=0,motion_=0,shift_=1;unsigned frame_=0;
public:
 // Empty vendor constructors leave numeric state (including ps_inc_) unset.
 FormantVoice(){__builtin_memset(static_cast<void*>(&first_),0,sizeof first_);__builtin_memset(static_cast<void*>(&second_),0,sizeof second_);__builtin_memset(static_cast<void*>(&vosim_),0,sizeof vosim_);}
 void init(unsigned rate){rate_=float(rate);first_.Init(rate_);second_.Init(rate_);vosim_.Init(rate_);envelope_.Init(rate_);active_=false;}
 void parameters(const float* p){for(unsigned i=0;i<9;++i)values_[i]=p[i];envelope_parameters(p);shift_=powf(2,p[2]/12);}
 void trigger(float velocity,const float* p){init(unsigned(rate_));parameters(p);velocity_=velocity;vowel_=p[1];motion_=0;frame_=0;active_=true;}
 float next(float hz,bool gate){if(!active_ || !envelope_next(gate))return 0;
  if((frame_++&31)==0){static constexpr float vowels[][2]={{300,870},{450,800},{800,1150},{500,1900},{300,2300}};
   vowel_+=(values_[1]-vowel_)*.08f;float pos=fmaxf(0,fminf(4,vowel_*4+sinf(motion_)*values_[4]*.3f));unsigned i=unsigned(pos);if(i>3)i=3;float f=pos-i;
   float f1=(vowels[i][0]+(vowels[i+1][0]-vowels[i][0])*f)*shift_,f2=(vowels[i][1]+(vowels[i+1][1]-vowels[i][1])*f)*shift_;
   first_.SetFormantFreq(f1);second_.SetFormantFreq(f2);first_.SetPhaseShift(values_[3]);second_.SetPhaseShift(values_[3]);vosim_.SetForm1Freq(f1);vosim_.SetForm2Freq(f2);vosim_.SetShape(values_[3]*2-1);
  }
  first_.SetCarrierFreq(hz);second_.SetCarrierFreq(hz);vosim_.SetFreq(hz);motion_+=6.2831853f*.6f/rate_;if(motion_>=6.2831853f)motion_-=6.2831853f;
  float x=values_[0]<.5f?(first_.Process()+second_.Process())*.5f:vosim_.Process();return x*level_*velocity_*.35f;
 }
};
}
