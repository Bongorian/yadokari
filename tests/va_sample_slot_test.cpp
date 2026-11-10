#include "../src/va_sample_slot.h"
#include <thread>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace groove;
static bool same(float a,float b){return !std::memcmp(&a,&b,4);}
int main(){
 constexpr unsigned voices=VaSampleSlot::capacity;
 assert(VaSampleSlot::selected_count(0)==0 && VaSampleSlot::selected_count(8)==8 && VaSampleSlot::selected_count(11)==8 && VaSampleSlot::selected_count(12)==9 && VaSampleSlot::selected_count(35)==32 && VaSampleSlot::selected_count(48)==32);
 VirtualAnalogState settings;VirtualAnalogTriggerBank bank;bank.init(48000);VirtualAnalogVoice voice[voices],reference[voices];for(unsigned i=0;i<voices;++i){settings.values[0]=i%4;settings.values[1]=(i/4)%4;settings.values[23]=i%3;settings.values[8]=.2f+float(i%7)*.1f;settings.values[18]=i%2;settings.values[19]=float(i%5)*.05f;voice[i].init(48000,&bank);voice[i].trigger(.8f,settings.values,100+i);reference[i]=voice[i];}
 VaSampleSlot slot;slot.worker_ready();uint64_t used=0;
 for(unsigned frame=0;frame<128;++frame){assert(slot.begin(frame));unsigned jobs[voices];for(unsigned j=0;j<voices;++j){unsigned i=(j+frame)%voices;jobs[i]=slot.add(voice+i,voice[i],220+i*10,true,0,i+16);}assert(slot.add(voice,voice[0],220,true,0,16)==voices);slot.publish();slot.process_one();for(unsigned i=0;i<voices;++i){float output;if(!slot.take(voice+i,frame,jobs[i],voice[i],220+i*10,true,output))output=voice[i].next(220+i*10,true);else ++used;assert(same(output,reference[i].next(220+i*10,true)));}}assert(used>700);auto reordered=slot.counts();assert(reordered.cloned<voices*8 && reordered.reused>voices*100);
 // Force the consumer to move ahead while a private job is still outstanding.
 for(unsigned i=0;i<voices;++i)assert(same(voice[i].next(220+i*10,true),reference[i].next(220+i*10,true)));
 assert(slot.begin(129));unsigned late=slot.add(voice,voice[0],220,true);slot.publish();float output;if(!slot.take(voice,130,late,voice[0],220,true,output))output=voice[0].next(220,true);assert(same(output,reference[0].next(220,true)));assert(!slot.begin(130));slot.process_one();assert(!slot.take(voice,130,late,voice[0],220,true,output));
 std::atomic<bool> stop{false};std::thread worker([&]{while(!stop.load(std::memory_order_acquire)){if(!slot.process_one())std::this_thread::yield();}});
 for(unsigned frame=131;frame<10131;++frame){unsigned jobs[voices];for(auto& j:jobs)j=voices;if(slot.begin(frame)){for(unsigned i=0;i<voices;++i)jobs[i]=slot.add(voice+i,voice[i],220+i*10,frame<9000);slot.publish();}for(unsigned i=0;i<voices;++i){float x;if(!slot.take(voice+i,frame,jobs[i],voice[i],220+i*10,frame<9000,x))x=voice[i].next(220+i*10,frame<9000);float y=reference[i].next(220+i*10,frame<9000);assert(same(x,y));}}
 stop.store(true,std::memory_order_release);worker.join();auto n=slot.counts();assert(n.used>700 && !n.mismatch);printf("VA typed private snapshots: used %llu late %llu busy %llu checked %llu mismatches %llu\n",(unsigned long long)n.used,(unsigned long long)n.late,(unsigned long long)n.busy,(unsigned long long)n.checked,(unsigned long long)n.mismatch);
}
