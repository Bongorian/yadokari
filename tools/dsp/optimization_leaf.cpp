#include "virtual_analog_voice.h"
#include "track_insert.h"
#include "project_save_slot.h"
#include "update/sha256.h"
#include <memory>
#include <cstdio>
#include <chrono>
using namespace groove;
static void emit(update::Sha256& hash){uint8_t d[32];hash.finish(d);for(auto x:d)printf("%02x",x);puts("");}
int main(){
 for(unsigned mode=0;mode<6;++mode){VirtualAnalogVoice v;v.init(48000);VirtualAnalogState state;state.values[18]=mode==3;state.values[19]=mode==4?.4f:0;state.values[23]=float(mode%3);state.values[6]=mode==5?0:.15f;state.values[7]=mode==5?0:.02f;v.trigger(.8f,state.values,0x57313001);update::Sha256 hash;
  for(unsigned i=0;i<48000;++i){if(i%4096==0){state.values[11]=.005f+float(i%7)*.0001f;state.values[4]=2200+float(i%19);state.values[23]=float((mode+i/4096)%3);v.parameters(state.values);}float sample=v.next(220+float(i%11),i<36000);hash.add(reinterpret_cast<uint8_t*>(&sample),sizeof sample);}printf("va-sweep-%u ",mode);emit(hash);
 }
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto slot=std::make_unique<ProjectEncodeSlot>();auto begin=std::chrono::steady_clock::now();if(!slot->submit_copy(*p,"PROJECTS/Optimization.mgxs"))return 4;auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();printf("snapshot-bytes %zu host-us %lld\n",sizeof(ProjectState),(long long)elapsed);if(!slot->process_one())return 5;ProjectEncodeSlot::Result r;if(!slot->collect(r)||!r.ok)return 6;delete[] r.bytes;return 0;
}
