#pragma once
#include "wav_sampler.h"
namespace groove {
// Converted two-frame PCM from the same YWT1 import path as external assets.
inline WavView dsp_wavetable_fixture(){
 static constexpr int16_t pcm[]={
#include "generated/wavetable_probe.inc"
 };
 return {reinterpret_cast<const uint8_t*>(pcm),sizeof(pcm)/sizeof(*pcm),48000,1,16,1};
}
}
