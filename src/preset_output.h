#pragma once
#include "chunk_io.h"
namespace groove {
// Root preset gain, independent of optional FX. Unity is omitted on disk and
// bypassed in DSP so pre-trim MGXC13 files retain their exact bytes and sound.
inline constexpr uint32_t kChunkPtrm=0x4d525450u; // PTRM v1: one little-endian f32
inline constexpr float kPresetOutputTrimMin=.125f,kPresetOutputTrimMax=4.f;
inline bool valid_output_trim(float gain){return gain>=kPresetOutputTrimMin && gain<=kPresetOutputTrimMax;}
__attribute__((noinline)) inline bool write_output_trim_frame(ChunkBuffer& out,float gain){
 if(!valid_output_trim(gain))return false;
 if(gain==1)return true;
 uint32_t frame;ChunkWriter<ChunkBuffer> w(out);
 return out.begin_frame(kChunkPtrm,1,frame) && w.f32(gain) && out.end_frame(frame);
}
__attribute__((noinline)) inline bool read_output_trim_frame(ChunkSource& src,float& gain){
 ChunkReader<ChunkSource> r(src);return r.f32(gain) && src.at_end() && valid_output_trim(gain);
}
}
