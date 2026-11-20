#pragma once
#include "Drums/analogbassdrum.h"
#include "Drums/analogsnaredrum.h"
#include "Drums/synthbassdrum.h"
#include "Drums/synthsnaredrum.h"
#include "Drums/hihat.h"
#include <stdint.h>
namespace groove {
// DaisySP drum generators, one selected voice per track. Initialization happens
// outside the audio callback; Process() never allocates.
//
// The bank is shared by every engine voice of its track, so it advances at
// most once per output sample (next(now) caches by sample index): overlapping
// hits no longer run the generator N times per sample.
//
// Non-finite controls are replaced by safe values before they reach DaisySP,
// and a generator whose output turns non-finite is re-initialised in place
// (Init only assigns fields). A NaN can therefore cost one silent sample, but
// can no longer latch in the filter state and hold the master in Audio fault.
class DaisyDrumBank {
 daisysp::AnalogBassDrum analog_kick_;
 daisysp::AnalogSnareDrum analog_snare_;
 daisysp::SyntheticBassDrum synthetic_kick_;
 daisysp::SyntheticSnareDrum synthetic_snare_;
 daisysp::HiHat<> hat_;
 unsigned kit_=0,voice_=0,recoveries_=0;
 float rate_=48000,last_=0;
 uint64_t last_at_=0;
 bool cached_=false;
 static float unit(float x,float fallback){return isfinite(x)?(x<0?0:(x>1?1:x)):fallback;}
 void recover(){
  ++recoveries_;
  if(kit_==0){if(voice_==0)analog_kick_.Init(rate_);else if(voice_==1)analog_snare_.Init(rate_);else hat_.Init(rate_);}
  else{if(voice_==0)synthetic_kick_.Init(rate_);else if(voice_==1)synthetic_snare_.Init(rate_);else hat_.Init(rate_);}
 }
 float process(){return kit_==0?(voice_==0?analog_kick_.Process():voice_==1?analog_snare_.Process():hat_.Process()):(voice_==0?synthetic_kick_.Process():voice_==1?synthetic_snare_.Process():hat_.Process());}
public:
 void init(float rate){rate_=isfinite(rate) && rate>0?rate:48000;analog_kick_.Init(rate_);analog_snare_.Init(rate_);synthetic_kick_.Init(rate_);synthetic_snare_.Init(rate_);hat_.Init(rate_);cached_=false;}
 void trigger(unsigned kit,const float* p,float velocity,float hz){
  const float voice=unit(p[0],0),tune=unit(p[1],0.5f),tone=unit(p[2],0.5f),decay=unit(p[3],0.5f),accent_amount=unit(p[4],1);
  kit_=kit?1:0;voice_=unsigned(voice*2.999f);if(voice_>2)voice_=2;
  float accent=unit(velocity,0)*(0.25f+0.75f*accent_amount);if(accent>1)accent=1;
  if(!isfinite(hz))hz=65.40639f;
  float pitch=hz*powf(2.0f,(tune-0.5f)*2.0f);if(!(pitch>=30))pitch=30;if(pitch>500)pitch=500;
  if(kit_==0){
   if(voice_==0){analog_kick_.SetFreq(pitch*0.5f);analog_kick_.SetTone(tone);analog_kick_.SetDecay(decay);analog_kick_.SetAccent(accent);analog_kick_.Trig();}
   else if(voice_==1){analog_snare_.SetFreq(pitch);analog_snare_.SetTone(tone);analog_snare_.SetDecay(decay);analog_snare_.SetAccent(accent);analog_snare_.Trig();}
   else{hat_.SetTone(tone);hat_.SetDecay(decay);hat_.SetAccent(accent);hat_.Trig();}
  }else{
   if(voice_==0){synthetic_kick_.SetFreq(pitch*0.5f);synthetic_kick_.SetTone(tone);synthetic_kick_.SetDecay(decay);synthetic_kick_.SetAccent(accent);synthetic_kick_.Trig();}
   else if(voice_==1){synthetic_snare_.SetFreq(pitch);synthetic_snare_.SetSnappy(tone);synthetic_snare_.SetDecay(decay);synthetic_snare_.SetAccent(accent);synthetic_snare_.Trig();}
   else{hat_.SetTone(tone);hat_.SetDecay(decay);hat_.SetAccent(accent);hat_.Trig();}
  }
 }
 // One generator step per output sample, shared by all voices of the track.
 float next(uint64_t now){
  if(cached_ && now==last_at_)return last_;
  last_=next();
  last_at_=now;cached_=true;
  return last_;
 }
 // Standalone stepping (tests, offline tools): advances on every call.
 float next(){cached_=false;float x=process();if(isfinite(x))return x;recover();return 0;}
 unsigned recoveries()const{return recoveries_;}
};
}
