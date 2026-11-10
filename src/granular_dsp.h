#pragma once
#include <stdint.h>
#include "texture_dsp.h"
namespace groove {
// Native 32 kHz / 32-frame DSP boundary. Caller owns rate conversion/routing.
class GranularDsp {
public:
 static constexpr unsigned kBlock=32,kLargeBytes=69760,kSmallBytes=16384;
 static unsigned allocation_bytes();
 GranularDsp();~GranularDsp();
 GranularDsp(const GranularDsp&)=delete;GranularDsp& operator=(const GranularDsp&)=delete;
 bool ready()const{return impl_!=nullptr;}
 void init(uint32_t seed=1);
 // p: mode, position, size, pitch, density, texture, blend, spread,
 // feedback, reverb, freeze, quality, input, trigger, gate.
 void process(const float* p,const TextureFrame* input,TextureFrame* output,bool strum=false);
 bool frozen()const;
private:
 struct Impl;Impl* impl_=nullptr;uint32_t random_=1;bool freeze_=false,trigger_=false;
};
}
