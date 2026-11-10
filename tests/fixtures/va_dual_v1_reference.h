// Audio regression reference from master 96f2fe52; test only.
#pragma once
#include "Synthesis/oscillator.h"
#include "Control/adsr.h"
#include "Filters/ladder.h"
#include <math.h>
namespace groove {
// Existing MIT DaisySP blocks; all state and noise are private to this voice.
class ReferenceVirtualAnalogVoice {
 daisysp::Oscillator osc1_,osc2_,sub_;daisysp::Adsr envelope_;daisysp::LadderFilter filter_;
 float rate_=48000,velocity_=0,level_=0,lfo_=0,values_[18]{},target_[18]{};uint32_t noise_=1,frame_=0;bool active_=false;
 static unsigned waveform(float v){static const uint8_t shapes[]={daisysp::Oscillator::WAVE_POLYBLEP_SAW,daisysp::Oscillator::WAVE_POLYBLEP_SQUARE,daisysp::Oscillator::WAVE_POLYBLEP_TRI,daisysp::Oscillator::WAVE_SIN};unsigned i=unsigned(v+.5f);return shapes[i<4?i:0];}
 public:
 void init(unsigned rate){rate_=float(rate);osc1_.Init(rate_);osc2_.Init(rate_);sub_.Init(rate_);sub_.SetWaveform(daisysp::Oscillator::WAVE_SIN);osc1_.SetAmp(1);osc2_.SetAmp(1);sub_.SetAmp(1);envelope_.Init(rate_);filter_.Init(rate_);active_=false;}
 void parameters(const float* p){for(unsigned i=0;i<18;++i)target_[i]=p[i];envelope_.SetTime(daisysp::ADSR_SEG_ATTACK,p[11]);envelope_.SetTime(daisysp::ADSR_SEG_DECAY,p[12]);envelope_.SetSustainLevel(p[13]);envelope_.SetTime(daisysp::ADSR_SEG_RELEASE,p[14]);}
 void trigger(float velocity,const float* p,uint32_t seed){init(unsigned(rate_));parameters(p);for(unsigned i=0;i<18;++i)values_[i]=p[i];velocity_=velocity;noise_=seed?seed:1;lfo_=0;frame_=0;active_=true;}
 void visual_envelope(float* out)const{for(unsigned i=0;i<4;++i)out[i]=target_[11+i];}
 bool active()const{return active_;}float level()const{return level_;}
 float next(float frequency,bool gate){if(!active_)return 0;level_=envelope_.Process(gate);if(!gate && !envelope_.IsRunning()){active_=false;return 0;}
  if((frame_++&31)==0){for(unsigned i=0;i<18;++i)values_[i]+=(target_[i]-values_[i])*.12f;values_[0]=target_[0];values_[1]=target_[1];values_[16]=target_[16];
   osc1_.SetWaveform(waveform(values_[0]));osc2_.SetWaveform(waveform(values_[1]));
   osc1_.SetFreq(fminf(frequency*powf(2,-values_[2]/2400),rate_*.45f));osc2_.SetFreq(fminf(frequency*powf(2,values_[2]/2400+values_[16]/12),rate_*.45f));sub_.SetFreq(fminf(frequency*.5f,rate_*.45f));
   float width=values_[8]+values_[9]*sinf(lfo_*6.283185307f);width=fmaxf(.05f,fminf(.95f,width));osc1_.SetPw(width);osc2_.SetPw(1-width);
   filter_.SetFreq(fminf(rate_*.425f,values_[4]*powf(2,values_[15]*level_)));filter_.SetRes(values_[5]);filter_.SetInputDrive(values_[17]);
  }
  lfo_+=values_[10]/rate_;if(lfo_>=1)lfo_-=1;noise_^=noise_<<13;noise_^=noise_>>17;noise_^=noise_<<5;
  float noise=float(int32_t(noise_))/2147483648.f;
  float a=osc1_.Process(),b=osc2_.Process();float x=(a+(b-a)*values_[3]+sub_.Process()*values_[6]+noise*values_[7])/(1+values_[6]+values_[7]);
  return filter_.Process(x)*level_*velocity_*.35f;
 }
};
}
