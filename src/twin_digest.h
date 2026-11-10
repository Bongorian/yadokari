#pragma once
#include "content_shelf.h"
namespace groove {
// File/UI owner only: reuse the canonical preset digest and add the currently
// effective performance context. Disabled routes and inactive Morph sets do not
// invalidate a rendered instrument. This never runs in the DSP callback.
inline bool twin_content_digest(const SoundState& sound,const ProjectState& p,unsigned track,uint8_t (&digest)[32]){
 if(track>=kTracks)return false;
 uint8_t base[32];if(!preset_content_digest(sound,p.assets,base))return false;
 ChunkBuffer context;ChunkWriter<ChunkBuffer> w(context);if(!w.block(base,32) || !w.u8(p.morph_enabled))return false;
 if(p.morph_enabled && (!w.f32(p.morph_position) || !w.u8(p.morph_set) || !write_morph_snapshot(w,p.morph_a[track]) || !write_morph_snapshot(w,p.morph_b[track])))return false;
 for(const auto& macro:p.macros){bool active=false;for(const auto& route:macro.routes)active|=macro.active && route.enabled && (route.tracks&(1u<<track));if(!w.u8(active))return false;if(!active)continue;if(!w.f32(macro.value))return false;
  for(const auto& route:macro.routes){bool use=route.enabled && (route.tracks&(1u<<track));if(!w.u8(use))return false;if(use && (!write_parameter_target(w,route.parameter,&p.unavailable_targets) || !w.f32(route.low) || !w.f32(route.high) || !w.u8(uint8_t(route.mode)) || !w.u8(uint8_t(route.curve)) || !w.u8(uint8_t(route.source))))return false;}
 }
 const auto& controls=p.tracks[track];for(unsigned i=0;i<16;++i)if(!w.u8(controls.midi_cc_used[i]))return false;for(unsigned i=0;i<128;++i)if((controls.midi_cc_used[i/8]&(1u<<(i%8))) && !w.u8(controls.midi_cc[i]))return false;
 update::Sha256::hash(context.data,context.size,digest);return true;
}
}
