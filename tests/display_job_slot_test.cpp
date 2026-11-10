#include "display_job_slot.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
int main(){
 groove::DisplayJobSlot slot;std::atomic<bool> stop{false};std::thread worker([&]{while(!stop.load(std::memory_order_relaxed)){if(!slot.process_one())std::this_thread::yield();}});
 auto pixels=std::make_unique<uint32_t[]>(480*272);auto shadow=std::make_unique<uint16_t[]>(480*272);auto packet=std::make_unique<uint8_t[]>(480*32*2+28);
 for(unsigned run=1;run<=64;++run){groove::DisplayJob job;job.pixels=pixels.get();job.shadow=shadow.get();job.packet=packet.get();job.side=run%2;job.epoch=run;job.frame=run*2;for(bool& v:job.valid)v=true;pixels[(run%16)*480+run]=0xffffff;assert(slot.submit(job));assert(!slot.submit(job));groove::DisplayJob done;while(!slot.collect(done))std::this_thread::yield();assert(done.epoch==run && done.frame==run*2 && done.result.valid && done.result.dirty && done.result.side==run%2);pixels[(run%16)*480+run]=0;}
 stop.store(true,std::memory_order_relaxed);worker.join();assert(!slot.busy());
}
