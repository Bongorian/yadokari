#pragma once
#include <stdint.h>
namespace groove {
struct TextureFrame {int16_t l,r;};
// Native 32 kHz / 32-frame DSP boundary. Caller owns rate conversion/routing.
class TextureDsp {
public:
 static constexpr unsigned kBlock=32,kLargeBytes=118784,kSmallBytes=65536-128;
 static unsigned allocation_bytes();
 TextureDsp();~TextureDsp();
 TextureDsp(const TextureDsp&)=delete;TextureDsp& operator=(const TextureDsp&)=delete;
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
