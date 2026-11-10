#pragma once
#include "Drums/analogbassdrum.h"
#include "Drums/analogsnaredrum.h"
#include "Drums/synthbassdrum.h"
#include "Drums/synthsnaredrum.h"
#include "Drums/hihat.h"
namespace groove {
// DaisySP drum generators, one selected voice per track. Initialization happens
// outside the audio callback; Process() never allocates.
class DaisyDrumBank {
 daisysp::AnalogBassDrum analog_kick_;
 daisysp::AnalogSnareDrum analog_snare_;
 daisysp::SyntheticBassDrum synthetic_kick_;
 daisysp::SyntheticSnareDrum synthetic_snare_;
 daisysp::HiHat<> hat_;
 unsigned kit_=0,voice_=0;
public:
 void init(float rate){analog_kick_.Init(rate);analog_snare_.Init(rate);synthetic_kick_.Init(rate);synthetic_snare_.Init(rate);hat_.Init(rate);}
 void trigger(unsigned kit,const float* p,float velocity,float hz){
  kit_=kit;voice_=unsigned(p[0]*2.999f);if(voice_>2)voice_=2;
  float accent=velocity*(0.25f+0.75f*p[4]);if(accent<0)accent=0;if(accent>1)accent=1;
  float pitch=hz*powf(2.0f,(p[1]-0.5f)*2.0f);if(pitch<30)pitch=30;if(pitch>500)pitch=500;
  if(kit_==0){
   if(voice_==0){analog_kick_.SetFreq(pitch*0.5f);analog_kick_.SetTone(p[2]);analog_kick_.SetDecay(p[3]);analog_kick_.SetAccent(accent);analog_kick_.Trig();}
   else if(voice_==1){analog_snare_.SetFreq(pitch);analog_snare_.SetTone(p[2]);analog_snare_.SetDecay(p[3]);analog_snare_.SetAccent(accent);analog_snare_.Trig();}
   else{hat_.SetTone(p[2]);hat_.SetDecay(p[3]);hat_.SetAccent(accent);hat_.Trig();}
  }else{
   if(voice_==0){synthetic_kick_.SetFreq(pitch*0.5f);synthetic_kick_.SetTone(p[2]);synthetic_kick_.SetDecay(p[3]);synthetic_kick_.SetAccent(accent);synthetic_kick_.Trig();}
   else if(voice_==1){synthetic_snare_.SetFreq(pitch);synthetic_snare_.SetSnappy(p[2]);synthetic_snare_.SetDecay(p[3]);synthetic_snare_.SetAccent(accent);synthetic_snare_.Trig();}
   else{hat_.SetTone(p[2]);hat_.SetDecay(p[3]);hat_.SetAccent(accent);hat_.Trig();}
  }
 }
 float next(){return kit_==0?(voice_==0?analog_kick_.Process():voice_==1?analog_snare_.Process():hat_.Process()):(voice_==0?synthetic_kick_.Process():voice_==1?synthetic_snare_.Process():hat_.Process());}
};
}
