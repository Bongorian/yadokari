#include "sample_analysis_slot.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
int main(){
 std::vector<uint8_t> pcm(2048);for(unsigned i=0;i<1024;++i){int16_t s=int16_t((int(i%127)-63)*400);pcm[2*i]=uint8_t(s);pcm[2*i+1]=uint8_t(uint16_t(s)>>8);}
 groove::WavView view{pcm.data(),1024,48000,1,16,1};groove::SampleAnalysis expected;groove::analyze_sample(view,expected);
 groove::SampleAnalysis partial;unsigned checks=0;assert(!groove::analyze_sample_until(view,partial,0,1,[&](){++checks;return true;}));assert(checks==1);
 groove::SampleAnalysisSlot cancelled;assert(cancelled.submit(view,0,1,999));cancelled.cancel();assert(cancelled.process_one());uint32_t cancelled_token=0;assert(cancelled.collect(partial,cancelled_token) && cancelled_token==999);
 assert(cancelled.submit(view,0,1,1000) && cancelled.process_one() && cancelled.collect(partial,cancelled_token));assert(cancelled_token==1000 && partial.slices==expected.slices);
 groove::SampleAnalysisSlot slot;std::atomic<bool> done{false};std::thread worker([&](){while(!done.load(std::memory_order_relaxed)){if(!slot.process_one())std::this_thread::yield();}});
 for(uint32_t i=1;i<=200;++i){while(!slot.submit(view,0,1,i))std::this_thread::yield();groove::SampleAnalysis actual;uint32_t token=0;while(!slot.collect(actual,token))std::this_thread::yield();assert(token==i && actual.slices==expected.slices);for(unsigned b=0;b<groove::kSampleWaveBins;++b)assert(actual.low[b]==expected.low[b] && actual.high[b]==expected.high[b]);}
 done.store(true,std::memory_order_relaxed);worker.join();assert(!slot.busy());
}
