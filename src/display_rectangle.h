#pragma once
#include "mk3_output.h"
#include "display_rle.h"
#include "ui/framebuffer565.h"
namespace groove {
constexpr unsigned kDisplayPacketCapacity=480*64*2+28;
struct PreparedDisplayRect {
 bool valid=false,dirty=false,last=false;
 unsigned side=0,first=0,y=0,rows=0,left=0,width=0,bytes=0;
};
// Immutable per-frame differences. Worker-owned; remove a band only after a
// successful transport acknowledgement. No hash collision can hide a pixel.
struct DisplayScanPlan {
 int left[17]{},right[17]{};
 void scan(const uint32_t* pixels,const uint16_t* shadow,const bool valid[17],ui::PixelOrder order){
  for(unsigned band=0;band<17;++band){left[band]=480;right[band]=-1;
   for(unsigned y=band*16;y<(band+1)*16;++y)for(unsigned x=0;x<480;++x)
    if(!valid[band] || ui::pack565(pixels[y*480+x],order)!=shadow[y*480+x]){
     if(int(x)<left[band])left[band]=int(x);
     if(int(x)>right[band])right[band]=int(x);
    }
  }
 }
 void acknowledged(unsigned first,unsigned rows){if(first>=17 || rows%16 || rows/16>17-first)return;for(unsigned b=first;b<first+rows/16;++b){left[b]=480;right[b]=-1;}}
};
// Pure preparation: the caller owns immutable pixels/shadow and the packet.
// The return contains only bounded aligned rectangles accepted by the MK3.
inline PreparedDisplayRect prepare_display_rectangle(unsigned side,const uint32_t* pixels,const uint16_t* shadow,const bool valid[17],unsigned cursor,ui::PixelOrder order,uint8_t* packet,unsigned max_rows=32,bool compact=false,const DisplayScanPlan* plan=nullptr){
 if(!compact && max_rows>64)max_rows=64;
 PreparedDisplayRect out{};if(side>1 || !pixels || !shadow || !valid || !packet || cursor>=17)return out;out.valid=true;out.side=side;
 for(unsigned scanned=0;scanned<17;++scanned){unsigned first=(cursor+scanned)%17,y=first*16;int left=plan?plan->left[first]:480,right=plan?plan->right[first]:-1;
  if(!plan)for(unsigned j=y;j<y+16;++j)for(unsigned x=0;x<480;++x)if(!valid[first] || ui::pack565(pixels[j*480+x],order)!=shadow[j*480+x]){if(int(x)<left)left=int(x);if(int(x)>right)right=int(x);}
  if(right<left)continue;
  unsigned rows=16;
  while(rows+16<=max_rows && y+rows+16<=272){int next_left=plan?plan->left[first+rows/16]:480,next_right=plan?plan->right[first+rows/16]:-1;
   if(!plan)for(unsigned j=y+rows;j<y+rows+16;++j)for(unsigned x=0;x<480;++x)if(!valid[first+rows/16] || ui::pack565(pixels[j*480+x],order)!=shadow[j*480+x]){if(int(x)<next_left)next_left=int(x);if(int(x)>next_right)next_right=int(x);}
   if(next_right>=next_left){int merged_left=left<next_left?left:next_left,merged_right=right>next_right?right:next_right;
    unsigned combined=unsigned(((merged_right+4)&~3)-(merged_left&~3))*(rows+16)*2+28;
    unsigned separate=unsigned(((right+4)&~3)-(left&~3))*rows*2+unsigned(((next_right+4)&~3)-(next_left&~3))*32+56;
    if(combined<=separate+(max_rows>32?4096:0)){rows+=16;left=merged_left;right=merged_right;}else break;
   }else break;
  }
  left&=~3;right=(right+4)&~3;unsigned width=unsigned(right-left);
  unsigned bytes=0;
  for(;;){
   if(!screen_header(uint8_t(side),{uint16_t(left),uint16_t(y),uint16_t(width),uint16_t(rows)},packet)){out.valid=false;return out;}
   const unsigned raw_end=20+width*rows*2,limit=raw_end<kDisplayPacketCapacity-8?raw_end:kDisplayPacketCapacity-8;
   bytes=compact?pack_display_pairs(pixels,y,unsigned(left),width,rows,order,packet,limit):0;
   if(bytes && bytes<raw_end)break;
   if(raw_end+8<=kDisplayPacketCapacity){screen_header(uint8_t(side),{uint16_t(left),uint16_t(y),uint16_t(width),uint16_t(rows)},packet);for(unsigned j=0;j<rows;++j)for(unsigned x=0;x<width;++x)put_be16(packet+20+(j*width+x)*2,ui::pack565(pixels[(y+j)*480+unsigned(left)+x],order));bytes=raw_end;break;}
   // An incompressible large rectangle retries fewer bands. No packet grows.
   rows=((rows/16+1)/2)*16;
  }
  screen_footer(uint8_t(side),packet+bytes);
  // Completion is known before submission: all differences outside this
  // rectangle must already match the device shadow. This avoids a redundant
  // clean scan after the last USB acknowledgement (possibly in the next slot).
  out.last=true;
  for(unsigned band=0;band<17 && out.last;++band){
   if(band>=first && band<first+rows/16)continue;
   if(plan){if(plan->right[band]>=plan->left[band])out.last=false;continue;}
   if(!valid[band]){out.last=false;break;}
   for(unsigned j=band*16;j<(band+1)*16 && out.last;++j)
    for(unsigned x=0;x<480;++x)if(ui::pack565(pixels[j*480+x],order)!=shadow[j*480+x]){out.last=false;break;}
  }
  out.dirty=true;out.first=first;out.y=y;out.rows=rows;out.left=unsigned(left);out.width=width;out.bytes=bytes+8;return out;
 }
 return out;
}
}
