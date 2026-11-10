#pragma once
#include <stdint.h>
namespace groove {
inline float sanitize_audio(float v) { if(v!=v) return 0; if(v>1) return 1; if(v< -1) return -1; return v; }
inline int16_t pcm16(float v) { return int16_t(sanitize_audio(v)*32767.0f); }
inline void pcm24le(float v,uint8_t* out) { uint32_t value=uint32_t(int32_t(sanitize_audio(v)*8388607.0f)); out[0]=uint8_t(value); out[1]=uint8_t(value>>8); out[2]=uint8_t(value>>16); }
inline float pcm24le_read(const uint8_t* p) { uint32_t n=uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16; int32_t s=n&0x800000u ? int32_t(n)-0x1000000 : int32_t(n); return float(s)/8388608.0f; }
// This MK3's 24-bit capture arrives right-justified in a four-byte USB slot.
// Circle keeps the discarded wire byte in internal byte 3 for this adapter.
inline float mk3_pcm24_capture(const uint8_t* converted) { const uint8_t right[]={converted[3],converted[0],converted[1]};return pcm24le_read(right); }
}
