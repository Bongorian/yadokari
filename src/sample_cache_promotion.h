#pragma once
#include "wav_sampler.h"
namespace groove {
// Source is already validated and remains untouched if promotion fails.
// Disk/DMA fills source in low memory; high memory is used only by the CPU.
template<class Allocate,class FreeLow,class FreeHigh>
bool promote_sample_cache(uint8_t*& source,unsigned length,WavView& view,Allocate allocate,FreeLow free_low,FreeHigh free_high){
 if(!source || !length)return false;
 auto* high=static_cast<uint8_t*>(allocate(length));if(!high)return false;
 __builtin_memcpy(high,source,length);
 WavView promoted{};if(!parse_wav(high,length,promoted)){free_high(high);return false;}
 free_low(source);source=high;view=promoted;return true;
}
}
