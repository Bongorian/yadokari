#include "audio_stage_profile.h"
#include <cassert>
static uint32_t tick(){return 0;}
int main(){groove::AudioStageProfile p;assert(!p.sample());p.begin(tick);assert(p.sample()==tick);p.record(3,4,8,2);for(unsigned i=0;i<63;++i)assert(!p.sample());assert(p.sample()==tick);p.record(1,9,2,4);auto s=p.end();assert(s.calls==2 && s.control==4 && s.events==13 && s.tracks==10 && s.output==6);assert(s.max_control==3 && s.max_events==9 && s.max_tracks==8 && s.max_output==4);assert(s.max_total==17 && s.peak_control==3 && s.peak_events==4 && s.peak_tracks==8 && s.peak_output==2);assert(!p.sample());p.begin(tick,63);unsigned even=0,odd=0;for(unsigned i=0;i<127;++i)if(p.sample()){if(i%2)++odd;else ++even;}assert(even==2 && odd==1);p.end();}
