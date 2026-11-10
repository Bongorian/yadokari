#pragma once
#include <stddef.h>
#include <stdint.h>
namespace groove {
// Count Output bits for one HID report ID. Descriptor bytes are untrusted;
// reject truncated items, unbalanced global state and unreasonable sizes.
inline unsigned hid_report_bytes(const uint8_t* data,size_t length,uint8_t wanted,unsigned main_tag){
 if(!data || !wanted)return 0;
 struct Globals{uint32_t size,count,id;};Globals g{},stack[8]{};unsigned depth=0;uint64_t bits=0;
 for(size_t at=0;at<length;){uint8_t prefix=data[at++];if(prefix==0xfe){if(length-at<2)return 0;unsigned n=data[at];at+=2;if(n>length-at)return 0;at+=n;continue;}
  unsigned n=prefix&3;if(n==3)n=4;if(n>length-at)return 0;uint32_t value=0;for(unsigned i=0;i<n;++i)value|=uint32_t(data[at+i])<<(8*i);at+=n;unsigned type=(prefix>>2)&3,tag=prefix>>4;
  if(type==1){if(tag==7)g.size=value;else if(tag==8){if(!value || value>255)return 0;g.id=value;}else if(tag==9)g.count=value;else if(tag==10){if(depth==8)return 0;stack[depth++]=g;}else if(tag==11){if(!depth)return 0;g=stack[--depth];}}
  else if(type==0 && tag==main_tag && g.id==wanted){uint64_t field=uint64_t(g.size)*g.count;if(field>65528-bits)return 0;bits+=field;}
 }
 if(depth || !bits || (bits&7))return 0;
 return unsigned(bits/8)+1;
}
inline unsigned hid_output_report_bytes(const uint8_t* data,size_t length,uint8_t wanted){return hid_report_bytes(data,length,wanted,9);}
inline unsigned hid_input_report_bytes(const uint8_t* data,size_t length,uint8_t wanted){return hid_report_bytes(data,length,wanted,8);}
// Wire framing from encdr's MK3 descriptor. USB transfer boundaries remain unverified.
struct ScreenRect { uint16_t x,y,w,h; };
inline void put_be16(uint8_t* p, uint16_t v) { p[0]=uint8_t(v>>8); p[1]=uint8_t(v); }
inline bool screen_header(uint8_t screen, ScreenRect r, uint8_t out[20]) {
 if(!out || screen>1 || !r.w || !r.h || (r.x%4) || (r.w%4) || (r.y%2) || (r.h%2) || unsigned(r.x)+r.w>480 || unsigned(r.y)+r.h>272) return false;
 const uint8_t initial[20]={0x84,0,screen,0x60,0,0,0,0};
 for(unsigned i=0;i<20;++i) out[i]=initial[i];
 put_be16(out+8,r.x); put_be16(out+10,r.y); put_be16(out+12,r.w); put_be16(out+14,r.h); put_be16(out+18,uint16_t(unsigned(r.w)*r.h/2)); return true;
}
inline bool screen_footer(uint8_t screen,uint8_t out[8]) {
 if(!out || screen>1) return false;
 const uint8_t f[8]={3,0,0,0,0x40,0,screen,0}; for(unsigned i=0;i<8;++i) out[i]=f[i]; return true;
}
inline void rgb565_be(uint8_t r,uint8_t g,uint8_t b,uint8_t out[2]) {
 uint16_t v=uint16_t((uint16_t(r>>3)<<11)|(uint16_t(g>>2)<<5)|(b>>3)); put_be16(out,v);
}
// LED values use the hardware palette. out includes report ID; offsets are payload-relative.
inline void pad_led_report(const uint8_t strip[25],const uint8_t pads[16],uint8_t out[81]) {
 for(unsigned i=0;i<81;++i) out[i]=0;
 out[0]=0x81;
 for(unsigned i=0;i<25;++i) out[1+i]=strip[i];
 for(unsigned i=0;i<16;++i) out[26+i]=pads[i];
}
}
