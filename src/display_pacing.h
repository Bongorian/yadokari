#pragma once
#include <stdint.h>
namespace groove {
// One screen per task pass. Alternating at half the previous paired interval
// preserves per-screen pacing while returning to input/MIDI between transfers.
class DisplayPacing {
 public:
  bool next(uint32_t now,uint32_t interval,bool enabled,unsigned& side){
   if(!enabled || uint32_t(now-last_)<interval)return false;
   last_=now;side=side_;side_^=1;return true;
  }
 private:
  uint32_t last_=0;unsigned side_=0;
};
}
