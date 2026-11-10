#pragma once
#include <stdint.h>
namespace groove { namespace update {
// A held pad can repeat HIT reports. Firmware actions require a new press;
// handoff carries physical held state from the performance controller.
class PadLatch {
 uint16_t held_=0;
public:
 void preset(uint16_t mask){held_=mask;}
 bool press(unsigned pad,uint8_t tag){if(pad>=16)return false;uint16_t bit=uint16_t(1u<<pad);if(tag==0x20||tag==0x30){held_&=uint16_t(~bit);return false;}if(tag!=0x10||(held_&bit))return false;held_|=bit;return true;}
};
// MK3's upper buttons occupy byte 9 bits 0-6 and byte 1 bit 7.
class ButtonLatch {
 uint8_t first_=0,last_=0;
public:
 void reset(){first_=last_=0;}
 uint8_t update(unsigned byte,uint8_t value,bool baseline){
  uint8_t* old=byte==9?&first_:byte==1?&last_:nullptr;
  if(!old)return 0;
  uint8_t rising=baseline?0:uint8_t(value & uint8_t(~*old));*old=value;
  return byte==9?uint8_t(rising&0x7f):uint8_t(rising&0x80);
 }
};
}}
