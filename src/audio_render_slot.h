#pragma once
#include <stdint.h>
namespace groove {
// One core owns the renderer; the USB interrupt consumes interleaved stereo
// frames without waiting. Storage and object remain alive through stop_ack.
//
// Restart call-order contract (SPEC section 8, item m5):
//  1. start() may only be called while stopped()==true. It resets the
//     read/write cursors and the missing counter before publishing the
//     active state, so a (re)start always begins from an empty slot.
//  2. The owner starts the consuming device only after start() returned
//     true and the slot has been prefilled (available() >= block; the USB
//     owner waits for target frames with a bounded timeout).
//  3. To stop, the owner calls request_stop() and must keep the context
//     object and this slot alive until stopped()==true: the render core
//     acknowledges through process_one(), and read() may still drain the
//     frames that were published before the acknowledgement.
//  4. start() after a stop follows the same order; calling start() while
//     active or while a stop is unacknowledged fails (returns false)
//     instead of disturbing the in-flight stream.
class AudioRenderSlot {
 public:
 using Render=void (*)(void*,float*,float*,unsigned);
 // 4 ms render-ahead absorbs the measured ~1.8 ms FX event burst plus
 // normal DSP/USB scheduling. Adds 2 ms versus the former 96-frame target.
 static constexpr unsigned capacity=256, target=192, block=24;
 static_assert(target+block<=capacity,"render-ahead must fit the fixed ring");
 bool start(void* context,Render render){
  if(!context || !render || __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0)return false;
  context_=context;render_=render;__atomic_store_n(&read_,0u,__ATOMIC_RELAXED);__atomic_store_n(&write_,0u,__ATOMIC_RELAXED);__atomic_store_n(&missing_,0u,__ATOMIC_RELAXED);__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);wake();return true;
 }
 void request_stop(){__atomic_store_n(&state_,2u,__ATOMIC_RELEASE);wake();}
 bool stopped()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==0;}
 bool active()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)==1;}
 unsigned available()const{unsigned r=__atomic_load_n(&read_,__ATOMIC_ACQUIRE),w=__atomic_load_n(&write_,__ATOMIC_ACQUIRE),n=w-r;return n<=capacity?n:0;}
 unsigned missing()const{return __atomic_load_n(&missing_,__ATOMIC_RELAXED);}
 // Returns true for work or a stop acknowledgement, false when idle/full.
 bool process_one(){
  unsigned state=__atomic_load_n(&state_,__ATOMIC_ACQUIRE);
  if(state==2){__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);wake();return true;}
  if(state!=1)return false;
  unsigned w=__atomic_load_n(&write_,__ATOMIC_RELAXED),r=__atomic_load_n(&read_,__ATOMIC_ACQUIRE);
  if(w-r>target-block)return false;
  float main[block*2],cue[block*2];render_(context_,main,cue,block);
  for(unsigned i=0;i<block;++i){Frame& f=frames_[(w+i)%capacity];f={main[2*i],main[2*i+1],cue[2*i],cue[2*i+1]};}
  __atomic_store_n(&write_,w+block,__ATOMIC_RELEASE);return true;
 }
 void read(float* main,float* cue,unsigned frames){
  unsigned r=__atomic_load_n(&read_,__ATOMIC_RELAXED),w=__atomic_load_n(&write_,__ATOMIC_ACQUIRE);unsigned count=w-r;if(count>frames)count=frames;
  for(unsigned i=0;i<count;++i){const Frame& f=frames_[(r+i)%capacity];main[2*i]=f.l;main[2*i+1]=f.r;cue[2*i]=f.cl;cue[2*i+1]=f.cr;}
  for(unsigned i=count;i<frames;++i)main[2*i]=main[2*i+1]=cue[2*i]=cue[2*i+1]=0;
  if(count<frames)__atomic_add_fetch(&missing_,frames-count,__ATOMIC_RELAXED);
  __atomic_store_n(&read_,r+count,__ATOMIC_RELEASE);wake();
 }
 private:
 static void wake(){
#if defined(__aarch64__)
  asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 }
 struct Frame{float l,r,cl,cr;};Frame frames_[capacity]{};
 void* context_=nullptr;Render render_=nullptr;unsigned read_=0,write_=0,state_=0,missing_=0;
};
}
