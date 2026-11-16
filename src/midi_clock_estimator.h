#pragma once
#include <stdint.h>
namespace groove {
// Tempo follower for the external clock. Times are audio samples. The
// integer observe() is the original bounded follower; observe_q16() exposes
// the same interval average in Q16 BPM for the phase-locked slave.
class MidiClockEstimator {
public:
 void reset(){last_=sum_=0;count_=cursor_=0;have_last_=false;for(auto& x:intervals_)x=0;}
 bool observe(uint64_t now,unsigned rate,unsigned& tempo){
  uint64_t unit=0,divisor=0;if(!step(now,rate,unit,divisor))return false;
  unsigned result=unsigned((unit*count_+divisor/2)/divisor);
  tempo=result<20?20:result>300?300:result;return true;
 }
 bool observe_q16(uint64_t now,unsigned rate,uint32_t& tempo_q16){
  uint64_t unit=0,divisor=0;if(!step(now,rate,unit,divisor))return false;
  uint64_t result=(unit*count_*65536+divisor/2)/divisor;
  tempo_q16=result<(20u<<16)?(20u<<16):result>(300u<<16)?(300u<<16):uint32_t(result);return true;
 }
private:
 bool step(uint64_t now,unsigned rate,uint64_t& unit,uint64_t& divisor){
  if(!rate)return false;
  if(!have_last_){last_=now;have_last_=true;return false;}
  if(now<=last_)return false;
  uint64_t delta=now-last_;unit=uint64_t(rate)*60;
  uint64_t minimum=unit/(24*300),maximum=(unit+24*20-1)/(24*20);
  if(delta<minimum)return false; // Ignore spurious early pulses without moving the anchor.
  if(delta>maximum){reset();last_=now;have_last_=true;return false;}
  last_=now;if(count_==8)sum_-=intervals_[cursor_];else ++count_;
  intervals_[cursor_]=delta;sum_+=delta;cursor_=uint8_t((cursor_+1)%8);
  divisor=sum_*24;return true;
 }
 uint64_t last_=0,sum_=0,intervals_[8]{};uint8_t count_=0,cursor_=0;bool have_last_=false;
};
}
