#pragma once
#include <stdint.h>
#include <math.h>
namespace groove {
// Project-wide track roles: arrangement audio never consumes step triggers.
constexpr unsigned kAudioLanes=4,kAudioClips=8,kLoopMaxBars=64,kLoopFreeSeconds=300;
enum class TrackKind:uint8_t {Instrument,Looper,Audio};
enum class LoopLength:uint8_t {Bars,Pattern,Free};
struct AudioClip {uint32_t tick=0,first=0,frames=0;float gain=1;uint8_t asset=255;};
struct AudioLane {
 TrackKind kind=TrackKind::Instrument;LoopLength length=LoopLength::Bars;
 uint8_t bars=1,source=1;bool enabled=true;float gain=0.8f,pan=0,feedback=1;
 AudioClip clips[kAudioClips]{};
};
// Later slots win placements sharing the same start tick.
inline int audio_clip_at(const AudioLane& lane,uint32_t tick,uint32_t since=0,bool after=false){
 int selected=-1;
 for(unsigned i=0;i<kAudioClips;++i){
  const auto& c=lane.clips[i];
  if(c.asset<64 && c.tick<=tick && (!after || c.tick>since) &&
     (selected<0 || c.tick>=lane.clips[selected].tick))selected=int(i);
 }
 return selected;
}
inline bool valid_audio_lane(const AudioLane& lane){
 if(unsigned(lane.kind)>2 || unsigned(lane.length)>2 || !lane.bars || lane.bars>kLoopMaxBars || lane.source>60 || !isfinite(lane.gain) || lane.gain<0 || lane.gain>2 || !isfinite(lane.pan) || fabsf(lane.pan)>1 || !isfinite(lane.feedback) || lane.feedback<0 || lane.feedback>1)return false;
 for(const auto& c:lane.clips)if((c.asset!=255 && (c.asset>=64 || !c.frames)) || c.tick>3932160 || c.first>0xffffffffu-c.frames || !isfinite(c.gain) || c.gain<0 || c.gain>2)return false;
 return true;
}
inline bool valid_audio_lanes(const AudioLane* lanes){unsigned n=0;for(unsigned t=0;t<16;++t){if(!valid_audio_lane(lanes[t]))return false;if(lanes[t].kind!=TrackKind::Instrument)++n;}return n<=kAudioLanes;}
inline uint32_t audio_bar_frames(unsigned rate,unsigned tempo){return tempo?uint32_t((uint64_t(rate)*240+tempo/2)/tempo):0;}
inline uint32_t quantized_loop_frames(uint32_t frames,unsigned rate,unsigned tempo){uint32_t bar=audio_bar_frames(rate,tempo);if(!bar)return 0;uint64_t bars=(uint64_t(frames)+bar/2)/bar;if(!bars)bars=1;if(bars>0xffffffffu/bar)bars=0xffffffffu/bar;return uint32_t(bars*bar);}
}
