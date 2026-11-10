#include "rings/dsp/resonator.h"
#undef RINGS_DSP_RESONATOR_H_
#define Resonator ReferenceResonator
#define kMaxModes kReferenceMaxModes
#include "../third_party/mutable/rings/dsp/resonator.h"
#undef kMaxModes
#undef Resonator
#include "resonator_boundary.h"
#include "update/sha256.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <initializer_list>
int main(){
 unsigned cases=0;groove::update::Sha256 hash;
 for(float damping:{0.f,.2f,.5f,.9f,std::nextafter(1.f,0.f)})
 for(float structure:{0.f,.5f,.9995f})for(float brightness:{0.f,.5f,1.f})
 for(float position:{0.f,.5f,1.f})for(float hz:{55.f,440.f,8000.f}){
  auto actual=std::make_unique<rings::Resonator>();auto reference=std::make_unique<rings::ReferenceResonator>();
  std::memset(static_cast<void*>(actual.get()),0,sizeof *actual);std::memset(static_cast<void*>(reference.get()),0,sizeof *reference);actual->Init();reference->Init();
  actual->set_frequency(hz/48000);reference->set_frequency(hz/48000);actual->set_structure(structure);reference->set_structure(structure);actual->set_brightness(brightness);reference->set_brightness(brightness);actual->set_position(position);reference->set_position(position);actual->set_damping(damping);reference->set_damping(damping);
  for(unsigned b=0;b<32;++b){float input[24]{},out[24]{},aux[24]{},old[24]{},old_aux[24]{};if(!b)input[0]=.3f;actual->Process(input,out,aux,24);reference->Process(input,old,old_aux,24);assert(!std::memcmp(out,old,sizeof out) && !std::memcmp(aux,old_aux,sizeof aux));hash.add(reinterpret_cast<uint8_t*>(out),sizeof out);hash.add(reinterpret_cast<uint8_t*>(aux),sizeof aux);}
  ++cases;
 }
 assert(groove::resonator_damping_lookup(1.f)==rings::lut_4_decades[256]);
 for(float hz:{55.f,440.f,8000.f})for(float structure:{0.f,.5f,.9995f}){
  auto actual=std::make_unique<rings::Resonator>();std::memset(static_cast<void*>(actual.get()),0,sizeof *actual);actual->Init();actual->set_frequency(hz/48000);actual->set_structure(structure);actual->set_brightness(1);actual->set_position(1);actual->set_damping(1);
  for(unsigned b=0;b<128;++b){float in[24]{},out[24]{},aux[24]{};if(!b)in[0]=.5f;actual->Process(in,out,aux,24);for(unsigned i=0;i<24;++i)assert(std::isfinite(out[i]) && std::isfinite(aux[i]));}
 }
 uint8_t digest[32];hash.finish(digest);std::printf("RESONATOR %u non-endpoint cases byte-identical to pristine upstream; endpoint defined as lut[256]=%.9g; PCM SHA256 ",cases,double(rings::lut_4_decades[256]));for(auto byte:digest)std::printf("%02x",byte);std::puts("");
}
