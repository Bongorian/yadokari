#pragma once
#include <stdint.h>
#include <math.h>
namespace groove { namespace ui {
// UI-owned state. Both the strip marker and the number use held, never a
// second peak accumulator. Time is monotonic UI milliseconds, not draw count.
struct MeterBallistics {
 float live=0,held=0;uint64_t live_until=0,peak_until=0;
 void update(float input,uint64_t now,uint64_t elapsed){
  if(!isfinite(input) || input<0)input=0;
  if(input>=live){live=input;live_until=now+180;}
  else if(now>=live_until)live+=(input-live)*fminf(1,float(elapsed)/180);
  if(input>=held){held=input;peak_until=now+1000;}
  else if(now>peak_until){uint64_t since=now-peak_until,dt=elapsed<since?elapsed:since;held=fmaxf(input,held*expf(-float(dt)*.0010361633f));} // 9 dB/s
  if(held<.0000001f)held=0;
 }
};
}}
