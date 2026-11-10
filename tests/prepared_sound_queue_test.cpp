#include "prepared_sound_queue.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>
using namespace groove;
static void stamp(SoundState& s,unsigned n){s.instrument=Instrument::VirtualAnalog;s.parameters[0]=float(n);std::snprintf(s.preset_name,sizeof s.preset_name,"snapshot-%u",n);for(auto& p:s.layer.parts){p.raw_size=kLayerRawBytes;std::memset(p.raw,uint8_t(n),sizeof p.raw);}s.virtual_analog.values[4]=float(100+n);}
int main(){
 std::vector<unsigned char> src(131200),dst(src.size());for(unsigned i=0;i<src.size();++i)src[i]=uint8_t(i*37+11);
 for(unsigned a=0;a<16;++a)for(unsigned b=0;b<16;++b)for(unsigned n:{0u,1u,31u,32u,63u,64u,65u,8193u,64060u,65537u}){std::memset(dst.data(),0xa5,dst.size());copy_normal_bytes(dst.data()+16+b,src.data()+a,n);assert(!std::memcmp(dst.data()+16+b,src.data()+a,n));for(unsigned i=0;i<16+b;++i)assert(dst[i]==0xa5);for(unsigned i=16+b+n;i<dst.size();++i)assert(dst[i]==0xa5);}
 auto pool=std::make_unique<SoundState[]>(kPresetSlots);for(unsigned i=0;i<kPresetSlots;++i)stamp(pool[i],i);
 auto q=std::make_unique<PreparedSoundQueue>();auto out=std::make_unique<SoundState>();uint16_t wanted[]={0,1};q->request_prepare(wanted,2,1);
 assert(q->prepare_one(pool.get(),1));assert(q->counters().prepared_bytes==16384);assert(q->push(pool[0],0,1));
 stamp(pool[0],1000);q->request_prepare(wanted,2,2);for(unsigned i=0;i<12;++i)q->prepare_one(pool.get(),2);assert(q->push(pool[0],0,2));
 assert(q->pop(*out) && out->parameters[0]==0 && out->layer.parts[3].raw[4095]==0);assert(q->pop(*out) && out->parameters[0]==1000 && out->layer.parts[3].raw[4095]==uint8_t(1000));assert(!q->pop(*out));
 for(unsigned i=0;i<64;++i){stamp(pool[i],i);assert(q->push(pool[i],i,3));}assert(!q->free_space() && !q->push(pool[64],64,3));
 uint16_t far[]={255};q->request_prepare(far,1,3);for(unsigned i=0;i<4;++i)assert(q->prepare_one(pool.get(),3));
 assert(q->pop(*out) && out->parameters[0]==0);assert(q->push(pool[64],64,3));for(unsigned i=1;i<=64;++i)assert(q->pop(*out) && out->parameters[0]==i);assert(q->free_space()==64);
 // Cancel a partial old version. All bytes of the newer version must agree.
 uint16_t one[]={2};q->request_prepare(one,1,4);assert(q->prepare_one(pool.get(),4));stamp(pool[2],2000);q->request_prepare(one,1,5);for(unsigned i=0;i<4;++i)assert(q->prepare_one(pool.get(),5));assert(q->push(pool[2],2,5) && q->pop(*out));assert(out->parameters[0]==2000 && out->layer.parts[0].raw[0]==uint8_t(2000) && out->layer.parts[3].raw[4095]==uint8_t(2000));
 auto threaded=std::make_unique<PreparedSoundQueue>();constexpr unsigned N=1024;std::atomic<bool> started{false};
 std::thread consumer([&]{auto value=std::make_unique<SoundState>();started.store(true,std::memory_order_release);for(unsigned n=0;n<N;){if(!threaded->pop(*value)){std::this_thread::yield();continue;}assert(value->parameters[0]==n && value->layer.parts[0].raw[0]==uint8_t(n) && value->layer.parts[3].raw[4095]==uint8_t(n));++n;}});
 while(!started.load(std::memory_order_acquire))std::this_thread::yield();
 for(unsigned n=0;n<N;++n){unsigned slot=n%16;stamp(pool[slot],n);uint16_t target[]={uint16_t(slot)};threaded->request_prepare(target,1,10+n);for(unsigned i=0;i<4;++i)threaded->prepare_one(pool.get(),10+n);while(!threaded->push(pool[slot],slot,10+n))std::this_thread::yield();}
 consumer.join();auto c=threaded->counters();assert(c.max_chunk<=16384 && c.completed==N && c.warm==N && !c.cold);
 std::printf("Prepared sound queue: unaligned exact copies, bounded chunks, immutable old versions, full-queue refusal and threaded 1024 snapshots passed; bytes %llu chunks %llu\n",(unsigned long long)c.prepared_bytes,(unsigned long long)c.chunks);
}
