#pragma once
#include <stdint.h>
namespace groove {
#include "generated/boot_splash.inc"
inline const uint8_t* boot_splash_packet(unsigned side,unsigned& size){size=side?sizeof kBootSplashRight:sizeof kBootSplashLeft;return side?kBootSplashRight:kBootSplashLeft;}
// Shadow is committed only after the full packet's USB acknowledgement.
inline bool boot_splash_shadow(unsigned side,uint16_t* shadow){
 if(side>1 || !shadow)return false;
 unsigned length=0;const uint8_t* p=boot_splash_packet(side,length);unsigned at=16,pixels=0;
 while(at+4<=length-8){unsigned op=p[at],count=(unsigned(p[at+1])<<16)|(unsigned(p[at+2])<<8)|p[at+3];at+=4;
  if(op>1 || !count || count>(480*272-pixels)/2 || at+(op?4:count*4)>length-8)return false;
  for(unsigned i=0;i<count;++i){unsigned src=at+(op?0:i*4);shadow[pixels++]=uint16_t((unsigned(p[src])<<8)|p[src+1]);shadow[pixels++]=uint16_t((unsigned(p[src+2])<<8)|p[src+3]);}
  at+=op?4:count*4;
 }
 return at==length-8 && pixels==480*272;
}
}
