#pragma once
#include "Synthesis/oscillator.h"
#include "Control/adsr.h"
#include "Filters/ladder.h"
#include "Filters/svf.h"
#include "Utility/dsp.h"
#include "virtual_analog_state.h"
#include "va_mod_oscillator.h"
#include <math.h>
namespace groove {
// One Engine/audio owner. Templates contain only value-state Daisy objects;
// sounding envelopes/oscillators/filters are copied, never shared or advanced here.
struct VirtualAnalogTriggerBank {
 daisysp::Oscillator osc1,osc2,sub;daisysp::Adsr envelope;daisysp::LadderFilter filter;daisysp::Svf svf;float rate=0;
 struct Math {uint32_t input=0;float output=0;bool valid=false;};Math math[6]{};
 unsigned envelope_updates=0;float adsr[4]{};bool adsr_valid=false;
 void init(unsigned sample_rate){rate=float(sample_rate);osc1.Init(rate);osc2.Init(rate);sub.Init(rate);sub.SetWaveform(daisysp::Oscillator::WAVE_SIN);osc1.SetAmp(1);osc2.SetAmp(1);sub.SetAmp(1);envelope.Init(rate);filter.Init(rate);svf.Init(rate);adsr_valid=false;envelope_updates=0;for(auto& m:math)m={};}
 void prepare_envelope(const float* p){bool changed=!adsr_valid;for(unsigned i=0;i<4;++i)changed|=adsr[i]!=p[11+i];if(!changed)return;envelope.SetTime(daisysp::ADSR_SEG_ATTACK,p[11]);envelope.SetTime(daisysp::ADSR_SEG_DECAY,p[12]);envelope.SetSustainLevel(p[13]);envelope.SetTime(daisysp::ADSR_SEG_RELEASE,p[14]);for(unsigned i=0;i<4;++i)adsr[i]=p[11+i];adsr_valid=true;++envelope_updates;}
 __attribute__((always_inline)) inline float evaluate(unsigned slot,float input){uint32_t bits;__builtin_memcpy(&bits,&input,4);auto& m=math[slot];if(m.valid && m.input==bits)return m.output;float result=slot==3?sinf(input):slot==4?expf(input):powf(2,input);m.input=bits;m.output=result;m.valid=true;return result;}
};
// Existing MIT DaisySP blocks; all state and noise are private to this voice.
class VirtualAnalogVoice {
 friend class VaSampleSlot;
 VirtualAnalogTriggerBank* trigger_bank_=nullptr;
 __attribute__((always_inline)) inline float math(unsigned slot,float x){if(trigger_bank_ && trigger_bank_->rate==rate_)return trigger_bank_->evaluate(slot,x);return slot==3?sinf(x):slot==4?expf(x):powf(2,x);}
 daisysp::Oscillator osc1_,osc2_,sub_;daisysp::Adsr envelope_;daisysp::LadderFilter filter_;daisysp::Svf svf_;VaModOscillator mod_;float hz1_=0,hz2_=0,width_=.5f;
 float rate_=48000,velocity_=0,level_=0,lfo_=0,values_[kVirtualAnalogParameterCount]{},target_[kVirtualAnalogParameterCount]{};uint32_t noise_=1,frame_=0;bool active_=false;
 static unsigned waveform(float v){static const uint8_t shapes[]={daisysp::Oscillator::WAVE_POLYBLEP_SAW,daisysp::Oscillator::WAVE_POLYBLEP_SQUARE,daisysp::Oscillator::WAVE_POLYBLEP_TRI,daisysp::Oscillator::WAVE_SIN};unsigned i=unsigned(v+.5f);return shapes[i<4?i:0];}
 public:
 void init(unsigned rate,VirtualAnalogTriggerBank* bank=nullptr){rate_=float(rate);trigger_bank_=bank;reset_primitives();}
 private:
 void reset_primitives(){if(trigger_bank_ && trigger_bank_->rate==rate_){osc1_=trigger_bank_->osc1;osc2_=trigger_bank_->osc2;sub_=trigger_bank_->sub;envelope_=trigger_bank_->envelope;filter_=trigger_bank_->filter;svf_=trigger_bank_->svf;}else{osc1_.Init(rate_);osc2_.Init(rate_);sub_.Init(rate_);sub_.SetWaveform(daisysp::Oscillator::WAVE_SIN);osc1_.SetAmp(1);osc2_.SetAmp(1);sub_.SetAmp(1);envelope_.Init(rate_);filter_.Init(rate_);svf_.Init(rate_);}mod_.reset();active_=false;}
 public:
 void parameters(const float* p){for(unsigned i=0;i<kVirtualAnalogParameterCount;++i)target_[i]=p[i];envelope_.SetTime(daisysp::ADSR_SEG_ATTACK,p[11]);envelope_.SetTime(daisysp::ADSR_SEG_DECAY,p[12]);envelope_.SetSustainLevel(p[13]);envelope_.SetTime(daisysp::ADSR_SEG_RELEASE,p[14]);}
 void trigger(float velocity,const float* p,uint32_t seed){if(trigger_bank_ && trigger_bank_->rate==rate_)trigger_bank_->prepare_envelope(p);reset_primitives();parameters(p);for(unsigned i=0;i<kVirtualAnalogParameterCount;++i)values_[i]=p[i];velocity_=velocity;noise_=seed?seed:1;lfo_=0;frame_=0;active_=true;}
 void visual_envelope(float* out)const{for(unsigned i=0;i<4;++i)out[i]=target_[11+i];}
 bool active()const{return active_;}float level()const{return level_;}
 float next(float frequency,bool gate){if(!active_)return 0;level_=envelope_.Process(gate);if(!gate && !envelope_.IsRunning()){active_=false;return 0;}
  if((frame_++&31)==0){for(unsigned i=0;i<kVirtualAnalogParameterCount;++i)values_[i]+=(target_[i]-values_[i])*.12f;values_[0]=target_[0];values_[1]=target_[1];values_[16]=target_[16];values_[18]=target_[18];values_[22]=target_[22];values_[23]=target_[23];values_[24]=target_[24];
   if(values_[20]!=0)frequency*=math(5,values_[20]*math(4,-float(frame_-1)/(rate_*values_[21]))/12);
   osc1_.SetWaveform(waveform(values_[0]));osc2_.SetWaveform(waveform(values_[1]));
   hz1_=fminf(frequency*math(0,-values_[2]/2400),rate_*.45f);hz2_=fminf(frequency*math(1,values_[2]/2400+values_[16]/12),rate_*.45f);
   osc1_.SetFreq(hz1_);osc2_.SetFreq(hz2_);sub_.SetFreq(fminf(frequency*.5f,rate_*.45f));sub_.SetWaveform(values_[22]<.5f?daisysp::Oscillator::WAVE_SIN:daisysp::Oscillator::WAVE_POLYBLEP_SQUARE);
   float width=values_[8]+values_[9]*math(3,lfo_*6.283185307f);width=fmaxf(.05f,fminf(.95f,width));width_=width;osc1_.SetPw(width);osc2_.SetPw(1-width);
   // Both filter paths apply their current coefficients before first use.
   // The inactive ladder has no time-advancing state in these setters.
   if(values_[23]<.5f){filter_.SetFreq(fminf(rate_*.425f,values_[4]*math(2,values_[15]*level_)));filter_.SetRes(values_[5]);filter_.SetInputDrive(values_[17]);}if(values_[23]>0){svf_.SetFreq(fminf(rate_*.3f,values_[4]*math(2,values_[15]*level_)));svf_.SetRes(fminf(.95f,values_[5]/1.2f));svf_.SetDrive(0);}
  }
  lfo_+=values_[10]/rate_;if(lfo_>=1)lfo_-=1;noise_^=noise_<<13;noise_^=noise_>>17;noise_^=noise_<<5;
  float noise=float(int32_t(noise_))/2147483648.f;
  float a=osc1_.Process(),b=osc2_.Process();if(values_[18]>.5f || values_[19]>0)mod_.process(hz1_,hz2_,rate_,unsigned(values_[0]),unsigned(values_[1]),width_,values_[18]>.5f,values_[19],a,b);float x=(a+(b-a)*values_[3]+sub_.Process()*values_[6]+noise*values_[7])/(1+values_[6]+values_[7]);
  float output;if(values_[23]<.5f)output=filter_.Process(x);else{svf_.Process(daisysp::SoftClip(x*values_[17]));if(values_[23]<1.5f)output=values_[24]<.5f?svf_.Low():values_[24]<1.5f?svf_.High():svf_.Band();else{float position=values_[25]*2;output=position<=1?svf_.Low()+(svf_.Notch()-svf_.Low())*position:svf_.Notch()+(svf_.High()-svf_.Notch())*(position-1);}}
  return output*level_*velocity_*.35f;
 }
};
}
