#pragma once
#include <stdint.h>
namespace groove {
// Bounded tempo follower, not a phase-locked clock. Times are audio samples.
class MidiClockEstimator {
public:
 void reset(){last_=sum_=0;count_=cursor_=0;have_last_=false;for(auto& x:intervals_)x=0;}
 // ticks: clock pulses that arrived together at this sample. A late delivery
 // hands over several at once; they share the time since the previous arrival.
 bool observe(uint64_t now,unsigned rate,unsigned& tempo,unsigned ticks=1){
  if(!rate || !ticks)return false;
  if(!have_last_){last_=now;have_last_=true;return false;}
  if(now<=last_)return false;
  uint64_t delta=(now-last_+ticks/2)/ticks,unit=uint64_t(rate)*60;
  uint64_t minimum=unit/(24*300),maximum=(unit+24*20-1)/(24*20);
  if(delta<minimum)return false; // Ignore spurious early pulses without moving the anchor.
  if(delta>maximum){reset();last_=now;have_last_=true;return false;}
  last_=now;if(count_==8)sum_-=intervals_[cursor_];else ++count_;
  intervals_[cursor_]=delta;sum_+=delta;cursor_=uint8_t((cursor_+1)%8);
  uint64_t divisor=sum_*24;unsigned result=unsigned((unit*count_+divisor/2)/divisor);
  tempo=result<20?20:result>300?300:result;return true;
 }
 // Unrounded estimate as tempo x 256; 0 until an interval has been observed.
 unsigned tempo_q8(unsigned rate)const{if(!rate || !count_ || !sum_)return 0;uint64_t unit=uint64_t(rate)*60,divisor=sum_*24;return unsigned((unit*count_*256+divisor/2)/divisor);}
private:
 uint64_t last_=0,sum_=0,intervals_[8]{};uint8_t count_=0,cursor_=0;bool have_last_=false;
};
}
