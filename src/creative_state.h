#pragma once
#include <stdint.h>
namespace groove {
// Six independent 8-cell loops inside a voice, separate from track trigger lanes.
struct CreativeState {
 uint8_t length[6]={4,3,4,4,5,7};
 uint8_t value[6][8]={{0,1,2,3,4,5,6,7},{48,55,60,43,48,52,55,60},{24,24,12,36,24,12,48,24},{100,80,120,60,100,90,120,70},{20,80,40,110,60,40,100,20},{64,100,30,80,64,110,40,90}};
 uint8_t follower_source=0;int8_t follower_amount=0;
 uint32_t seed=0x7319ab35;uint8_t mutation=0;
};
inline bool valid_creative(const CreativeState& c){if(c.follower_source>16 || c.follower_amount< -100 || c.follower_amount>100 || !c.seed || c.mutation>100)return false;for(unsigned l=0;l<6;++l){if(!c.length[l] || c.length[l]>8)return false;for(auto v:c.value[l])if(v>127)return false;}return true;}
enum class GestureTarget:uint8_t { Parameter, Morph, Macro };
struct GesturePoint {uint16_t tick=0,parameter=3;float value=0;uint8_t track=0,pattern=0;GestureTarget target=GestureTarget::Parameter;};
struct GestureState {uint16_t count=0,ticks=384;GesturePoint points[128]{};};

}
