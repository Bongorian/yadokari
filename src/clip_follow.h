#pragma once
#include <stdint.h>
namespace groove {
enum class ClipFollowAction:uint8_t {Off,Stop,Next,Target,Random};
struct ClipFollow {ClipFollowAction action=ClipFollowAction::Off;uint8_t cycles=1,target=0,probability=100;};
inline bool valid_clip_follow(const ClipFollow& f){return unsigned(f.action)<=4 && f.cycles>=1 && f.cycles<=64 && f.target<16 && f.probability<=100;}
inline uint32_t clip_follow_random(uint32_t& seed){if(!seed)seed=1;seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
// -1: keep playing, -2: stop, 0..15: launch target. Called at a completed
// clip cycle, after higher-priority transport/Scene/manual decisions.
inline int clip_follow_destination(const ClipFollow& f,unsigned pattern,unsigned cycle,uint32_t& seed){
 if(!valid_clip_follow(f) || f.action==ClipFollowAction::Off || !cycle || cycle%f.cycles)return -1;
 if(!f.probability || (f.probability<100 && clip_follow_random(seed)%100>=f.probability))return -1;
 if(f.action==ClipFollowAction::Stop)return -2;
 if(f.action==ClipFollowAction::Next)return int((pattern+1)%16);
 if(f.action==ClipFollowAction::Target)return f.target;
 // Random chooses one of the other 15 patterns; a self-loop is explicit Target.
 unsigned choice=clip_follow_random(seed)%15;return int(choice>=pattern?choice+1:choice);
}
inline const char* clip_follow_name(ClipFollowAction a){static const char* names[]={"OFF","STOP","NEXT","TARGET","RANDOM"};return unsigned(a)<5?names[unsigned(a)]:"?";}
}
