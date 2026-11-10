#pragma once
#include "project.h"
namespace groove {
inline int bounded(int value,int lo,int hi){return value<lo?lo:value>hi?hi:value;}
inline int positive_mod(int64_t value,int divisor){int r=int(value%divisor);return r<0?r+divisor:r;}
inline uint32_t next_random(uint32_t& seed){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
// A step's probability is a function of musical position, not of the order in
// which other tracks happen to fire. Seek and uninterrupted playback use the
// same step index; no mutable random stream is shared across tracks.
inline uint32_t step_probability_draw(uint32_t seed,unsigned track,unsigned pattern,uint64_t tick){
 uint32_t x=seed^(0x9e3779b9u*(track+1))^(0x85ebca6bu*(pattern+1))^uint32_t(tick)^(uint32_t(tick>>32)*0xc2b2ae35u);
 x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x%100;
}
// Reproducible, non-destructive target; each grid cell has an independent seed.
inline StepState mutation_step(ConstTrackView track,unsigned cell){
 if(cell>=kSteps){return StepState{};}StepState out=track.steps[cell];unsigned amount=track.mutation.amount;if(!amount)return out;
 StepState source=out;if(track.mutation.frozen)source=track.mutation_source[cell];StepState target=source;
 uint32_t rng=track.mutation.seed^(0x9e3779b9u*(cell+1));if(!rng)rng=0x7319ab35;auto draw=[&](){return next_random(rng);};
 uint32_t hit=draw();if(hit%4==0)target.enabled=!target.enabled;
 target.note=uint8_t(bounded(int(source.note)+int(draw()%11)-5,0,127));
 // Transpose chord tones with the root; 255 denotes an empty chord slot.
 // Keeping the same displacement preserves intervals except at MIDI bounds.
 int transpose=int(target.note)-int(source.note);
 for(unsigned i=0;i<kChordTones;++i)if(source.chord[i]<128)target.chord[i]=uint8_t(bounded(int(source.chord[i])+transpose,0,127));
 target.velocity=uint8_t(bounded(int(source.velocity)+int(draw()%41)-20,1,127));
 target.gate=uint16_t(bounded(int(source.gate)+int(draw()%49)-24,1,384));
 target.probability=uint8_t(bounded(int(source.probability)+int(draw()%41)-20,0,100));
 target.microtiming=int16_t(bounded(int(source.microtiming)+int(draw()%49)-24,-192,192));
 uint32_t ratchet=draw();target.ratchet=uint8_t(1+ratchet%3);
 for(auto& lock:target.locks)if(lock.enabled){const auto* d=parameter_descriptor(lock.id);if(!d)continue;int direction=int(draw()%201)-100;lock.value=clamp_parameter(lock.id,lock.value+(d->maximum-d->minimum)*float(direction)/1000.0f);}
 uint32_t sound=draw();if(source.sound<kPresetSlots)target.sound=uint8_t(sound%16);
 auto blend=[&](int a,int b){return a+(b-a)*int(amount)/100;};
 if(!(out.lock_only && !out.enabled) && amount>=1+(hit>>8)%100)out.enabled=target.enabled;
 out.note=uint8_t(blend(out.note,target.note));out.velocity=uint8_t(blend(out.velocity,target.velocity));out.gate=uint16_t(blend(out.gate,target.gate));out.probability=uint8_t(blend(out.probability,target.probability));out.microtiming=int16_t(blend(out.microtiming,target.microtiming));
 for(unsigned i=0;i<kChordTones;++i){if(out.chord[i]<128 && target.chord[i]<128)out.chord[i]=uint8_t(blend(out.chord[i],target.chord[i]));else if(out.chord[i]!=target.chord[i]){uint32_t selector=track.mutation.seed^(0x85ebca6bu*(cell+1))^(0xc2b2ae35u*(i+1));if(amount>=1+next_random(selector)%100)out.chord[i]=target.chord[i];}}
 if(amount>=1+(ratchet>>8)%100)out.ratchet=target.ratchet;
 if(amount>=1+(sound>>8)%100)out.sound=target.sound;
 for(unsigned i=0;i<kLocks;++i){uint32_t gate=draw();if(out.locks[i].enabled && target.locks[i].enabled && out.locks[i].id==target.locks[i].id && parameter_descriptor(out.locks[i].id))out.locks[i].value=clamp_parameter(out.locks[i].id,out.locks[i].value+(target.locks[i].value-out.locks[i].value)*float(amount)/100);else if(amount>=1+gate%100)out.locks[i]=target.locks[i];}
 return out;
}

inline int accumulated_pitch(const AccumulatorState& a,uint32_t cycle){
 if(a.reset)cycle%=a.reset;
 int64_t v=int64_t(cycle)*a.pitch;int lo=a.minimum,hi=a.maximum,width=hi-lo;
 if(!width)return lo;
 if(a.mode==AccumulatorMode::Clamp)return v<lo?lo:v>hi?hi:int(v);
 if(a.mode==AccumulatorMode::Wrap)return lo+positive_mod(v-lo,width+1);
 int phase=positive_mod(v-lo,width*2);return phase<=width?lo+phase:hi-(phase-width);
}
inline int scale_move(int note,int degrees,ConstTrackView t){
 note=bounded(note,0,127);int direction=degrees<0?-1:1;unsigned count=unsigned(degrees<0?-degrees:degrees);
 while(count--){do{int next=note+direction;if(next<0 || next>127)return note;note=next;}while(!(t.scale_mask&(1u<<((note+12-t.scale_root)%12))));}
 return note;
}
inline int groove_timing(const GrooveState& g,unsigned cell){
 cell%=16;static const int8_t push[]={0,-12,8,-8,0,-16,12,-4,0,-12,8,-8,0,-16,12,-4};
 static const int8_t human[]={0,-5,7,-3,4,-8,3,-2,0,6,-4,8,-6,2,-3,5};
 return g.preset==1?push[cell]:g.preset==2?(cell&1?48:0):g.preset==3?human[cell]:g.preset==4?g.timing[cell]:0;
}
inline int groove_velocity(const GrooveState& g,unsigned cell){
 cell%=16;static const int8_t accent[]={12,-8,-3,-12,8,-10,0,-6,12,-8,-3,-12,8,-10,0,-6};
 return g.preset==1 || g.preset==2?accent[cell]:g.preset==3?(int(cell*13%17)-8):g.preset==4?g.velocity[cell]:0;
}
// Capture a non-destructive 16-cell template from every enabled step. Timing
// is relative to the straight grid, including source swing and groove.
inline bool extract_groove(ConstTrackView track,unsigned swing,int16_t timing[16],GrooveState& result){
 result=GrooveState{};result.preset=4;result.amount=100;bool clipped=false;
 for(unsigned cell=0;cell<16;++cell){int sum_time=0,sum_velocity=0,count=0;int gt=groove_timing(track.groove,cell)*track.groove.amount/100,gv=groove_velocity(track.groove,cell)*track.groove.amount/100;
  for(unsigned step=cell;step<track.length;step+=16){const auto& event=track.steps[step];if(!event.enabled)continue;sum_time+=bounded(int(event.microtiming)+gt,-192,192);sum_velocity+=bounded(int(event.velocity)+gv,0,127)-100;++count;}
  int time=count?sum_time/count:gt;int vel=count?sum_velocity/count:gv;timing[cell]=int16_t(time+((cell&1)?int(swing)*96/100:0));if(vel< -64 || vel>64)clipped=true;result.velocity[cell]=int8_t(bounded(vel,-64,64));
 }
 return clipped;
}
inline bool paste_groove(const int16_t timing[16],unsigned swing,GrooveState& result){bool clipped=false;for(unsigned cell=0;cell<16;++cell){int value=timing[cell]-((cell&1)?int(swing)*96/100:0);if(value< -96 || value>96)clipped=true;result.timing[cell]=int8_t(bounded(value,-96,96));}return clipped;}
inline uint8_t lane_cell(const TriggerLane& lane,uint64_t triggers){
 unsigned span=0;for(unsigned i=0;i<lane.length;++i)span+=lane.duration[i];
 // Ping-pong visits the endpoints once: 0,1,2,1,0,...
 if(lane.direction==PlaybackDirection::PingPong && lane.length>1){span=lane.duration[0]+lane.duration[lane.length-1];for(unsigned i=1;i+1<lane.length;++i)span+=2*lane.duration[i];}
 unsigned phase=unsigned(triggers%span);
 for(unsigned n=0;n<(lane.direction==PlaybackDirection::PingPong && lane.length>1?2u*lane.length-2:unsigned(lane.length));++n){unsigned cell=lane.direction==PlaybackDirection::Reverse?lane.length-1-n:lane.direction==PlaybackDirection::PingPong && n>=lane.length?2*lane.length-2-n:n;if(phase<lane.duration[cell])return uint8_t(cell);phase-=lane.duration[cell];}
 return 0;
}
// Compact command fields are shared by audio and edit mirrors, keeping validation
// identical and making individual live edits undoable without heap allocation.
enum class AdvancedGroup : uint16_t { Accumulator, Groove, Lane };
inline int advanced_value(ConstTrackView t,AdvancedGroup group,unsigned key,unsigned cell=0){
 if(group==AdvancedGroup::Accumulator){const auto& a=t.accumulator;const int v[]={a.pitch,a.minimum,a.maximum,int(a.mode),a.reset,int(a.scale_degrees),a.velocity,a.gate};return key<8?v[key]:0;}
 if(group==AdvancedGroup::Groove){return key==0?t.groove.preset:key==1?t.groove.amount:key==2?t.groove.timing[cell%16]:t.groove.velocity[cell%16];}
 if(key==0)return t.lanes_enabled;
 unsigned l=(key-1)/4,k=(key-1)%4;if(l>2)return 0;const auto& lane=t.lanes[l];return k==0?lane.length:k==1?int(lane.direction):k==2?lane.values[cell%16]:lane.duration[cell%16];
}
inline bool set_advanced(TrackView t,AdvancedGroup group,unsigned key,int value,unsigned cell=0){
 if(group==AdvancedGroup::Accumulator){auto a=t.accumulator;switch(key){case 0:a.pitch=int8_t(value);break;case 1:a.minimum=int8_t(value);break;case 2:a.maximum=int8_t(value);break;case 3:a.mode=AccumulatorMode(value);break;case 4:a.reset=uint8_t(value);break;case 5:if(value<0 || value>1)return false;a.scale_degrees=value;break;case 6:a.velocity=int8_t(value);break;case 7:a.gate=int8_t(value);break;default:return false;}if(value< -64 || value>64)return false;auto old=t.accumulator;t.accumulator=a;if(!valid_advanced_track(t)){t.accumulator=old;return false;}return true;}
 if(group==AdvancedGroup::Groove){if(cell>=16)return false;if(key==0 && value>=0 && value<=4)t.groove.preset=uint8_t(value);else if(key==1 && value>=0 && value<=100)t.groove.amount=uint8_t(value);else if(key==2 && value>=-96 && value<=96)t.groove.timing[cell]=int8_t(value);else if(key==3 && value>=-64 && value<=64)t.groove.velocity[cell]=int8_t(value);else return false;return true;}
 if(group!=AdvancedGroup::Lane || cell>=16)return false;
 if(key==0){if(value<0 || value>1)return false;t.lanes_enabled=value;return true;}
 unsigned l=(key-1)/4,k=(key-1)%4;if(l>2)return false;auto& lane=t.lanes[l];if(k==0 && value>=1 && value<=16)lane.length=uint8_t(value);else if(k==1 && value>=0 && value<=2)lane.direction=PlaybackDirection(value);else if(k==2 && value>=(l==2?1:0) && value<=127)lane.values[cell]=uint8_t(value);else if(k==3 && value>=1 && value<=16)lane.duration[cell]=uint8_t(value);else return false;return true;
}
}
