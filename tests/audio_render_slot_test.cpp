#include "audio_render_slot.h"
#include <cassert>
#include <cstdio>
#include <thread>
struct Counter {unsigned value=0;};
static void render(void* context,float* main,float* cue,unsigned count){
 auto& counter=*static_cast<Counter*>(context);
 for(unsigned i=0;i<count;++i){float v=float(++counter.value);main[2*i]=v;main[2*i+1]=-v;cue[2*i]=v+1;cue[2*i+1]=-v-1;}
}
int main(){
 groove::AudioRenderSlot slot;Counter counter;assert(slot.start(&counter,render));
 std::thread producer([&]{while(!slot.stopped()){if(!slot.process_one())std::this_thread::yield();}});
 unsigned expected=1;float main[128],cue[128];
 while(expected<=500000){unsigned n=slot.available();if(!n){std::this_thread::yield();continue;}if(n>31)n=31;slot.read(main,cue,n);for(unsigned i=0;i<n;++i){assert(main[i*2]==float(expected));assert(main[i*2+1]==-float(expected));assert(cue[i*2]==float(expected+1));assert(cue[i*2+1]==-float(expected+1));++expected;}}
 slot.request_stop();while(!slot.stopped())std::this_thread::yield();producer.join();assert(slot.missing()==0);
 Counter second;assert(slot.start(&second,render));assert(slot.process_one());slot.read(main,cue,31);assert(slot.missing()==7);for(unsigned i=24;i<31;++i)assert(main[2*i]==0 && cue[2*i]==0);slot.request_stop();assert(slot.process_one());assert(slot.stopped());
 puts("audio render slot ownership, ordering, restart, and underrun passed");
}
