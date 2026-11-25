#pragma once
#include "mk3_output.h"
#include "ui/framebuffer565.h"
namespace groove {
// MK3 command 01 repeats one RGB565 pixel pair; command 00 copies pairs.
// Protocol facts: cabl 1e415dff, doc/hardware/maschine-mk3/MaschineMK3-Display.md.
// Original encoder, no external implementation. Zero means use the raw fallback.
inline unsigned pack_display_pairs(const uint32_t* pixels,unsigned y,unsigned left,unsigned width,unsigned rows,ui::PixelOrder order,uint8_t* packet,unsigned limit){
 // Splitting an open literal for a short run adds device commands on dense
 // transitions. Four pairs suffice where there is no literal to split.
 constexpr unsigned kStandaloneRun=4,kSplitLiteralRun=8;
 unsigned at=16,literal=0,literals=0;
 auto command=[&](unsigned where,unsigned op,unsigned count){
  packet[where]=uint8_t(op);packet[where+1]=uint8_t(count>>16);
  put_be16(packet+where+2,uint16_t(count));
 };
 auto flush_literal=[&](){if(literals){command(literal,0,literals);literals=0;}};
 auto write_pair=[&](uint32_t value){
  put_be16(packet+at,uint16_t(value>>16));put_be16(packet+at+2,uint16_t(value));at+=4;
 };
 auto emit=[&](uint32_t value,unsigned count){
  if(count>=(literals?kSplitLiteralRun:kStandaloneRun)){
   if(at+8>limit)return false;
   flush_literal();command(at,1,count);at+=4;write_pair(value);
  }else{
   if(at+4*count+(literals?0:4)>limit)return false;
   if(!literals){literal=at;at+=4;}
   literals+=count;
   for(unsigned i=0;i<count;++i)write_pair(value);
  }
  return true;
 };
 // Walk the original stride once. A run can cross a rectangle row boundary.
 uint32_t previous=0;unsigned count=0;
 for(unsigned row=0;row<rows;++row){
  const uint32_t* line=pixels+(y+row)*480+left;
  for(unsigned x=0;x<width;x+=2){
   const uint32_t pair=(uint32_t(ui::pack565(line[x],order))<<16)|ui::pack565(line[x+1],order);
   if(count && pair!=previous){
    if(!emit(previous,count))return 0;
    count=0;
   }
   previous=pair;++count;
  }
 }
 if(count && !emit(previous,count))return 0;
 flush_literal();
 return at;
}
}
