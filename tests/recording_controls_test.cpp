#include "recording_controls.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace groove;
int main(){
 RecordingContext c;c.shift=true;c.audio=true;
 assert(!recording_button_control(4,c).enabled && std::strstr(recording_hint(c,0),"PLAY"));
 c.playing=true;assert(recording_button_control(4,c).action==RecordingAction::Resample && recording_button_control(4,c).enabled);
 c.file_busy=true;assert(!recording_button_control(4,c).enabled && !recording_source_control(c).enabled && !recording_button_control(5,c).enabled);
 c.file_busy=false;c.scene_take=true;assert(!resample_start_allowed(c));c.scene_take=false;c.audio=false;assert(!resample_start_allowed(c));c.audio=true;
 c.pending=true;assert(!resample_start_allowed(c) && !recording_source_control(c).enabled);c.resampling=c.live=true;
 // Cancel has its own priority request even when an ordinary file action is pending.
 assert(recording_button_control(4,c).action==RecordingAction::Cancel && recording_button_control(4,c).enabled);
 c.cancelling=true;assert(!recording_button_control(4,c).enabled && !std::strcmp(recording_button_control(4,c).label,"CANCELLING"));
 c={};c.source=60;auto d=recording_source_control(c);assert(d.adjusted(59,100)==60 && d.adjusted(1,-100)==0 && d.normalized(60)==1);
 for(unsigned bars=1;bars<=4;++bars){c.bars=bars;c.shift=true;assert(!std::strcmp(recording_button_control(5,c).label,resample_bar_label(bars)));}
 c.shift=false;c.source=0;assert(!std::strcmp(recording_button_control(5,c).label,"OVERDUB IN"));c.source=53;assert(!std::strcmp(recording_button_control(5,c).label,"OVERDUB SRC"));
 c={};c.audio=true;assert(sampler_recording_control(0,c).enabled && sampler_recording_control(2,c).enabled);c.live=true;c.origin=RecordingOrigin::Source;c.source=1;c.pending=true;assert(!sampler_recording_selected(0,c) && sampler_recording_selected(2,c));assert(sampler_recording_control(0,c).action==RecordingAction::Stop && sampler_recording_control(0,c).enabled);c.stopping=true;assert(!sampler_recording_control(2,c).enabled);assert(!std::strcmp(sampler_recording_control(2,c).label,"STOPPING"));
 c={};c.shift=true;c.quantized=true;assert(!recording_button_selected(4,c,false));c.shift=false;assert(recording_button_selected(4,c,false));
 std::puts("Recording controls: eligibility, pending/cancel priority, source range, bars and overdub labels passed");
}
