#pragma once
#include <stdint.h>
#ifndef GROOVE_DETAILED_RENDER_ATTRIBUTION
#define GROOVE_DETAILED_RENDER_ATTRIBUTION 1
#endif
namespace groove {
// Diagnostic clocks only. All mutable state belongs to the single DSP owner.
class RenderAudit {
public:
 using Clock=uint32_t(*)();
 enum Reset:unsigned {PerformanceReset,GroupReset,DelayReset,ReverbReset,TrackReset,SnapshotReset,ResetCount};
 enum EventWork:unsigned {MountValidate,MountCopy,MountPublish,ClipReset,StepWork,TriggerWork,EventCount};
 struct EventCounts {uint32_t us[EventCount]{};};
 struct Sample {uint32_t begin=0,total=0,control=0,commands=0,visual=0,command_count=0,top_us=0,top_kind=0,top_track=0,generation_before=0,generation_after=0,events=0,tracks=0,output=0,reset[ResetCount]{};};
 void begin(Clock clock,uint32_t at,bool ordinary){armed_=clock;clock_=ordinary?clock:nullptr;sample_={};sample_.begin=at;}
 void generation(unsigned value){sample_.generation_before=value;}
 uint32_t command_begin(){if(!clock_)clock_=armed_;return clock_?clock_():0;}
 void command_end(uint32_t at,unsigned kind,unsigned track){if(!clock_)return;unsigned us=clock_()-at;sample_.commands+=us;++sample_.command_count;if(us>sample_.top_us){sample_.top_us=us;sample_.top_kind=kind;sample_.top_track=track;}}
 void control_end(){if(clock_){mark_=clock_();sample_.control=mark_-sample_.begin;}}
 void cut(unsigned stage){if(!clock_)return;unsigned now=clock_(),us=now-mark_;mark_=now;if(stage==0)sample_.events+=us;else if(stage==1)sample_.tracks+=us;else sample_.output+=us;}
 Clock clock()const{return clock_;}
 void visual(unsigned elapsed){sample_.visual+=elapsed;}
 void reset(unsigned kind,unsigned elapsed){if(kind<ResetCount)sample_.reset[kind]+=elapsed;}
 void stages(unsigned events,unsigned tracks,unsigned output){if(clock_){sample_.events=events;sample_.tracks=tracks;sample_.output=output;}}
 void finish(unsigned generation){if(clock_)sample_.total=clock_()-sample_.begin;sample_.generation_after=generation;clock_=armed_=nullptr;}
 const Sample& sample()const{return sample_;}
private:Clock armed_=nullptr,clock_=nullptr;Sample sample_{};unsigned mark_=0;
};
class RenderResetScope {
 RenderAudit& audit_;RenderAudit::Clock clock_;unsigned kind_,at_;
public:RenderResetScope(RenderAudit& a,unsigned kind):audit_(a),clock_(a.clock()),kind_(kind),at_(clock_?clock_():0){}
 ~RenderResetScope(){if(clock_)audit_.reset(kind_,clock_()-at_);}
};
// These event scopes are inclusive: TriggerWork can be nested within StepWork.
class RenderEventScope {
 RenderAudit::EventCounts& counts_;RenderAudit::Clock clock_;unsigned kind_,at_;
public:
 RenderEventScope(RenderAudit& a,RenderAudit::EventCounts& counts,unsigned kind):counts_(counts),clock_(a.clock()),kind_(kind),at_(clock_?clock_():0){}
 ~RenderEventScope(){if(clock_)counts_.us[kind_]+=clock_()-at_;}
};
}
