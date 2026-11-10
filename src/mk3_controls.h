#pragma once
#include <stdint.h>
namespace groove {
// Device-specific counter range reported by libmk3 at ca3627a. The range
// remains a fixture assumption until the user's controller is captured.
inline bool mk3_rotary_delta(uint16_t previous,uint16_t current,int16_t& delta) {
 if(previous>=1000 || current>=1000)return false;
 int value=int(current)-int(previous);if(value>500)value-=1000;else if(value< -500)value+=1000;
 delta=int16_t(value);return true;
}
inline int16_t mk3_main_delta(uint8_t previous,uint8_t current){return int16_t(int((current-previous+8)&15)-8);}
}
