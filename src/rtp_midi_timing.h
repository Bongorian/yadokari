#pragma once
// Timing glue between network MIDI and the audio engine.
//
// SampleClock maps local time (microseconds) to the engine's rendered-sample
// counter and back. The counter advances in bursts as audio blocks render, so
// the mapping follows the upper envelope of (samples - rate * time): a time
// maps to a sample that is not rendered yet at that time, and a rendered
// sample maps to a time that has passed. The envelope decays slowly so it
// tracks the drift between the CPU timer and the audio clock.
//
// DueQueue holds one input's channel messages until their due time, in order.
#include <stdint.h>
#include <cstring>

namespace groove::rtpmidi {

class SampleClock {
public:
 static constexpr int64_t kDecayPerSecond=20; // samples/s, well above crystal drift
 explicit SampleClock(uint32_t rate=48000):rate_(rate){}
 bool valid()const{return valid_;}
 void observe(uint64_t now_us,uint64_t samples){
  const int64_t e=(int64_t(samples)-line(now_us))*256;
  if(!valid_){offset_=e;valid_=true;last_=now_us;return;}
  if(now_us>last_){offset_-=int64_t(now_us-last_)*kDecayPerSecond*256/1000000;last_=now_us;}
  if(e>offset_)offset_=e;
 }
 uint64_t sample_at(uint64_t us)const{const int64_t s=line(us)+offset_/256;return s>0?uint64_t(s):0;}
 uint64_t us_at(uint64_t sample)const{
  const int64_t s=int64_t(sample)-offset_/256;
  return s>0?uint64_t(s)*1000000/rate_:0;
 }
private:
 int64_t line(uint64_t us)const{return int64_t(us*rate_/1000000);}
 uint32_t rate_;int64_t offset_=0;uint64_t last_=0;bool valid_=false;
};

class DueQueue {
public:
 static constexpr unsigned kCapacity=256;
 struct Item { uint64_t due=0;uint8_t bytes[3]{};uint8_t length=0; };
 bool empty()const{return count_==0;}
 unsigned size()const{return count_;}
 bool full()const{return count_==kCapacity;}
 bool push(uint64_t due,const uint8_t* bytes,unsigned length){
  if(full() || length>3)return false;
  auto& item=items_[(head_+count_)%kCapacity];item.due=due;item.length=uint8_t(length);memcpy(item.bytes,bytes,length);++count_;
  return true;
 }
 const Item& front()const{return items_[head_];}
 void pop(){if(count_){head_=(head_+1)%kCapacity;--count_;}}
 void clear(){head_=count_=0;}
 // Delivers every message due at or before now.
 template<class Fn> unsigned release(uint64_t now,Fn fn){
  unsigned n=0;
  while(count_ && items_[head_].due<=now){const Item item=items_[head_];pop();fn(item.bytes,unsigned(item.length));++n;}
  return n;
 }
 template<class Fn> void flush(Fn fn){while(count_){const Item item=items_[head_];pop();fn(item.bytes,unsigned(item.length));}}
private:
 Item items_[kCapacity];unsigned head_=0,count_=0;
};

} // namespace groove::rtpmidi
