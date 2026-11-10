#pragma once
#include "project.h"
namespace groove {
// DSP owns the fixed take until Ready/Failed. Task copies only Ready and
// acknowledges after committing/discarding. Reset never overwrites published data.
class SceneTake {
public:
 enum State:unsigned {Idle,Armed,Recording,Finishing,Ready,Failed};
 enum Error:unsigned {None,Capacity,Interrupted,Unsupported,NoBars};
 State state()const{return State(__atomic_load_n(&state_,__ATOMIC_ACQUIRE));}
 bool active()const{auto s=state();return s==Armed || s==Recording || s==Finishing;}
 bool arm(){if(state()!=Idle)return false;take_=SongState{};take_.enabled=true;take_.loop=false;error_=None;publish(Armed);return true;}
 void finish(){if(state()==Recording)publish(Finishing);else if(state()==Armed)fail(NoBars);}
 void fail(Error e){if(active()){error_=e;publish(Failed);}}
 void reset(){publish(Idle);}
 void begin(uint64_t tick,unsigned scene,const ClipState* clips,uint16_t muted){if(state()!=Armed)return;opened_=tick;take_.length=1;snapshot(take_.sections[0],scene,clips,muted,0xffff);publish(Recording);}
 void advance(uint64_t tick){
  if(state()!=Recording && state()!=Finishing)return;
  if(tick<opened_)return;
  while(tick-opened_>=kSongMaxTicks){auto& previous=take_.sections[take_.length-1];previous.ticks=kSongMaxTicks;
   if(take_.length==32){fail(Capacity);return;}
   auto& next=take_.sections[take_.length++];next=previous;next.ticks=0;next.restart_mask=0;opened_+=kSongMaxTicks;
  }
 }
 void observe(uint64_t tick,unsigned scene,const ClipState* clips,uint16_t muted,uint16_t restarts){
  if(state()!=Recording && state()!=Finishing)return;
  advance(tick);if(state()==Failed)return;
  SongSection next;snapshot(next,scene,clips,muted,restarts);auto& previous=take_.sections[take_.length-1];bool changed=previous.playing_mask!=next.playing_mask || previous.mute_mask!=next.mute_mask;
  for(unsigned t=0;t<kTracks;++t)changed|=previous.patterns[t]!=next.patterns[t];
  if(!changed && !restarts)return;
  if(tick==opened_){next.restart_mask|=previous.restart_mask;previous=next;return;}
  previous.ticks=uint32_t(tick-opened_);if(take_.length==32){fail(Capacity);return;}
  take_.sections[take_.length++]=next;opened_=tick;
 }
 void end(uint64_t tick){
  if(state()!=Recording && state()!=Finishing)return;
  if(tick<opened_){fail(NoBars);return;}
  // Finish before splitting: an exact maximum-length final section is valid.
  while(tick-opened_>kSongMaxTicks){advance(opened_+kSongMaxTicks);if(state()==Failed)return;}
  auto& last=take_.sections[take_.length-1];last.ticks=uint32_t(tick-opened_);
  if(!last.ticks){if(take_.length>1)--take_.length;else {fail(NoBars);return;}}
  publish(Ready);
 }
 bool copy(SongState& out)const{if(state()!=Ready)return false;out=take_;return true;}
 Error error()const{return state()==Failed?error_:None;}
 void acknowledge(){auto s=state();if(s==Ready || s==Failed)publish(Idle);}
private:
 static void snapshot(SongSection& s,unsigned scene,const ClipState* clips,uint16_t muted,uint16_t restarts){s=SongSection{};s.scene=uint8_t(scene);s.playing_mask=0;s.mute_mask=muted;s.restart_mask=restarts;for(unsigned t=0;t<kTracks;++t){s.patterns[t]=clips[t].pattern;if(clips[t].playing)s.playing_mask|=uint16_t(1u<<t);}}
 void publish(State s){__atomic_store_n(&state_,unsigned(s),__ATOMIC_RELEASE);}
 SongState take_{};uint64_t opened_=0;unsigned state_=Idle;Error error_=None;
};
}
