#pragma once
#include <stdint.h>
namespace groove {
// Poll from the UI/USB task; the audio callback never waits on this monitor.
class AudioDrainMonitor {
 bool waiting_=false;uint32_t since_=0;
public:
 bool update(bool present,bool active,bool running,uint32_t now,uint32_t threshold){
  if(!present || !active || running){waiting_=false;return false;}
  if(!waiting_){waiting_=true;since_=now;return threshold==0;}
  return uint32_t(now-since_)>=threshold;
 }
};
}
