#pragma once
#include "parameters.h"
#include "chunk_io.h"
namespace groove {
inline constexpr uint32_t kChunkPfxs=0x53584650u; // root or leaf FX snapshot v1
struct PresetFxSnapshot {
 bool present=false;float inserts[8]={0,.5f,.25f,1,0,.5f,.25f,1};float delay=0,reverb=0;uint8_t disabled=0;
 template<class T> void capture(const T& t){present=true;for(unsigned i=0;i<8;++i)inserts[i]=t.insert_fx[i];delay=t.delay_send;reverb=t.reverb_send;disabled=t.insert_disabled;}
 template<class T> void apply(T t)const{if(!present)return;for(unsigned i=0;i<8;++i)t.insert_fx[i]=inserts[i];t.delay_send=delay;t.reverb_send=reverb;t.insert_disabled=disabled;}
 bool valid()const{if(!present)return true;if(disabled>3 || !(delay>=0 && delay<=1 && reverb>=0 && reverb<=1))return false;for(unsigned i=0;i<8;++i)if(!valid_parameter(uint16_t(94+i),inserts[i]) || (i%4==0 && floorf(inserts[i])!=inserts[i]))return false;return true;}
};
inline bool write_fx_frame(ChunkBuffer& out,const PresetFxSnapshot& fx){if(!fx.present)return true;uint32_t frame;ChunkWriter<ChunkBuffer> w(out);if(!fx.valid() || !out.begin_frame(kChunkPfxs,1,frame))return false;for(float v:fx.inserts)if(!w.f32(v))return false;return w.f32(fx.delay) && w.f32(fx.reverb) && w.u8(fx.disabled) && out.end_frame(frame);}
inline bool read_fx_frame(ChunkSource& src,PresetFxSnapshot& fx){ChunkReader<ChunkSource> r(src);fx.present=true;for(float& v:fx.inserts)if(!r.f32(v))return false;return r.f32(fx.delay) && r.f32(fx.reverb) && r.u8(fx.disabled) && src.at_end() && fx.valid();}
}
