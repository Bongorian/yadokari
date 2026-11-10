#pragma once
#include <circle/timer.h>
// Core 0 only. Allow the no-filesystem UI pump at explicit, stable recording
// I/O boundaries. Never call SessionFiles, FatFs, hotplug or update processing
// recursively. DSP ownership remains on core 3.
class StorageUiWait {
 inline static void (*pump_)(void*)=nullptr;
 inline static void* context_=nullptr;
 inline static unsigned depth_=0;
 inline static bool running_=false;
 inline static uint64_t last_=0;
public:
 inline static uint64_t pump_us=0;
 static void bind(void (*pump)(void*),void* context){pump_=pump;context_=context;}
 static void poll(){uint64_t now=CTimer::GetClockTicks64();if(!depth_ || running_ || !pump_ || now-last_<200)return;last_=now;running_=true;pump_(context_);pump_us+=CTimer::GetClockTicks64()-now;running_=false;}
 struct Scope {Scope(){++depth_;}~Scope(){--depth_;}};
};
