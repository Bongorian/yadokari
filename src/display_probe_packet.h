#pragma once
#include "mk3_output.h"
namespace groove {
// Identical uniform pixels. Modes1/2 have exactly equal bytes and pixel counts,
// but1 versus16 commands. Mode0 changes bytes at the same command count1.
inline unsigned display_probe_packet(unsigned side,unsigned x,unsigned y,unsigned width,uint16_t color,unsigned mode,uint8_t* packet){
 const unsigned pairs=width*16/2;if(side>1 || mode>2 || pairs<=30 || !screen_header(uint8_t(side),{uint16_t(x),uint16_t(y),uint16_t(width),16},packet))return 0;
 unsigned at=16;
 auto command=[&](unsigned op,unsigned n){packet[at++]=uint8_t(op);packet[at++]=uint8_t(n>>16);packet[at++]=uint8_t(n>>8);packet[at++]=uint8_t(n);};
 auto pair=[&](){put_be16(packet+at,color);put_be16(packet+at+2,color);at+=4;};
 if(mode==0){command(1,pairs);pair();}
 else{unsigned literal=mode==1?pairs:pairs-30;command(0,literal);for(unsigned i=0;i<literal;++i)pair();if(mode==2)for(unsigned i=0;i<15;++i){command(1,2);pair();}}
 screen_footer(uint8_t(side),packet+at);return at+8;
}
}
