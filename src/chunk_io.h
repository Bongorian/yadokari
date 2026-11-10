#pragma once
#include "alloc_policy.h"
#include <stdint.h>
#include <stddef.h>
namespace groove {
inline constexpr uint32_t kMaxFormatBytes=64u<<20;
// Worker-side byte storage. Only initialized [0,size) bytes are exposed;
// large copies can yield to the audio owner through the target copy policy.
struct ChunkBuffer {
 using Copy=bool (*)(void*,void*,const void*,size_t);
 uint8_t* data=nullptr;
 uint32_t size=0;
 uint32_t capacity=0;
 Copy copy=nullptr;void* context=nullptr;
 ~ChunkBuffer(){clear();}
 ChunkBuffer()=default;
 ChunkBuffer(Copy copier,void* ctx):copy(copier),context(ctx){}
 ChunkBuffer(const ChunkBuffer&)=delete;
 ChunkBuffer& operator=(const ChunkBuffer&)=delete;
 void clear(){alloc_delete_array(data);size=0;capacity=0;}
 uint8_t* release(){auto* p=data;data=nullptr;size=capacity=0;return p;}
 bool copy_bytes(void* dst,const void* src,size_t n){if(copy && n>=32768)return copy(context,dst,src,n);if(n)__builtin_memcpy(dst,src,n);return true;}
 bool write(const uint8_t* p,size_t n){
  if(n>kMaxFormatBytes+14u-size)return false;
  uint32_t need=size+uint32_t(n);
  if(need>capacity){
   uint32_t next=capacity?capacity:8192;
   while(next<need){if(next>kMaxFormatBytes/2){next=need;break;}next*=2;}
   uint8_t* grown=alloc_uninitialized_bytes(next);
   if(!grown)return false;
   if(!copy_bytes(grown,data,size)){delete[] grown;return false;}
   alloc_delete_array(data);data=grown;capacity=next;
  }
  if(n && !copy_bytes(data+size,p,n))return false;
  size=need;return true;
 }
 void patch_u32(uint32_t at,uint32_t value){for(unsigned i=0;i<4;++i)data[at+i]=uint8_t(value>>(8*i));}
 bool begin_frame(uint32_t kind,uint16_t version,uint32_t& at,uint16_t flags=0){
  at=size;uint8_t header[12]={uint8_t(kind),uint8_t(kind>>8),uint8_t(kind>>16),uint8_t(kind>>24),uint8_t(version),uint8_t(version>>8),uint8_t(flags),uint8_t(flags>>8),0,0,0,0};return write(header,sizeof header);
 }
 bool end_frame(uint32_t at){if(at>size || size-at<12)return false;patch_u32(at+8,size-at-12);return true;}
};
struct ChunkSource {
 const uint8_t* data=nullptr;
 uint32_t size=0;
 uint32_t pos=0;
 bool read(uint8_t* p,size_t n){
  if(n>size-pos)return false;
  if(n)__builtin_memcpy(p,data+pos,n);
  pos+=uint32_t(n);return true;
 }
 bool at_end()const{return pos>=size;}
 uint32_t remaining()const{return size-pos;}
 // Zero-copy sub-view over the next n bytes; advances this source.
 bool slice(uint32_t n,ChunkSource& out){
  if(n>remaining())return false;
  out.data=data+pos;out.size=n;out.pos=0;pos+=n;return true;
 }
};
// Plain (no CRC) field writer/reader for inside chunk payloads. The field
// helpers above (write_sample_edit, write_drum_kit, extended steps and
// automation) work unchanged over these.
template<class Sink> class ChunkWriter {
public:
 explicit ChunkWriter(Sink& s):s_(s){}
 bool raw(uint8_t x){return s_.write(&x,1);}
 bool u8(uint8_t x){return raw(x);}
 bool u16(uint16_t x){uint8_t b[2]={uint8_t(x),uint8_t(x>>8)};return s_.write(b,2);}
 bool u32(uint32_t x){uint8_t b[4]={uint8_t(x),uint8_t(x>>8),uint8_t(x>>16),uint8_t(x>>24)};return s_.write(b,4);}
 bool f32(float x){uint32_t bits;__builtin_memcpy(&bits,&x,4);return u32(bits);}
 bool block(const uint8_t* p,uint32_t n){return s_.write(p,n);}
private: Sink& s_;
};
template<class Source> class ChunkReader {
public:
 explicit ChunkReader(Source& s):s_(s){}
 bool raw(uint8_t& x){return s_.read(&x,1);}
 bool u8(uint8_t& x){return raw(x);}
 bool u16(uint16_t& x){uint8_t b[2];if(!s_.read(b,2))return false;x=uint16_t(b[0])|(uint16_t(b[1])<<8);return true;}
 bool u32(uint32_t& x){uint8_t b[4];if(!s_.read(b,4))return false;x=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;}
 bool f32(float& x){uint32_t bits;if(!u32(bits))return false;__builtin_memcpy(&x,&bits,4);return true;}
 bool block(uint8_t* p,uint32_t n){return s_.read(p,n);}
 bool at_end(){return s_.at_end();}
 // Zero-copy payload slice (ChunkSource only).
 bool sliced(uint32_t n,ChunkSource& out){return s_.slice(n,out);}
private: Source& s_;
};
}
