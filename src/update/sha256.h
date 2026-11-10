#pragma once
#include <stdint.h>
#include <stddef.h>
namespace groove { namespace update {
// SHA-256, FIPS 180-4. Streaming, bounded storage; no platform dependencies.
class Sha256 {
 uint32_t h_[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 uint8_t block_[64]{}; unsigned used_=0; uint64_t bytes_=0;
 static uint32_t rotate(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
 void compress(){
  static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t w[64];for(unsigned i=0;i<16;++i)w[i]=(uint32_t(block_[i*4])<<24)|(uint32_t(block_[i*4+1])<<16)|(uint32_t(block_[i*4+2])<<8)|block_[i*4+3];
  for(unsigned i=16;i<64;++i){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}
  uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
  for(unsigned i=0;i<64;++i){uint32_t t=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^((~e)&g))+k[i]+w[i];uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
  h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
 }
public:
 void add(const uint8_t* p,size_t n){bytes_+=n;while(n--){block_[used_++]=*p++;if(used_==64){compress();used_=0;}}}
 void finish(uint8_t out[32]){uint64_t bits=bytes_*8;uint8_t b=0x80;add(&b,1);b=0;while(used_!=56)add(&b,1);uint8_t length[8];for(unsigned i=0;i<8;++i)length[7-i]=uint8_t(bits>>(i*8));add(length,8);for(unsigned i=0;i<32;++i)out[i]=uint8_t(h_[i/4]>>(24-8*(i%4)));}
 static void hash(const uint8_t* p,size_t n,uint8_t out[32]){Sha256 h;h.add(p,n);h.finish(out);}
};
inline bool equal(const uint8_t* a,const uint8_t* b,size_t n){uint8_t diff=0;for(size_t i=0;i<n;++i)diff|=a[i]^b[i];return diff==0;}
inline uint32_t get32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(i*8));}
}}
