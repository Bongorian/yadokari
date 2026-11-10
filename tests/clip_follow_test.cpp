#include "clip_follow.h"
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 ClipFollow f;uint32_t seed=1234;assert(clip_follow_destination(f,3,1,seed)==-1);
 f.action=ClipFollowAction::Next;f.cycles=2;assert(clip_follow_destination(f,15,1,seed)==-1 && clip_follow_destination(f,15,2,seed)==0);
 f.action=ClipFollowAction::Stop;assert(clip_follow_destination(f,1,2,seed)==-2);
 f.action=ClipFollowAction::Target;f.target=9;assert(clip_follow_destination(f,1,2,seed)==9);
 f.action=ClipFollowAction::Random;f.cycles=1;f.probability=51;uint32_t twin=seed;unsigned changed=0,kept=0;for(unsigned i=1;i<256;++i){int a=clip_follow_destination(f,7,i,seed),b=clip_follow_destination(f,7,i,twin);assert(a==b && a!=7 && a<16);if(a<0)++kept;else ++changed;}assert(changed && kept);
 f.probability=0;assert(clip_follow_destination(f,1,1,seed)==-1);f.cycles=0;assert(!valid_clip_follow(f));std::puts("Clip Follow: Off/Stop/Next/Target/Random, cycles, probability and deterministic seed passed");
}
