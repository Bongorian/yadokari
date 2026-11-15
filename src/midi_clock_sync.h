#pragma once
#include <stdint.h>
namespace groove {
// Phase lock for an external 24 PPQN MIDI clock (slave mode).
//
// The engine keeps its integer tempo and its sample-domain step grid. Once per
// audio sample this class says how many transport samples that audio sample is
// worth: 1 nominally, 2 to catch up, 0 to hold. The engine applies the pace to
// every tempo accumulator and moves the step grid by the difference, so the
// whole musical timeline is steered as one and the internal clock stays
// untouched when the mode is off.
//
// Position is counted, not estimated: each received tick is one tick of master
// time and each internal tick boundary is one tick of local time. A PI loop
// removes the residual error, the first tick after Start / Continue is aligned
// at once, and a silent clock leaves the transport on its last locked pace
// (flywheel) until the clock returns. Audio owner only; times are samples.
class MidiClockSync {
public:
 enum State : uint8_t { Free, Waiting, Locked };
 void reset(){*this=MidiClockSync{};}
 State state()const{return waiting_?Waiting:locked_?Locked:Free;}
 bool present(uint64_t now)const{return have_tick_ && now-last_tick_<=timeout_;}
 // The learned difference between the master's tempo and the nominal one.
 double trim()const{return integral_;}
 // The transport sits on a tick boundary (Start, Song Position): the next
 // received tick is exactly this position.
 void seek(){delta_=-1;counted_=true;measure_=false;}
 // The transport starts running. A non-zero wait holds it for that many
 // samples at most, so the first tick can start it on the sample.
 void arm(uint32_t wait){waiting_=wait!=0;wait_left_=wait;locked_=false;measure_=false;catchup_=0;accumulator_=0;set_pace(integral_);}
 // Keeps the counted position: Stop followed by Continue resumes in place.
 void stop(){waiting_=false;locked_=false;measure_=false;catchup_=0;accumulator_=0;set_pace(integral_);}
 // The internal clock crossed a tick boundary.
 void wrapped(){--delta_;}
 // The nominal tempo moved under the loop. With an estimate (tempo x 256) the
 // master changed tempo: start from it instead of unwinding the old pace.
 // Without one only the rounding moved: the loop's own pace is finer than any
 // estimate and carries over unchanged.
 void retune(unsigned from,unsigned to,unsigned estimate_q8){
  if(!from || !to || from==to)return;
  integral_=limit(estimate_q8?double(estimate_q8)/(256.0*double(to))-1.0:(1.0+integral_)*double(from)/double(to)-1.0,kIntegralLimit);set_pace(integral_);
 }
 // A tick arrived. fraction: progress [0,1) through the current internal tick.
 // tick_samples: one tick at the nominal tempo. floor: shortest loss timeout.
 // Every tick counts; the phase is read once per sample in advance(), so ticks
 // delivered together are one measurement taken at the last of them.
 void tick(uint64_t now,bool running,double fraction,double tick_samples,uint32_t floor){
  const bool gap=!have_tick_ || now-last_tick_>timeout_;
  last_tick_=now;have_tick_=true;
  const double window=tick_samples*4;timeout_=window>double(floor)?uint64_t(window)+1:floor;
  if(!running)return;
  if(locked_ && gap){locked_=false;counted_=false;}
  ++delta_;fraction_=fraction;tick_samples_=tick_samples;measure_=true;
 }
 // Transport samples for this audio sample while the transport is running.
 unsigned advance(uint64_t now){
  if(measure_){measure_=false;measured();}
  if(waiting_){if(wait_left_ && --wait_left_)return 0;waiting_=false;}
  if(locked_ && now-last_tick_>timeout_){locked_=false;counted_=false;catchup_=0;set_pace(integral_);}
  if(catchup_>0){--catchup_;return 2;}
  if(catchup_<0){++catchup_;return 0;}
  accumulator_+=step_;
  if(accumulator_>=kOne){accumulator_-=kOne;return 2;}
  if(accumulator_<=-kOne){accumulator_+=kOne;return 0;}
  return 1;
 }
private:
 // Double poles at 0.95 per tick: lock in about two beats, and delivery jitter
 // of a few milliseconds per tick is attenuated by an order of magnitude.
 static constexpr double kProportional=0.0975,kIntegral=0.0025,kIntegralLimit=0.25,kPaceLimit=0.5,kSnap=0.5,kSlip=8.0;
 static constexpr int64_t kOne=int64_t(1)<<32;
 static double limit(double value,double bound){return value>bound?bound:value<-bound?-bound:value;}
 static int64_t whole(double value){return int64_t(value<0?value-0.5:value+0.5);}
 void set_pace(double pace){pace_=limit(pace,kPaceLimit);step_=int64_t(pace_*4294967296.0);}
 void nearest(){delta_=fraction_>=0.5?1:0;}
 void measured(){
  double error=double(delta_)-fraction_;
  if(!locked_){
   // A counted position survives Stop / Continue and a bounded start wait.
   // Without one, or with a count that is clearly stale, lock to the nearest
   // tick. Either way the transport is brought onto the tick at once.
   if(!counted_ || error<-1.0 || error>kSlip){nearest();error=double(delta_)-fraction_;}
   counted_=true;locked_=true;waiting_=false;accumulator_=0;set_pace(integral_);
   catchup_=whole(error*tick_samples_);return;
  }
  // Too far to chase: the count itself is wrong, keep only the tick phase.
  if(error>kSlip || error<-kSlip){nearest();error=double(delta_)-fraction_;}
  // Coarse: an error the loop should not carry for beats is taken out at once,
  // and a correction still in flight is replaced by the fresh reading.
  if(catchup_ || error>kSnap || error<-kSnap){catchup_=whole(error*tick_samples_);set_pace(integral_);return;}
  integral_=limit(integral_+kIntegral*error,kIntegralLimit);
  set_pace(kProportional*error+integral_);
 }
 uint64_t last_tick_=0,timeout_=0;int64_t catchup_=0,accumulator_=0,step_=0;double integral_=0,pace_=0,fraction_=0,tick_samples_=0;
 int32_t delta_=-1;uint32_t wait_left_=0;bool have_tick_=false,waiting_=false,locked_=false,counted_=false,measure_=false;
};
}
