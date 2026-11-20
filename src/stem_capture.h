#pragma once
#include "alloc_policy.h"
#include "pcm_output.h"

namespace groove {
// The audio owner fills planar PCM blocks. Only complete blocks cross to the
// file task. Unreleased blocks are never overwritten by the render thread.
class StemCapture {
public:
 enum class State : unsigned { Idle, Armed, Recording, Complete, Cancelled, Overrun };
 static constexpr unsigned Groups=8, Direct=8, Returns=9, Mix=10, Buses=11;
 static constexpr unsigned Frames=4096, Blocks=16;
 struct Block { int16_t pcm[Buses][Frames*2]; unsigned frames; };

 ~StemCapture(){delete[] blocks_;}
 // The file task requests a take. Only its audio-thread Start command
 // can recycle the ring. Older queued commands and PCM stay with their take.
 State state()const{
  unsigned ticket=__atomic_load_n(&requested_,__ATOMIC_ACQUIRE);
  unsigned status=__atomic_load_n(&status_,__ATOMIC_ACQUIRE);
  if((status>>3)==ticket)return State(status&7);
  return cancelled(ticket)?State::Cancelled:State::Armed;
 }
 unsigned arm(){
  State current=state();
  if((current==State::Armed || current==State::Recording) && !cancelled(requested_))return 0;
  if(!blocks_)blocks_=alloc_array<Block>(Blocks);
  if(!blocks_)return 0;
  unsigned ticket=(requested_+1)&0x1fffffffu;if(!ticket)ticket=1;
  __atomic_store_n(&requested_,ticket,__ATOMIC_RELEASE);
  return ticket;
 }
 void request_cancel(){__atomic_store_n(&cancelled_,requested_,__ATOMIC_RELEASE);}
 const Block* front()const{
  State s=state();if(s!=State::Recording && s!=State::Complete)return nullptr;
  return read_==__atomic_load_n(&written_,__ATOMIC_ACQUIRE)?nullptr:&blocks_[read_%Blocks];
 }
 void release(){__atomic_add_fetch(&read_,1u,__ATOMIC_RELEASE);}

 // Audio owner only.
 bool accept_audio(unsigned ticket){
  if(ticket!=__atomic_load_n(&requested_,__ATOMIC_ACQUIRE))return false;
  active_=ticket;running_=false;armed_=true;
  if(cancelled(ticket)){interrupt();return false;}
  offset_=0;__atomic_store_n(&read_,written_,__ATOMIC_RELEASE);
  publish(State::Armed);return true;
 }
 void begin_audio(){check_cancel();if(armed_){armed_=false;running_=true;publish(State::Recording);}}
 bool running()const{return running_;}
 void check_cancel(){if(cancelled(active_) || active_!=__atomic_load_n(&requested_,__ATOMIC_ACQUIRE))interrupt();}
 void interrupt(){
  if(armed_ || running_){armed_=running_=false;publish(State::Cancelled);}
 }
 void finish(){
  if(!running_)return;
  if(offset_)commit_block();
  running_=false;publish(State::Complete);
 }
 void frame(const float* group_l,const float* group_r,const float* gains,
            float direct_l,float direct_r,float wet_l,float wet_r,float main_l,float main_r){
  if(!offset_ && written_-__atomic_load_n(&read_,__ATOMIC_ACQUIRE)>=Blocks){
   running_=false;publish(State::Overrun);return;
  }
  auto& block=blocks_[written_%Blocks];
  auto put=[&](unsigned bus,float l,float r){
   block.pcm[bus][2*offset_]=pcm16(l);block.pcm[bus][2*offset_+1]=pcm16(r);
  };
  for(unsigned g=0;g<Groups;++g)put(g,group_l[g]*gains[g],group_r[g]*gains[g]);
  put(Direct,direct_l,direct_r);put(Returns,wet_l,wet_r);put(Mix,main_l,main_r);
  if(++offset_==Frames)commit_block();
 }
private:
 bool cancelled(unsigned ticket)const{return __atomic_load_n(&cancelled_,__ATOMIC_ACQUIRE)==ticket;}
 void publish(State s){__atomic_store_n(&status_,(active_<<3)|unsigned(s),__ATOMIC_RELEASE);}
 void commit_block(){blocks_[written_%Blocks].frames=offset_;offset_=0;__atomic_add_fetch(&written_,1u,__ATOMIC_RELEASE);}
 Block* blocks_=nullptr;
 unsigned written_=0,read_=0,offset_=0;
 bool running_=false,armed_=false;
 unsigned requested_=0,cancelled_=0,active_=0,status_=unsigned(State::Idle);
};
}
