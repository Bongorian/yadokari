#pragma once
#include <stdint.h>
namespace groove { namespace ui {
enum class PixelOrder:uint8_t { RGB, BGR };
inline uint16_t pack565(uint32_t rgb,PixelOrder order=PixelOrder::RGB){unsigned r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;if(order==PixelOrder::BGR){unsigned tmp=r;r=b;b=tmp;}return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));}
inline uint32_t expand565(uint16_t value,PixelOrder order=PixelOrder::RGB){unsigned r=(value>>11)&31,g=(value>>5)&63,b=value&31;if(order==PixelOrder::BGR){unsigned tmp=r;r=b;b=tmp;}return 0xff000000|((r<<3|r>>2)<<16)|((g<<2|g>>4)<<8)|(b<<3|b>>2);}
inline uint32_t preview565(uint32_t color,PixelOrder order=PixelOrder::RGB){return expand565(pack565(color,order),order);}
}}
