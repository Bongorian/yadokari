#pragma once
#include "sample_state.h"
namespace groove {
// Group ownership changes only when membership masks change. The lowest
// numbered group wins, matching the original per-callback scan.
class GroupRoutingCache {
 public:
  const uint8_t* resolve(const GroupBus* groups){
   bool changed=!valid_;for(unsigned g=0;g<8;++g)if(masks_[g]!=groups[g].members)changed=true;
   if(changed){for(unsigned g=0;g<8;++g)masks_[g]=groups[g].members;for(unsigned t=0;t<16;++t){assign_[t]=8;for(unsigned g=0;g<8;++g)if(masks_[g]&(1u<<t)){assign_[t]=uint8_t(g);break;}}valid_=true;}
   return assign_;
  }
 private:
  uint16_t masks_[8]{};uint8_t assign_[16]{};bool valid_=false;
};
}
