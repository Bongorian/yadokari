#pragma once
#include "fx_registry.h"
#include <stdint.h>
namespace groove {
enum class SampleMode:uint8_t {OneShot,Gate,Loop,PingPong,Stretch};
struct SampleZone {uint8_t root=60,key_low=0,key_high=127,velocity_low=1,velocity_high=127;};
struct SampleEdit {
 float start=0,end=1,speed=1,fade_ms=2,crossfade_ms=4,attack_ms=0,release_ms=8;
 SampleMode mode=SampleMode::OneShot;uint8_t choke=0,slice_count=0;
 float markers[65]{};uint8_t pads[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
 uint8_t zone_count=0;SampleZone zones[16]{};uint8_t slice_warp[64]{};
};
inline bool valid_sample_edit(const SampleEdit& s){if(!(s.start>=0 && s.start<1 && s.end>0 && s.end<=1 && s.start<s.end && s.speed>=0.125f && s.speed<=8 && s.fade_ms>=0 && s.fade_ms<=100 && s.crossfade_ms>=0 && s.crossfade_ms<=100 && s.attack_ms>=0 && s.attack_ms<=2000 && s.release_ms>=0 && s.release_ms<=4000) || unsigned(s.mode)>4 || s.choke>16 || s.slice_count>64 || s.zone_count>16 || (s.zone_count && s.zone_count!=s.slice_count))return false;for(unsigned i=0;i<=s.slice_count;++i)if(!(s.markers[i]>=0 && s.markers[i]<=1) || (i && s.markers[i]<=s.markers[i-1]))return false;for(auto p:s.pads)if(p>=64)return false;for(auto w:s.slice_warp)if(w>1)return false;for(unsigned i=0;i<s.zone_count;++i){const auto& z=s.zones[i];if(z.root>127 || z.key_low>z.key_high || z.key_high>127 || !z.velocity_low || z.velocity_low>z.velocity_high || z.velocity_high>127)return false;}return true;}
inline unsigned select_sample_zone(const SampleEdit& edit,unsigned note,unsigned velocity,unsigned round_robin=0){unsigned best=0,matches=0;int best_score=0x7fffffff;for(unsigned i=0;i<edit.zone_count;++i){const auto& z=edit.zones[i];int key=note<z.key_low?int(z.key_low-note):note>z.key_high?int(note-z.key_high):0;int vel=velocity<z.velocity_low?int(z.velocity_low-velocity):velocity>z.velocity_high?int(velocity-z.velocity_high):0;int root=note>z.root?int(note-z.root):int(z.root-note);int score=key*256+vel*4+root;if(score<best_score){best_score=score;best=i;matches=1;}else if(score==best_score){++matches;if(round_robin%matches==matches-1)best=i;}}return best;}
struct SampleMetadata {uint32_t rate=0,frames=0,content_crc=0;uint16_t bits=0,bpm=0;uint8_t channels=0,root=60;bool favorite=false,missing=false;};
inline bool same_sample_metadata(const SampleMetadata& a,const SampleMetadata& b){return a.rate==b.rate && a.frames==b.frames && a.content_crc==b.content_crc && a.bits==b.bits && a.bpm==b.bpm && a.channels==b.channels && a.root==b.root && a.favorite==b.favorite && a.missing==b.missing;}
// Capture IDs 3..18 retain post-fader compatibility. Track taps each
// cover 16 tracks; returns and performance are independent stereo buses.
constexpr unsigned kCaptureMax=60;
inline unsigned capture_track(unsigned source){return source>=3 && source<=50?(source-3)%16:16;}
inline unsigned capture_track_tap(unsigned source){return source>=3 && source<=50?(source-3)/16:3;}
inline unsigned capture_kind(unsigned source){return source<3?source:source<=50?3+capture_track_tap(source):source==51?6:source==52?7:8;}
struct GroupBus {uint16_t members=0;float gain=1;uint8_t output=1;float insert_fx[8]={0,0.5f,0.25f,1,0,0.5f,0.25f,1};float send[2]={0,0};float parallel=0;};
struct RoutingState {float main=1,cue=0.7f,preview=0.65f,monitor=0;uint8_t monitor_bus=2,capture_bus=0;GroupBus groups[8]{};};
inline bool valid_routing(const RoutingState& r){if(!(r.main>=0 && r.main<=1 && r.cue>=0 && r.cue<=1 && r.preview>=0 && r.preview<=1 && r.monitor>=0 && r.monitor<=1 && r.monitor_bus<=3 && r.capture_bus<=kCaptureMax))return false;uint16_t seen=0;for(const auto& g:r.groups){if(!(g.gain>=0 && g.gain<=1) || g.output>3 || (seen&g.members) || !(g.send[0]>=0 && g.send[0]<=1 && g.send[1]>=0 && g.send[1]<=1 && g.parallel>=0 && g.parallel<=1))return false;for(unsigned slot=0;slot<2;++slot){const float* fx=g.insert_fx+slot*4;if(!(fx[0]>=0 && fx[0]<kFxCount && fx[0]==float(unsigned(fx[0]))) || !(fx[1]>=0 && fx[1]<=1 && fx[2]>=0 && fx[2]<=1 && fx[3]>=0 && fx[3]<=1))return false;}seen|=g.members;}return true;}
struct ProjectUi {uint8_t theme=0,motion=2,order=0,strip=1;bool preview565=true,note_quantized=true,note_replace=false,metronome=false,count_in=false,click_main=false;uint8_t arp_direction=0,arp_octaves=1,repeat_gate=100,repeat_rate=2,chord_shape=0,chord_inversion=0;uint16_t wave_profile_mask=0,wave_clock_mask=0;int8_t keyboard_octave[16]{};};
}
