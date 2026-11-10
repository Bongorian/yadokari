#pragma once
#include "engine_state_types.h"
#include "chunk_io.h"
namespace groove {
inline constexpr uint32_t kChunkTxtr=0x52545854u; // TXTR revision 2: fourteen preset floats; INPUT is a persistent track field
inline constexpr uint32_t kChunkReso=0x4f534552u; // RESO revision 1: eight float parameters
inline constexpr uint32_t kChunkParm=0x4d524150u; // "PARM"
inline constexpr uint32_t kChunkSmpl=0x4c504d53u; // "SMPL"
inline constexpr uint32_t kChunkCrea=0x41455243u; // "CREA"
inline constexpr uint32_t kChunkKit=0x2054494bu;  // "KIT "
template<class W> bool write_warp(W& w,const SampleEdit& e){if(!w.u8(e.slice_count))return false;for(unsigned i=0;i<e.slice_count;++i)if(!w.u8(e.slice_warp[i]))return false;return true;}
template<class R> bool read_warp(R& r,SampleEdit& e){for(auto& v:e.slice_warp)v=false;if(r.at_end())return true;uint8_t count;if(!r.u8(count)||count>64)return false;for(unsigned i=0;i<count;++i){uint8_t v;if(!r.u8(v)||v>1)return false;e.slice_warp[i]=v;}return true;}
template<class W> bool write_sample_edit(W& w,const SampleEdit& e){if(!w.f32(e.start)||!w.f32(e.end)||!w.f32(e.speed)||!w.f32(e.fade_ms)||!w.f32(e.crossfade_ms)||!w.f32(e.attack_ms)||!w.f32(e.release_ms)||!w.u8(uint8_t(e.mode))||!w.u8(e.choke)||!w.u8(e.slice_count))return false;for(float v:e.markers)if(!w.f32(v))return false;for(auto v:e.pads)if(!w.u8(v))return false;return true;}
template<class R> bool read_sample_edit(R& r,SampleEdit& e){uint8_t mode;if(!r.f32(e.start)||!r.f32(e.end)||!r.f32(e.speed)||!r.f32(e.fade_ms)||!r.f32(e.crossfade_ms)||!r.f32(e.attack_ms)||!r.f32(e.release_ms)||!r.u8(mode)||!r.u8(e.choke)||!r.u8(e.slice_count))return false;e.mode=SampleMode(mode);for(float& v:e.markers)if(!r.f32(v))return false;for(auto& v:e.pads)if(!r.u8(v))return false;return valid_sample_edit(e);}
inline bool same_kit_step(const KitStepState& a,const KitStepState& b){return a.enabled==b.enabled && a.velocity==b.velocity && a.probability==b.probability && a.ratchet==b.ratchet && a.sample_lock==b.sample_lock && a.start_lock==b.start_lock && a.filter_lock==b.filter_lock && a.reverse_lock==b.reverse_lock && a.pitch_lock==b.pitch_lock && a.microtiming==b.microtiming && a.level_lock==b.level_lock && a.delay_lock==b.delay_lock && a.reverb_lock==b.reverb_lock && a.fx_lock[0]==b.fx_lock[0] && a.fx_lock[1]==b.fx_lock[1] && a.fx_lock[2]==b.fx_lock[2] && a.fx_lock[3]==b.fx_lock[3];}
template<class W> bool write_kit_definition(W& w,const KitDefinition& kit){
 for(const auto& p:kit.pads){if(!w.u8(p.sample[0])||!w.u8(p.sample[1])||!w.u8(p.velocity_split)||!w.u8(p.layer_mode)||!w.u8(p.muted)||!w.u8(p.reverse)||!w.f32(p.level)||!w.f32(p.pan)||!w.f32(p.filter)||!w.f32(p.delay_send)||!w.f32(p.reverb_send)||!write_sample_edit(w,p.edit)||!w.u8(p.edit.zone_count))return false;for(unsigned i=0;i<p.edit.zone_count;++i){const auto& z=p.edit.zones[i];if(!w.u8(z.root)||!w.u8(z.key_low)||!w.u8(z.key_high)||!w.u8(z.velocity_low)||!w.u8(z.velocity_high))return false;}}
 for(const auto& pad:kit.pads){if(!write_warp(w,pad.edit))return false;for(float value:pad.insert_fx)if(!w.f32(value))return false;}
 return w.u16(kit.solo_mask);
}
template<class R> bool read_kit_definition(R& r,KitDefinition& kit){
 for(auto& p:kit.pads){p.insert_fx[0]=0;p.insert_fx[1]=0.5f;p.insert_fx[2]=0.25f;p.insert_fx[3]=1;p.edit.zone_count=0;uint8_t mute,reverse;if(!r.u8(p.sample[0])||!r.u8(p.sample[1])||!r.u8(p.velocity_split)||!r.u8(p.layer_mode)||!r.u8(mute)||mute>1||!r.u8(reverse)||reverse>1||!r.f32(p.level)||!r.f32(p.pan)||!r.f32(p.filter)||!r.f32(p.delay_send)||!r.f32(p.reverb_send)||!read_sample_edit(r,p.edit)||!r.u8(p.edit.zone_count)||p.edit.zone_count>16)return false;p.muted=mute;p.reverse=reverse;for(unsigned i=0;i<p.edit.zone_count;++i){auto& z=p.edit.zones[i];if(!r.u8(z.root)||!r.u8(z.key_low)||!r.u8(z.key_high)||!r.u8(z.velocity_low)||!r.u8(z.velocity_high))return false;}}
 for(auto& pad:kit.pads){if(!read_warp(r,pad.edit))return false;for(float& value:pad.insert_fx)if(!r.f32(value))return false;}
 return r.u16(kit.solo_mask) && valid_kit_definition(kit);
}
template<class W> bool write_kit_sequence(W& w,const KitSequence& kit){
 const KitStepState empty{};
 for(const auto& lane:kit.steps){uint8_t bits[kSteps/8]{};for(unsigned i=0;i<kSteps;++i)if(!same_kit_step(lane[i],empty))bits[i/8]|=uint8_t(1u<<(i%8));for(uint8_t b:bits)if(!w.u8(b))return false;for(unsigned i=0;i<kSteps;++i)if(bits[i/8]&(1u<<(i%8))){const auto& s=lane[i];if(!w.u8(s.enabled)||!w.u8(s.velocity)||!w.u8(s.probability)||!w.u8(s.ratchet)||!w.u8(s.sample_lock)||!w.u8(s.start_lock)||!w.u8(s.filter_lock)||!w.u8(s.reverse_lock)||!w.u8(uint8_t(s.pitch_lock))||!w.u8(uint8_t(s.microtiming))||!w.u8(s.level_lock)||!w.u8(s.delay_lock)||!w.u8(s.reverb_lock))return false;for(uint8_t lock:s.fx_lock)if(!w.u8(lock))return false;}}
 return true;
}
template<class R> bool read_kit_sequence(R& r,KitSequence& kit){
 for(auto& lane:kit.steps){uint8_t bits[kSteps/8];for(auto& b:bits)if(!r.u8(b))return false;for(unsigned i=0;i<kSteps;++i){lane[i]=KitStepState{};if(bits[i/8]&(1u<<(i%8))){auto& s=lane[i];uint8_t pitch,micro;if(!r.u8(s.enabled)||!r.u8(s.velocity)||!r.u8(s.probability)||!r.u8(s.ratchet)||!r.u8(s.sample_lock)||!r.u8(s.start_lock)||!r.u8(s.filter_lock)||!r.u8(s.reverse_lock)||!r.u8(pitch)||!r.u8(micro))return false;s.pitch_lock=int8_t(pitch);s.microtiming=int8_t(micro);if(!r.u8(s.level_lock)||!r.u8(s.delay_lock)||!r.u8(s.reverb_lock))return false;for(auto& lock:s.fx_lock)if(!r.u8(lock))return false;}}}
 return valid_kit_sequence(kit);
}
inline bool kit_sequence_present(const KitSequence& kit){const KitStepState empty{};for(const auto& lane:kit.steps)for(const auto& step:lane)if(!same_kit_step(step,empty))return true;return false;}
inline bool same_sample_edit(const SampleEdit& a,const SampleEdit& b){
 if(a.start!=b.start || a.end!=b.end || a.speed!=b.speed || a.fade_ms!=b.fade_ms || a.crossfade_ms!=b.crossfade_ms || a.attack_ms!=b.attack_ms || a.release_ms!=b.release_ms || a.mode!=b.mode || a.choke!=b.choke || a.slice_count!=b.slice_count || a.zone_count!=b.zone_count)return false;
 for(unsigned i=0;i<65;++i)if(a.markers[i]!=b.markers[i])return false;
 for(unsigned i=0;i<64;++i)if(a.slice_warp[i]!=b.slice_warp[i])return false;
 for(unsigned i=0;i<16;++i)if(a.pads[i]!=b.pads[i])return false;
 for(unsigned i=0;i<a.zone_count;++i){const auto& x=a.zones[i];const auto& y=b.zones[i];if(x.root!=y.root || x.key_low!=y.key_low || x.key_high!=y.key_high || x.velocity_low!=y.velocity_low || x.velocity_high!=y.velocity_high)return false;}
 return true;
}
inline bool same_creative(const CreativeState& a,const CreativeState& b){
 for(unsigned l=0;l<6;++l){if(a.length[l]!=b.length[l])return false;for(unsigned i=0;i<8;++i)if(a.value[l][i]!=b.value[l][i])return false;}
 return a.follower_source==b.follower_source && a.follower_amount==b.follower_amount && a.seed==b.seed && a.mutation==b.mutation;
}
inline bool write_zones(ChunkWriter<ChunkBuffer>& w,const SampleEdit& e){
 if(!w.u8(e.zone_count))return false;
 for(unsigned i=0;i<e.zone_count;++i){const auto& z=e.zones[i];if(!w.u8(z.root)||!w.u8(z.key_low)||!w.u8(z.key_high)||!w.u8(z.velocity_low)||!w.u8(z.velocity_high))return false;}
 return true;
}
inline bool write_creative_block(ChunkWriter<ChunkBuffer>& w,const CreativeState& c){
 for(uint8_t n:c.length)if(!w.u8(n))return false;
 for(const auto& lane:c.value)for(uint8_t n:lane)if(!w.u8(n))return false;
 return w.u8(c.follower_source)&&w.u8(uint8_t(c.follower_amount))&&w.u32(c.seed)&&w.u8(c.mutation);
}
inline bool read_zones(ChunkReader<ChunkSource>& r,SampleEdit& e){
 if(!r.u8(e.zone_count)||e.zone_count>16)return false;
 for(unsigned i=0;i<e.zone_count;++i){auto& z=e.zones[i];if(!r.u8(z.root)||!r.u8(z.key_low)||!r.u8(z.key_high)||!r.u8(z.velocity_low)||!r.u8(z.velocity_high))return false;}
 return true;
}
inline bool read_creative_block(ChunkReader<ChunkSource>& r,CreativeState& c){
 for(uint8_t& n:c.length)if(!r.u8(n))return false;
 for(auto& lane:c.value)for(uint8_t& n:lane)if(!r.u8(n))return false;
 uint8_t amount;if(!r.u8(c.follower_source)||!r.u8(amount)||!r.u32(c.seed)||!r.u8(c.mutation))return false;
 c.follower_amount=int8_t(amount);return true;
}

// Views separate portable engine payloads from their current track/pool owners.
// Track and pool instances share one portable payload contract.
template<template<class> class Q> struct BasicEngineStateView {
 Q<TextureState>* texture;Q<ResonatorState>* resonator;
 Q<uint8_t>* model;Q<float>* parameters;Q<uint8_t>* sample_slot;
 Q<SampleEdit>* sample_edit;Q<CreativeState>* creative;Q<KitDefinition>* kit;
 Q<bool>* sample_loop;Q<bool>* sample_reverse;
 Q<uint8_t>* sample_slice;Q<uint8_t>* sample_slices;Q<char>* preset_name=nullptr;
 Q<float>* daisy_adsr=nullptr;Q<uint8_t>* midi_program=nullptr;Q<uint8_t>* midi_cc=nullptr;Q<uint8_t>* midi_cc_used=nullptr;Q<WavetableState>* wavetable=nullptr;Q<VirtualAnalogState>* virtual_analog=nullptr;Q<Fm6State>* fm6=nullptr;Q<AdditiveState>* additive=nullptr;Q<PhaseDistState>* phase_dist=nullptr;Q<SoundFontState>* soundfont=nullptr;Q<LayerState>* layer=nullptr;Q<FormantState>* formant=nullptr;Q<GranularState>* granular=nullptr;
};
template<class T> using EngineMutable=T;
template<class T> using EngineConst=const T;
using EngineStateView=BasicEngineStateView<EngineMutable>;
using EngineStateSource=BasicEngineStateView<EngineConst>;
struct EngineChunkDescriptor {
 uint32_t fourcc;uint16_t version;
 bool (*emit)(const EngineStateSource&,bool required);
 bool (*encode)(ChunkBuffer&,const EngineStateSource&);
 bool (*decode)(ChunkSource&,const EngineStateView&);
 void (*defaults)(const EngineStateView&);
};
enum EngineChunkRequirement:uint8_t {RequireResonator=1,RequireTexture=2,RequireParameters=4,RequireSampler=8,RequireCreative=16,RequireKit=32,RequireEnvelope=64,RequireMidi=128};
struct EngineStateContract {
 const EngineChunkDescriptor* chunks;uint8_t count,required;
};
inline bool emit_reso(const EngineStateSource& s,bool required){const ResonatorState initial{};for(unsigned i=0;i<8;++i)if(s.resonator->values[i]!=initial.values[i])return true;return required;}
inline bool encode_reso(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);for(float v:s.resonator->values)if(!w.f32(v))return false;return true;}
inline bool decode_reso(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);for(float& v:s.resonator->values)if(!r.f32(v))return false;return src.at_end() && valid_resonator(*s.resonator);}
inline void defaults_reso(const EngineStateView& s){*s.resonator=ResonatorState{};}
inline bool emit_txtr(const EngineStateSource& s,bool required){const TextureState initial{};for(unsigned i=0;i<15;++i)if(i!=12 && s.texture->values[i]!=initial.values[i])return true;return required;}
inline bool encode_txtr(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);for(unsigned i=0;i<15;++i)if(i!=12 && !w.f32(s.texture->values[i]))return false;return true;}
inline bool decode_txtr(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);for(unsigned i=0;i<15;++i)if(i!=12 && !r.f32(s.texture->values[i]))return false;return src.at_end() && valid_texture(*s.texture);}
inline void defaults_txtr(const EngineStateView& s){float input=s.texture->values[12];*s.texture=TextureState{};s.texture->values[12]=input;}
inline bool emit_parm(const EngineStateSource&,bool){return true;}
inline bool encode_parm(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);if(!w.u8(*s.model))return false;for(unsigned i=0;i<5;++i)if(!w.f32(s.parameters[i]))return false;return w.u8(*s.sample_slot);}
inline bool decode_parm(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);if(!r.u8(*s.model))return false;for(unsigned i=0;i<5;++i)if(!r.f32(s.parameters[i]))return false;return r.u8(*s.sample_slot) && src.at_end();}
inline void defaults_parm(const EngineStateView& s){*s.model=0;for(unsigned i=0;i<5;++i)s.parameters[i]=0.5f;*s.sample_slot=0;}
inline bool emit_smpl(const EngineStateSource& s,bool required){return required || !same_sample_edit(*s.sample_edit,SampleEdit{}) || *s.sample_loop || *s.sample_reverse || *s.sample_slice || *s.sample_slices!=1;}
inline bool encode_smpl(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);return write_sample_edit(w,*s.sample_edit) && write_zones(w,*s.sample_edit) && write_warp(w,*s.sample_edit) && w.u8(*s.sample_slice) && w.u8(*s.sample_slices) && w.u8(*s.sample_loop) && w.u8(*s.sample_reverse);}
inline bool decode_smpl(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);uint8_t loop,reverse;if(!read_sample_edit(r,*s.sample_edit) || !read_zones(r,*s.sample_edit) || !read_warp(r,*s.sample_edit) || !r.u8(*s.sample_slice) || !r.u8(*s.sample_slices) || !r.u8(loop) || loop>1 || !r.u8(reverse) || reverse>1)return false;*s.sample_loop=loop;*s.sample_reverse=reverse;return *s.sample_slices && *s.sample_slices<=64 && *s.sample_slice<*s.sample_slices && src.at_end();}
inline void defaults_smpl(const EngineStateView& s){*s.sample_edit=SampleEdit{};*s.sample_slice=0;*s.sample_slices=1;*s.sample_loop=false;*s.sample_reverse=false;}
inline bool emit_crea(const EngineStateSource& s,bool required){return required || !same_creative(*s.creative,CreativeState{});}
inline bool encode_crea(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);return write_creative_block(w,*s.creative);}
inline bool decode_crea(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);return read_creative_block(r,*s.creative) && src.at_end();}
inline void defaults_crea(const EngineStateView& s){*s.creative=CreativeState{};}
inline bool emit_gmap(const EngineStateSource& s,bool){if(s.kit)for(const auto& pad:s.kit->pads)if(pad.gm_note<128)return true;return false;}
inline bool encode_gmap(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);if(!s.kit)return false;for(const auto& p:s.kit->pads)if(!w.u8(p.gm_note))return false;return true;}
inline bool decode_gmap(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);if(!s.kit)return false;for(auto& p:s.kit->pads)if(!r.u8(p.gm_note) || (p.gm_note>127 && p.gm_note!=255))return false;return src.at_end();}
inline void defaults_gmap(const EngineStateView& s){if(s.kit)for(auto& p:s.kit->pads)p.gm_note=255;}
inline bool emit_kit(const EngineStateSource& s,bool required){return required && s.kit;}
inline bool encode_kit(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);return s.kit && write_kit_definition(w,*s.kit);}
inline bool decode_kit(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);return s.kit && read_kit_definition(r,*s.kit) && src.at_end();}
inline void defaults_kit(const EngineStateView& s){if(s.kit)*s.kit=KitDefinition{};}
inline bool emit_envelope(const EngineStateSource& s,bool required){return required && s.daisy_adsr;}
inline bool encode_envelope(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);if(!s.daisy_adsr)return false;for(unsigned i=0;i<4;++i)if(!w.f32(s.daisy_adsr[i]))return false;return true;}
inline bool decode_envelope(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);if(!s.daisy_adsr)return false;for(unsigned i=0;i<4;++i)if(!r.f32(s.daisy_adsr[i]) || !valid_parameter(uint16_t(104+i),s.daisy_adsr[i]))return false;return src.at_end();}
inline void defaults_envelope(const EngineStateView& s){if(s.daisy_adsr){const float initial[]={0.005f,0.12f,0.55f,0.12f};for(unsigned i=0;i<4;++i)s.daisy_adsr[i]=initial[i];}}
inline bool emit_midi(const EngineStateSource& s,bool required){return required && s.midi_program;}
inline bool encode_midi(ChunkBuffer& out,const EngineStateSource& s){ChunkWriter<ChunkBuffer> w(out);if(!s.midi_program || !w.u8(*s.midi_program))return false;for(unsigned i=0;i<128;++i)if(!w.u8(s.midi_cc[i]))return false;for(unsigned i=0;i<16;++i)if(!w.u8(s.midi_cc_used[i]))return false;return true;}
inline bool decode_midi(ChunkSource& src,const EngineStateView& s){ChunkReader<ChunkSource> r(src);if(!s.midi_program || !r.u8(*s.midi_program) || *s.midi_program>128)return false;for(unsigned i=0;i<128;++i)if(!r.u8(s.midi_cc[i]) || s.midi_cc[i]>127)return false;for(unsigned i=0;i<16;++i)if(!r.u8(s.midi_cc_used[i]))return false;return src.at_end();}
inline void defaults_midi(const EngineStateView& s){if(s.midi_program){*s.midi_program=128;for(unsigned i=0;i<128;++i)s.midi_cc[i]=0;for(unsigned i=0;i<16;++i)s.midi_cc_used[i]=0;}}
inline constexpr EngineChunkDescriptor kEngineChunks[]={
 {kChunkReso,1,emit_reso,encode_reso,decode_reso,defaults_reso},
 {kChunkTxtr,2,emit_txtr,encode_txtr,decode_txtr,defaults_txtr},
 {kChunkParm,2,emit_parm,encode_parm,decode_parm,defaults_parm},
 {kChunkSmpl,2,emit_smpl,encode_smpl,decode_smpl,defaults_smpl},
 {kChunkCrea,1,emit_crea,encode_crea,decode_crea,defaults_crea},
 {kChunkKit,2,emit_kit,encode_kit,decode_kit,defaults_kit},
 {0x52534441u,1,emit_envelope,encode_envelope,decode_envelope,defaults_envelope}, // ADSR
 {0x4944494du,1,emit_midi,encode_midi,decode_midi,defaults_midi}, // MIDI
 {0x50414d47u,1,emit_gmap,encode_gmap,decode_gmap,defaults_gmap}, // optional GMAP; absent = factory roles
};
inline constexpr EngineStateContract engine_state_contract(uint8_t required=0){return {kEngineChunks,9,required};}
}

namespace groove {
inline bool encode_wavetable(ChunkBuffer& out,const EngineStateSource& s){if(!s.wavetable)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.wavetable->values)if(!w.f32(v))return false;return true;}
inline bool decode_wavetable(ChunkSource& src,const EngineStateView& s){if(!s.wavetable)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.wavetable->values)if(!r.f32(v))return false;return src.at_end() && valid_wavetable(*s.wavetable);}
inline void defaults_wavetable(const EngineStateView& s){if(s.wavetable)*s.wavetable=WavetableState{};}
inline EngineStateContract wavetable_state_contract(){static const EngineChunkDescriptor chunk={0x42545657u,1,emit_parm,encode_wavetable,decode_wavetable,defaults_wavetable};return {&chunk,1,1};}
}

namespace groove {
inline bool encode_virtual_analog(ChunkBuffer& out,const EngineStateSource& s){if(!s.virtual_analog)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.virtual_analog->values)if(!w.f32(v))return false;return true;}
inline bool decode_virtual_analog(ChunkSource& src,const EngineStateView& s){if(!s.virtual_analog)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.virtual_analog->values)if(!r.f32(v))return false;return src.at_end() && valid_virtual_analog(*s.virtual_analog);}
inline void defaults_virtual_analog(const EngineStateView& s){if(s.virtual_analog)*s.virtual_analog=VirtualAnalogState{};}
inline EngineStateContract virtual_analog_state_contract(){static const EngineChunkDescriptor chunk={0x48544156u,2,emit_parm,encode_virtual_analog,decode_virtual_analog,defaults_virtual_analog};return {&chunk,1,1};}
}

namespace groove {
inline bool encode_layer(ChunkBuffer&,const EngineStateSource&);
inline bool decode_layer(ChunkSource&,const EngineStateView&);
inline void defaults_layer(const EngineStateView& s){if(s.layer)*s.layer=empty_layer();}
inline EngineStateContract layer_state_contract(){static const EngineChunkDescriptor chunk={0x5259414cu,2,emit_parm,encode_layer,decode_layer,defaults_layer};return {&chunk,1,1};}
inline bool encode_additive(ChunkBuffer& out,const EngineStateSource& s){if(!s.additive)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.additive->values)if(!w.f32(v))return false;return true;}
inline bool decode_additive(ChunkSource& src,const EngineStateView& s){if(!s.additive)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.additive->values)if(!r.f32(v))return false;return src.at_end() && valid_additive(*s.additive);}
inline void defaults_additive(const EngineStateView& s){if(s.additive)*s.additive=AdditiveState{};}
inline EngineStateContract additive_state_contract(){static const EngineChunkDescriptor chunk={0x54444441u,1,emit_parm,encode_additive,decode_additive,defaults_additive};return {&chunk,1,1};}
inline bool encode_phase_dist(ChunkBuffer& out,const EngineStateSource& s){if(!s.phase_dist)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.phase_dist->values)if(!w.f32(v))return false;return true;}
inline bool decode_phase_dist(ChunkSource& src,const EngineStateView& s){if(!s.phase_dist)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.phase_dist->values)if(!r.f32(v))return false;return src.at_end() && valid_phase_dist(*s.phase_dist);}
inline void defaults_phase_dist(const EngineStateView& s){if(s.phase_dist)*s.phase_dist=PhaseDistState{};}
inline EngineStateContract phase_dist_state_contract(){static const EngineChunkDescriptor chunk={0x54534450u,1,emit_parm,encode_phase_dist,decode_phase_dist,defaults_phase_dist};return {&chunk,1,1};}
inline bool encode_formant(ChunkBuffer& out,const EngineStateSource& s){if(!s.formant)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.formant->values)if(!w.f32(v))return false;return true;}
inline bool decode_formant(ChunkSource& src,const EngineStateView& s){if(!s.formant)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.formant->values)if(!r.f32(v))return false;return src.at_end() && valid_formant(*s.formant);}
inline void defaults_formant(const EngineStateView& s){if(s.formant)*s.formant=FormantState{};}
inline EngineStateContract formant_state_contract(){static const EngineChunkDescriptor chunk={0x544d5246u,1,emit_parm,encode_formant,decode_formant,defaults_formant};return {&chunk,1,1};}
inline bool encode_granular(ChunkBuffer& out,const EngineStateSource& s){if(!s.granular)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.granular->values)if(!w.f32(v))return false;return true;}
inline bool decode_granular(ChunkSource& src,const EngineStateView& s){if(!s.granular)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.granular->values)if(!r.f32(v))return false;return src.at_end() && valid_granular(*s.granular);}
inline void defaults_granular(const EngineStateView& s){if(s.granular)*s.granular=GranularState{};}
inline EngineStateContract granular_state_contract(){static const EngineChunkDescriptor chunks[]={
 {kChunkParm,2,emit_parm,encode_parm,decode_parm,defaults_parm},
 {0x4e415247u,1,emit_parm,encode_granular,decode_granular,defaults_granular}};return {chunks,2,3};}
inline bool encode_soundfont(ChunkBuffer& out,const EngineStateSource& s){if(!s.soundfont || !s.sample_slot)return false;ChunkWriter<ChunkBuffer> w(out);if(!w.u8(*s.sample_slot))return false;for(float v:s.soundfont->values)if(!w.f32(v))return false;return true;}
inline bool decode_soundfont(ChunkSource& src,const EngineStateView& s){if(!s.soundfont || !s.sample_slot)return false;ChunkReader<ChunkSource> r(src);if(!r.u8(*s.sample_slot) || *s.sample_slot>=kSampleAssets)return false;for(float& v:s.soundfont->values)if(!r.f32(v))return false;return src.at_end() && valid_soundfont(*s.soundfont);}
inline void defaults_soundfont(const EngineStateView& s){if(s.soundfont)*s.soundfont=SoundFontState{};if(s.sample_slot)*s.sample_slot=0;}
inline EngineStateContract soundfont_state_contract(){static const EngineChunkDescriptor chunk={0x20324653u,1,emit_parm,encode_soundfont,decode_soundfont,defaults_soundfont};return {&chunk,1,1};}
inline bool encode_fm6(ChunkBuffer& out,const EngineStateSource& s){if(!s.fm6)return false;ChunkWriter<ChunkBuffer> w(out);for(float v:s.fm6->values)if(!w.f32(v))return false;return w.block(s.fm6->patch,128);}
inline bool decode_fm6(ChunkSource& src,const EngineStateView& s){if(!s.fm6)return false;ChunkReader<ChunkSource> r(src);for(float& v:s.fm6->values)if(!r.f32(v))return false;return r.block(s.fm6->patch,128) && src.at_end() && valid_fm6(*s.fm6);}
inline void defaults_fm6(const EngineStateView& s){if(s.fm6)*s.fm6=Fm6State{};}
inline EngineStateContract fm6_state_contract(){static const EngineChunkDescriptor chunk={0x20534d46u,1,emit_parm,encode_fm6,decode_fm6,defaults_fm6};return {&chunk,1,1};}
}
