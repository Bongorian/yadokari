#pragma once
#include "mk3_output.h"
#include "ui/framebuffer565.h"
namespace groove {
// MK3 command 01 repeats one RGB565 pixel pair; command 00 copies pairs.
// Protocol facts: cabl 1e415dff, doc/hardware/maschine-mk3/MaschineMK3-Display.md.
// Original encoder, no external implementation. Zero means use the raw fallback.
inline unsigned pack_display_pairs(const uint32_t* pixels,unsigned y,unsigned left,unsigned width,unsigned rows,ui::PixelOrder order,uint8_t* packet,unsigned limit){
 const unsigned pairs=width*rows/2;unsigned at=16,literal=~0u,literals=0;
 auto pair=[&](unsigned i){unsigned p=i*2,index=(y+p/width)*480+left+p%width;return (uint32_t(ui::pack565(pixels[index],order))<<16)|ui::pack565(pixels[index+1],order);};
 auto command=[&](unsigned where,unsigned op,unsigned count){packet[where]=uint8_t(op);packet[where+1]=uint8_t(count>>16);packet[where+2]=uint8_t(count>>8);packet[where+3]=uint8_t(count);};
 auto word=[&](unsigned where,uint32_t value){put_be16(packet+where,uint16_t(value>>16));put_be16(packet+where+2,uint16_t(value));};
 for(unsigned i=0;i<pairs;){uint32_t value=pair(i);unsigned count=1;while(i+count<pairs && pair(i+count)==value)++count;
  // Tiny runs save few bytes but add device-side commands. Keep them in the
  // adjacent literal; eight pairs balances command count against wire bytes.
  if(count>=8){
   if(literal!=~0u){command(literal,0,literals);literal=~0u;literals=0;}
   if(at+8>limit)return 0;
   command(at,1,count);word(at+4,value);at+=8;i+=count;
  }else{
   if(literal==~0u){if(at+4>limit)return 0;literal=at;at+=4;}
   if(at+4>limit)return 0;
   word(at,value);at+=4;++literals;++i;
  }
 }
 if(literal!=~0u)command(literal,0,literals);
 return at;
}
}
