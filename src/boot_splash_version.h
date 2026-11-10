#pragma once
#include "mk3_output.h"
#include "ui/font_renderer.h"
#include "ui/framebuffer565.h"

namespace groove {
// Public release plan: first public 0.9.0 = 900, 1.0.0 = 10000.
// Earlier private development versions occupy the raw range below 900.
inline void boot_splash_version(uint32_t version,char (&text)[24]) {
 char* p=text;*p++='v';
 auto decimal=[&](uint32_t value){char digits[10];unsigned n=0;do{digits[n++]=char('0'+value%10);value/=10;}while(value);while(n)*p++=digits[--n];};
 if(version<900)decimal(version);
 else{decimal(version/10000);*p++='.';decimal((version/100)%100);*p++='.';decimal(version%100);}
 *p=0;
}
// Only this bottom-centre strip is transmitted. The artwork is never redrawn.
constexpr ScreenRect kBootVersionRect{176,240,128,32};
constexpr unsigned kBootVersionPacketBytes=20+kBootVersionRect.w*kBootVersionRect.h*2+8;
constexpr unsigned kBootVersionGray=168; // Flat tagline colour in the approved PNG.
inline bool boot_splash_version_packet(uint32_t version,const uint16_t* shadow,uint8_t* packet,unsigned capacity,unsigned& bytes) {
 bytes=0;if(!shadow || !packet || capacity<kBootVersionPacketBytes)return false;
 const auto r=kBootVersionRect;
 if(!screen_header(1,r,packet))return false;
 for(unsigned y=0;y<r.h;++y)for(unsigned x=0;x<r.w;++x)put_be16(packet+20+(y*r.w+x)*2,shadow[(r.y+y)*480+r.x+x]);
 char text[24];boot_splash_version(version,text);
 auto pixel=[&](int x,int y,unsigned alpha){
  if(x<r.x || x>=r.x+r.w || y<r.y || y>=r.y+r.h)return;
  uint8_t* out=packet+20+((unsigned(y)-r.y)*r.w+unsigned(x)-r.x)*2;
  uint32_t base=ui::expand565(uint16_t((unsigned(out[0])<<8)|out[1])),color=0;
  for(unsigned shift=0;shift<24;shift+=8)color|=((((base>>shift)&255)*(255-alpha)+kBootVersionGray*alpha+127)/255)<<shift;
  put_be16(out,ui::pack565(color));
 };
 ui::raster_text(pixel,(480-ui::text_width(text,ui::FontRole::Small))/2,244,text,ui::FontRole::Small,r.w);
 screen_footer(1,packet+kBootVersionPacketBytes-8);bytes=kBootVersionPacketBytes;return true;
}
}
