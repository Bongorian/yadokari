#pragma once
// PolyBLEP saw adapted from DaisySP Source/Synthesis/oscillator.cpp,
// Copyright (c) 2020 Electrosmith, Corp.; MIT license in third_party/daisysp/LICENSE.
namespace groove {
class PolyBlepSaw {
public:
 void init(float sample_rate) { rate_=sample_rate>0?sample_rate:48000; phase_=0; set_frequency(440); }
 void set_frequency(float frequency) { if(frequency<0) frequency=0; if(frequency>rate_*0.49f) frequency=rate_*0.49f; increment_=frequency/rate_; }
 void reset() { phase_=0; }
 float process() {
  float out=(2.0f*phase_)-1.0f;
  const float dt=increment_; float correction=0;
  if(dt>0 && phase_<dt) { float t=phase_/dt; correction=t+t-t*t-1.0f; }
  else if(dt>0 && phase_>1.0f-dt) { float t=(phase_-1.0f)/dt; correction=t*t+t+t+1.0f; }
  out=-(out-correction); phase_+=dt; if(phase_>=1.0f) phase_-=1.0f; return out;
 }
private: float rate_=48000,phase_=0,increment_=0;
};
}
