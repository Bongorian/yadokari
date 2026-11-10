#pragma once
#include <stdint.h>
#include "layout.h"
namespace groove { namespace ui {
// Fixed semantic tokens, generated without colour adjustment from themes9.json.
// Compatibility aliases retain token meaning, not any retired palette.
struct Theme {
 bool monochrome=false;
#include "generated/themes9.inc"
};
struct Geometry {
 static constexpr int indicator=2,meter=6,cell=0,button=0,panel=0,card=0,dialog=0,border=1,gap=6,padding=10;
 static constexpr int soft_y=2,soft_h=18,header_y=24,body_y=64,body_h=Layout::body_h,knob_y=186,knob_h=52;
 static constexpr unsigned transition_ms=120,highlight_ms=160,flash_ms=90;
};
inline uint32_t mix(uint32_t a,uint32_t b,unsigned amount){if(amount>255)amount=255;unsigned inv=255-amount;return 0xff000000u|((((a>>16&255)*inv+(b>>16&255)*amount)/255)<<16)|((((a>>8&255)*inv+(b>>8&255)*amount)/255)<<8)|(((a&255)*inv+(b&255)*amount)/255);}
inline float clamp01(float v){return v<0?0:v>1?1:v;}
}}
