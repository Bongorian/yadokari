#pragma once
#include <stdint.h>
#include "voice_budget.h"
namespace groove {
// Pi 4 / 1.5 GHz costs per 24 output frames. Authored T1/T3 calibration
// (docs/dsp-efficiency.md) replaces only measured Resonator model2 / Texture mode0 mono q1
// averages, with ceil(observed *1.25). Unmeasured variants retain their costs.
// Burst reservation protects the 192-frame render-ahead from synchronized work.
struct NativeCost {uint16_t average_us=0,burst_us=0;};
struct NativeCostTable {const NativeCost* values=nullptr;uint8_t count=0,qualities=1;};
inline constexpr NativeCost kResonatorCosts[]={{18,48},{33,80},{21,80}};
inline constexpr NativeCost kTextureCosts[]={
 {33,90},{21,81},{33,90},{30,88},
 {32,300},{30,280},{32,300},{30,260},
 {22,75},{21,75},{23,70},{23,60},
 {34,1133},{25,600},{27,1131},{23,600}};
class NativeBudget {
 NativeCost held_[16]{};
public:
 static constexpr unsigned average_limit=VoiceBudget::voice_us,burst_limit=1200;
 bool admit(unsigned track,NativeCost cost){if(track>=16)return false;if(held_[track].average_us==cost.average_us && held_[track].burst_us==cost.burst_us)return true;unsigned average=cost.average_us,burst=cost.burst_us;for(unsigned t=0;t<16;++t)if(t!=track){average+=held_[t].average_us;burst+=held_[t].burst_us;}if(average>average_limit || burst>burst_limit)return false;held_[track]=cost;return true;}
 void release(unsigned track){if(track<16)held_[track]={};}
 void clear(){for(auto& cost:held_)cost={};}
 bool held(unsigned track)const{return track<16 && held_[track].average_us;}
 unsigned count()const{unsigned n=0;for(auto c:held_)n+=c.average_us!=0;return n;}
};
}
