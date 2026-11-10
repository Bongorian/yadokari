#pragma once
#include "project.h"
#include "project_format_version.h"
#include "engine_guids.h"
#include "preserved_payload.h"
#include <stddef.h>
#include <stdint.h>
namespace groove {
inline constexpr uint32_t kChunkTwin=0x4e495754u; // TWIN v1, optional Song association
inline constexpr uint32_t kChunkTrks=0x534b5254u,kChunkPhrs=0x53524850u,kChunkPseq=0x51455350u;

// Sink: bool write(const uint8_t*,size_t); Source: bool read(uint8_t*,size_t), bool at_end().
// Portable MGXC chunk format (docs/project-format.md), little endian and
// IEEE float32, CRC32 of everything after the magic. The positional MGXZ
// layout is retired. Unsupported formats are rejected without migration.
class ProjectCrc {
public:
 // Two four-bit steps use a small table instead of eight bitwise iterations.
 // This remains the same CRC-32/ISO-HDLC used by existing project and WAV data.
 void add(uint8_t x) {
  static constexpr uint32_t table[16]={0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu};
  crc_^=x;crc_=(crc_>>4)^table[crc_&15];crc_=(crc_>>4)^table[crc_&15];
 }
 uint32_t value() const { return crc_^0xffffffffu; }
private: uint32_t crc_=0xffffffffu;
};
template<class Sink> class ProjectWriter {
public:
 explicit ProjectWriter(Sink& s):s_(s) {}
 bool raw(uint8_t x) { return s_.write(&x,1); }
 bool u8(uint8_t x) { crc_.add(x); return raw(x); }
 bool u16(uint16_t x) { uint8_t b[2]={uint8_t(x),uint8_t(x>>8)};for(uint8_t v:b)crc_.add(v);return s_.write(b,2); }
 bool u32(uint32_t x) { uint8_t b[4]={uint8_t(x),uint8_t(x>>8),uint8_t(x>>16),uint8_t(x>>24)};for(uint8_t v:b)crc_.add(v);return s_.write(b,4); }
 bool f32(float x) { static_assert(sizeof(float)==4,"float32 required"); uint32_t bits; __builtin_memcpy(&bits,&x,4); return u32(bits); }
 bool finish() { uint32_t x=crc_.value(); for(unsigned i=0;i<4;++i) if(!raw(uint8_t(x>>(8*i)))) return false; return true; }
private: Sink& s_; ProjectCrc crc_;
};
template<class Source> class ProjectReader {
public:
 explicit ProjectReader(Source& s):s_(s) {}
 bool raw(uint8_t& x) { return s_.read(&x,1); }
 bool u8(uint8_t& x) { if(!raw(x)) return false; crc_.add(x); return true; }
 bool u16(uint16_t& x) { uint8_t b[2];if(!s_.read(b,2))return false;for(uint8_t v:b)crc_.add(v);x=uint16_t(b[0])|(uint16_t(b[1])<<8);return true; }
 bool u32(uint32_t& x) { uint8_t b[4];if(!s_.read(b,4))return false;for(uint8_t v:b)crc_.add(v);x=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true; }
 bool f32(float& x) { uint32_t bits; if(!u32(bits)) return false; __builtin_memcpy(&x,&bits,4); return true; }
 bool finish() { uint32_t expected=0; for(unsigned i=0;i<4;++i) { uint8_t x; if(!raw(x)) return false; expected|=uint32_t(x)<<(8*i); } return expected==crc_.value() && s_.at_end(); }
private: Source& s_; ProjectCrc crc_;
};
template<class W> bool write_extended_step(W& w,const StepState& s, const UnavailableTargetOwners* owners=nullptr){if(!w.u8(uint8_t(s.enabled | (s.lock_only?2:0) | (s.chromatic?4:0) | (s.native_drum?8:0) | (s.slide()?16:0) | (s.accent()?32:0)))||!w.u8(s.note)||!w.u8(s.velocity)||!w.u8(s.probability)||!w.u8(s.ratchet)||!w.u16(uint16_t(s.microtiming))||!w.u16(s.gate))return false;for(auto note:s.chord)if(!w.u8(note))return false;for(const auto& lock:s.locks)if(!write_parameter_target(w,lock.id,owners)||!w.f32(lock.value)||!w.u8(lock.enabled))return false;return w.u8(uint8_t(s.condition))&&w.u8(s.cycle_every)&&w.u8(s.cycle_phase)&&w.u16(s.sound);}
template<class R> bool read_extended_step(R& r,StepState& s, UnavailableTargetOwners* owners=nullptr){uint8_t enabled,condition;uint16_t micro;if(!r.u8(enabled)||enabled>63||!r.u8(s.note)||!r.u8(s.velocity)||!r.u8(s.probability)||!r.u8(s.ratchet)||!r.u16(micro)||!r.u16(s.gate))return false;s.enabled=(enabled&1)!=0;s.lock_only=(enabled&2)!=0;s.chromatic=(enabled&4)!=0;s.native_drum=(enabled&8)!=0;s.slide((enabled&16)!=0);s.accent((enabled&32)!=0);s.microtiming=int16_t(micro);for(auto& note:s.chord)if(!r.u8(note))return false;for(auto& lock:s.locks){uint8_t active;if(!read_parameter_target(r,lock.id,owners)||!r.f32(lock.value)||!r.u8(active)||active>1)return false;lock.enabled=active;}if(!r.u8(condition)||!r.u8(s.cycle_every)||!r.u8(s.cycle_phase)||!r.u16(s.sound))return false;s.condition=TrigCondition(condition);return true;}
// Extended cells use a fixed bitmap and sparse full records. Default cells do
// not inflate every preset/project by several megabytes; decoding remains bounded.
template<class W> bool write_extended_steps(W& w,const StepState* steps, const UnavailableTargetOwners* owners=nullptr){const StepState empty{};uint8_t bits[(kSteps-64)/8]{};for(unsigned i=64;i<kSteps;++i)if(!same_step(steps[i],empty))bits[(i-64)/8]|=uint8_t(1u<<((i-64)%8));for(uint8_t b:bits)if(!w.u8(b))return false;for(unsigned i=64;i<kSteps;++i)if((bits[(i-64)/8]&(1u<<((i-64)%8))) && !write_extended_step(w,steps[i],owners))return false;return true;}
template<class R> bool read_extended_steps(R& r,StepState* steps, UnavailableTargetOwners* owners=nullptr){const StepState empty{};uint8_t bits[(kSteps-64)/8];for(auto& b:bits)if(!r.u8(b))return false;for(unsigned i=64;i<kSteps;++i){steps[i]=empty;if((bits[(i-64)/8]&(1u<<((i-64)%8))) && !read_extended_step(r,steps[i],owners))return false;}return true;}
template<class W> bool write_extended_automation(W& w,const AutomationLane& lane){uint8_t bits[(kAutomationSteps-64)/8]{};for(unsigned i=64;i<kAutomationSteps;++i)if(__builtin_memcmp(&lane.values[i],&lane.values[i%64],sizeof(float)))bits[(i-64)/8]|=uint8_t(1u<<((i-64)%8));for(uint8_t b:bits)if(!w.u8(b))return false;for(unsigned i=64;i<kAutomationSteps;++i)if((bits[(i-64)/8]&(1u<<((i-64)%8))) && !w.f32(lane.values[i]))return false;return true;}
template<class R> bool read_extended_automation(R& r,AutomationLane& lane){uint8_t bits[(kAutomationSteps-64)/8];for(auto& b:bits)if(!r.u8(b))return false;for(unsigned i=64;i<kAutomationSteps;++i){lane.values[i]=lane.values[i%64];if((bits[(i-64)/8]&(1u<<((i-64)%8))) && !r.f32(lane.values[i]))return false;}return true;}
// ---------------------------------------------------------------------------
// MGXC chunk codec. See docs/project-format.md for the format definition,
// the GUID registry and the unknown-engine / unknown-chunk fallback rules.
inline constexpr uint32_t kChunkProj=0x4a4f5250u; // "PROJ" (LE of bytes P,R,O,J)
inline constexpr uint32_t kChunkAsst=0x54535341u; // "ASST"
inline constexpr uint32_t kChunkScen=0x4e454353u; // "SCEN"
inline constexpr uint32_t kChunkMacr=0x5243414du; // "MACR"
inline constexpr uint32_t kChunkMfx=0x2058464du;  // "MFX "
inline constexpr uint32_t kChunkType=0x45505954u; // TYPE: Song / Kit / Preset
inline constexpr uint32_t kChunkPref=0x46455250u; // PREF: exact previous pattern
inline constexpr uint32_t kChunkPtrn=0x4e525450u; // "PTRN"
inline constexpr uint32_t kChunkPmut=0x54554d50u; // "PMUT"
inline constexpr uint32_t kChunkSnds=0x53444e53u; // "SNDS"
 inline constexpr uint32_t kChunkAudio=0x4f445541u; // AUDO revision 1
inline constexpr uint32_t kChunkSong=0x474e4f53u; // "SONG"
inline constexpr uint32_t kChunkSnd=0x20444e53u;  // "SND "
inline constexpr uint32_t kChunkTrck=0x4b435254u; // "TRCK"
inline constexpr uint32_t kChunkKseq=0x5145534bu; // KSEQ: phrase-owned kit steps
inline constexpr uint32_t kChunkClfa=0x41464c43u; // "CLFA", revision 1: action/cycles/target/probability
inline constexpr uint32_t kChunkIden=0x4e454449u; // IDEN: preset identity v1
inline constexpr uint32_t kChunkPunk=0x4b4e5550u; // PUNK v1: scoped preserved definition records
inline constexpr uint32_t kChunkPrid=0x44495250u; // PRID: pinned track reference v1
inline constexpr uint32_t kChunkTnam=0x4d414e54u; // TNAM: independent track name v1
inline constexpr uint32_t kChunkTcom=0x4d4f4354u; // "TCOM"
inline constexpr uint32_t kChunkName=0x4d414e50u; // PNAM: optional instance provenance
inline constexpr uint32_t kChunkEngn=0x4e474e45u; // "ENGN"

// Shared by decoding and reference guards; unsupported revisions remain unknown.
inline constexpr uint16_t project_section_revision(uint32_t kind){return kind==kChunkMacr?5:kind==kChunkProj?3:kind==kChunkSong?2:1;}

// CRC-covered outer stream writer/reader (framing only; chunk payloads
// are built with ChunkWriter and copied through block()).
template<class Sink> class FormatWriter {
public:
 explicit FormatWriter(Sink& s):s_(s){}
 bool raw(uint8_t x){return s_.write(&x,1);}
 bool u8(uint8_t x){crc_.add(x);return raw(x);}
 bool u16(uint16_t x){uint8_t b[2]={uint8_t(x),uint8_t(x>>8)};for(uint8_t v:b)crc_.add(v);return s_.write(b,2);}
 bool u32(uint32_t x){uint8_t b[4]={uint8_t(x),uint8_t(x>>8),uint8_t(x>>16),uint8_t(x>>24)};for(uint8_t v:b)crc_.add(v);return s_.write(b,4);}
 bool f32(float x){uint32_t bits;__builtin_memcpy(&bits,&x,4);return u32(bits);}
 bool block(const uint8_t* p,uint32_t n){for(uint32_t i=0;i<n;++i)crc_.add(p[i]);return s_.write(p,n);}
 bool finish(){uint32_t x=crc_.value();for(unsigned i=0;i<4;++i)if(!raw(uint8_t(x>>(8*i))))return false;return true;}
private: Sink& s_; ProjectCrc crc_;
};
template<class Source> class FormatReader {
public:
 explicit FormatReader(Source& s):s_(s){}
 bool raw(uint8_t& x){return s_.read(&x,1);}
 bool u8(uint8_t& x){if(!raw(x))return false;crc_.add(x);return true;}
 bool u16(uint16_t& x){uint8_t b[2];if(!s_.read(b,2))return false;for(uint8_t v:b)crc_.add(v);x=uint16_t(b[0])|(uint16_t(b[1])<<8);return true;}
 bool u32(uint32_t& x){uint8_t b[4];if(!s_.read(b,4))return false;for(uint8_t v:b)crc_.add(v);x=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;}
 bool f32(float& x){uint32_t bits;if(!u32(bits))return false;__builtin_memcpy(&x,&bits,4);return true;}
 bool block(uint8_t* p,uint32_t n){if(!s_.read(p,n))return false;for(uint32_t i=0;i<n;++i)crc_.add(p[i]);return true;}
 // Memory sources lend a bounded view. Streaming sources still own a
 // temporary payload; neither path skips CRC or semantic validation.
 bool payload(uint32_t n,ChunkSource& out,ChunkBuffer& storage){return payload_from(s_,n,out,storage,0);}
 bool finish(){uint32_t expected=0;for(unsigned i=0;i<4;++i){uint8_t x;if(!raw(x))return false;expected|=uint32_t(x)<<(8*i);}return expected==crc_.value() && s_.at_end();}
private:
 template<class T> auto payload_from(T& source,uint32_t n,ChunkSource& out,ChunkBuffer&,int)->decltype(source.slice(n,out)){
  if(!source.slice(n,out))return false;
  for(uint32_t i=0;i<n;++i)crc_.add(out.data[i]);
  return true;
 }
 template<class T> bool payload_from(T&,uint32_t n,ChunkSource& out,ChunkBuffer& storage,long){
  if(n){storage.data=alloc_uninitialized_bytes(n);if(!storage.data)return false;storage.size=storage.capacity=n;if(!block(storage.data,n))return false;}
  out={storage.data,n,0};return true;
 }
 Source& s_; ProjectCrc crc_;
};
// Frame one chunk into a buffer-backed writer (used at every nesting
// level; `version` is the chunk's own format version).
template<class W> bool write_framed(W& w,uint32_t kind,uint16_t version,const ChunkBuffer& payload,uint16_t flags=0){
 return w.u32(kind)&&w.u16(version)&&w.u16(flags)&&w.u32(payload.size)&&w.block(payload.data,payload.size);
}
// Read one chunk header from an in-memory source and slice its payload.
inline bool read_framed(ChunkReader<ChunkSource>& r,uint32_t& kind,uint16_t& version,ChunkSource& payload,uint16_t* flags=nullptr){
 uint16_t read_flags;uint32_t size;
 if(!r.u32(kind)||!r.u16(version)||!r.u16(read_flags)||!r.u32(size))return false;
 if(flags)*flags=read_flags;
 return r.sliced(size,payload);
}

// --- Section writers -------------------------------------------------------
inline bool write_preset_identity(ChunkWriter<ChunkBuffer>& w,const PresetIdentity& id){return id.valid() && w.block(id.guid.bytes,16) && w.u32(id.revision) && w.u8(unsigned(id.origin)) && w.block(id.parent.bytes,16) && w.u32(id.parent_revision);}
inline bool read_preset_identity(ChunkReader<ChunkSource>& r,PresetIdentity& id){uint8_t origin;if(!r.block(id.guid.bytes,16) || !r.u32(id.revision) || !r.u8(origin) || !r.block(id.parent.bytes,16) || !r.u32(id.parent_revision))return false;id.origin=PresetOrigin(origin);return id.valid();}
inline bool write_identity_frame(ChunkBuffer& out,const PresetIdentity& id){uint32_t frame;ChunkWriter<ChunkBuffer> w(out);return id.assigned() && out.begin_frame(kChunkIden,1,frame) && write_preset_identity(w,id) && out.end_frame(frame);}

inline void preserve_auxiliary(PreservedStore* store,PreservedTrackEntry* entry,const ChunkBuffer& bytes){if(!store || !entry || !bytes.size)return;if(bytes.size>PreservedStore::kMaxEntryBytes || store->used_bytes()+bytes.size>PreservedStore::kMaxTotalBytes || !entry->auxiliary.set(bytes.data,bytes.size))store->dropped_unknown=true;}
inline void preserve_scope(PreservedStore* store,PreservedBytes* target,const ChunkBuffer& bytes){if(!store || !target || !bytes.size)return;if(bytes.size>PreservedStore::kMaxEntryBytes || uint64_t(store->used_bytes())+bytes.size>PreservedStore::kMaxTotalBytes || !target->set(bytes.data,bytes.size))store->dropped_unknown=true;}
inline bool append_unknown_frame(ChunkBuffer& out,uint32_t kind,uint16_t version,uint16_t flags,const ChunkSource& sub){ChunkWriter<ChunkBuffer> w(out);return w.u32(kind) && w.u16(version) && w.u16(flags) && w.u32(sub.size) && w.block(sub.data,sub.size);}
inline bool write_proj_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 if(!w.u16(p.tempo)||!w.u8(p.active_pattern)||!w.u8(p.active_scene)||!w.u16(p.muted_tracks)||!w.u16(p.solo_tracks))return false;
 for(uint8_t route:p.midi_input_route)if(!w.u8(route))return false;
 if(!w.u8(p.midi_clock_slave)||!w.u32(p.random_seed))return false;
 for(float c:p.tuning)if(!w.f32(c))return false;
 if(!w.u16(p.gesture.count)||!w.u16(p.gesture.ticks))return false;
 for(unsigned i=0;i<p.gesture.count;++i){const auto& e=p.gesture.points[i];if(!w.u16(e.tick)||!write_parameter_target(w,e.parameter,&p.unavailable_targets)||!w.f32(e.value)||!w.u8(e.track)||!w.u8(e.pattern)||!w.u8(uint8_t(e.target)))return false;}
 const auto& ui=p.ui;
 for(auto o:ui.keyboard_octave)if(!w.u8(uint8_t(o)))return false;
 return w.u8(0)&&w.u8(ui.motion)&&w.u8(ui.order)&&w.u8(ui.strip)&&w.u8(ui.preview565)&&w.u8(ui.note_quantized)&&w.u8(ui.note_replace)&&w.u8(ui.metronome)&&w.u8(ui.count_in)&&w.u8(ui.click_main)&&w.u8(ui.arp_direction)&&w.u8(ui.arp_octaves)&&w.u8(ui.repeat_gate)&&w.u8(ui.repeat_rate)&&w.u8(ui.chord_shape)&&w.u8(ui.chord_inversion)&&w.u16(ui.wave_profile_mask)&&w.u16(ui.wave_clock_mask);
}
inline bool write_asst_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 for(const auto& a:p.assets){
  for(char c:a.path)if(!w.u8(uint8_t(c)))return false;
  const auto& m=a.metadata;
  if(!w.u32(m.rate)||!w.u32(m.frames)||!w.u32(m.content_crc)||!w.u16(m.bits)||!w.u16(m.bpm)||!w.u8(m.channels)||!w.u8(m.root)||!w.u8(m.favorite))return false;
 }
 return true;
}
inline bool write_morph_snapshot(ChunkWriter<ChunkBuffer>& w,const ParameterSnapshot& s){
 for(unsigned i=0;i<kCommonMorphParameters;++i)if(morph_common_parameter(morph_parameter_at(i)) && !w.f32(s.values[i]))return false;
 const char* guid=s.engine==Instrument::Unknown?s.unknown_guid:engine_guid(s.engine);unsigned length=0;while(guid[length])++length;const auto* d=engine_descriptor(unsigned(s.engine));unsigned count=s.engine==Instrument::Unknown?s.unknown_count:d?d->parameters.count:0;
 if(count>kEngineMorphParameters || !w.u8(uint8_t(length)) || !w.block(reinterpret_cast<const uint8_t*>(guid),length) || !w.u16(uint16_t(count)))return false;
 for(unsigned i=0;i<count;++i)if(!w.f32(s.engine_values[i]))return false;
 return true;
}
inline bool write_scen_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 for(const auto& scene:p.scenes){for(uint8_t x:scene.pattern_for_track)if(!w.u8(x))return false;if(!w.u16(scene.mute_mask))return false;}
 if(!w.u8(p.chain.length)||!w.u8(p.chain.bars)||!w.u8(p.chain.enabled))return false;
 for(uint8_t scene:p.chain.scenes)if(!w.u8(scene))return false;
 if(!w.u8(uint8_t(p.chain.mode))||!w.u8(p.chain.probability))return false;
 for(const auto& rule:p.chain.follow)if(!w.u8(rule.bars)||!w.u8(rule.probability)||!w.u8(rule.weight)||!w.u8(uint8_t(rule.condition)))return false;
 for(const auto& clip:p.clips)if(!w.u8(clip.pattern)||!w.u8(clip.playing))return false;
 return true;
}
inline bool write_song_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 const auto& song=p.song;if(!w.u8(song.length) || !w.u8(song.enabled) || !w.u8(song.loop))return false;
 for(unsigned i=0;i<song.length;++i){const auto& s=song.sections[i];if(!w.u8(s.scene) || !w.u16(s.bars) || !w.u8(uint8_t(s.transpose)) || !w.u16(s.mute_mask) || !w.u32(s.ticks) || !w.u16(s.playing_mask) || !w.u16(s.restart_mask))return false;for(auto pattern:s.patterns)if(!w.u8(pattern))return false;}
 return true;
}
 inline bool write_macr_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 for(const auto& macro:p.macros){
  if(!w.u8(macro.active)||!w.f32(macro.value))return false;
  for(const auto& route:macro.routes)if(!w.u8(route.enabled)||!write_parameter_target(w,route.parameter,&p.unavailable_targets)||!w.u16(route.tracks)||!w.f32(route.low)||!w.f32(route.high)||!w.u8(uint8_t(unsigned(route.mode)|(unsigned(route.curve)<<2)|(unsigned(route.source)<<4))))return false;
 }
 if(!w.u8(p.morph_enabled)||!w.f32(p.morph_position))return false;
 for(unsigned t=0;t<kTracks;++t){
  if(!write_morph_snapshot(w,p.morph_a[t]))return false;
  if(!write_morph_snapshot(w,p.morph_b[t]))return false;
 }
 // Stage 4 morph sets tail (absent in pre-Stage-4 files).
 if(!w.u8(p.morph_set))return false;
 for(unsigned s=0;s<3;++s)for(unsigned t=0;t<kTracks;++t){
  if(!write_morph_snapshot(w,p.morph_extra_a[s][t]))return false;
  if(!write_morph_snapshot(w,p.morph_extra_b[s][t]))return false;
 }
 return true;
}
inline bool write_mfx_section(ChunkWriter<ChunkBuffer>& w,const ProjectState& p){
 const auto& e=p.effects;
 if(!w.f32(e.delay_mix)||!w.f32(e.delay_feedback)||!w.u16(e.delay_ms)||!w.f32(e.reverb_mix)||!w.f32(e.reverb_decay)||!w.f32(e.reverb_damp))return false;
 for(float v:e.compressor)if(!w.f32(v))return false;
 if(!w.f32(e.live_fx_type)||!w.f32(e.live_fx_amount))return false;
 const auto& r=p.routing;
 if(!w.f32(r.main)||!w.f32(r.cue)||!w.f32(r.preview)||!w.f32(r.monitor)||!w.u8(r.monitor_bus)||!w.u8(r.capture_bus))return false;
 for(const auto& g:r.groups){
  if(!w.u16(g.members)||!w.f32(g.gain)||!w.u8(g.output))return false;
  for(float v:g.insert_fx)if(!w.f32(v))return false;
  for(float v:g.send)if(!w.f32(v))return false;
  if(!w.f32(g.parallel))return false;
 }
 return true;
}
// --- Persistent track (TCOM) and phrase sequence (PSEQ) ---------------------------------------------------
inline bool write_full_step(ChunkWriter<ChunkBuffer>& w,const StepState& s, const UnavailableTargetOwners* owners=nullptr){
 if(!w.u8(uint8_t(s.enabled | (s.lock_only?2:0) | (s.chromatic?4:0) | (s.native_drum?8:0) | (s.slide()?16:0) | (s.accent()?32:0)))||!w.u8(s.note)||!w.u8(s.velocity)||!w.u8(s.probability)||!w.u8(s.ratchet)||!w.u16(uint16_t(s.microtiming))||!w.u16(s.gate))return false;
 for(const auto& lock:s.locks)if(!write_parameter_target(w,lock.id,owners)||!w.f32(lock.value)||!w.u8(lock.enabled))return false;
 if(!w.u8(uint8_t(s.condition))||!w.u8(s.cycle_every)||!w.u8(s.cycle_phase)||!w.u16(s.sound))return false;
 for(uint8_t note:s.chord)if(!w.u8(note))return false;
 return true;
}
inline bool write_tcom_section(ChunkWriter<ChunkBuffer>& w,ConstTrackView t){
 if(!w.u8(t.midi_channel)||!w.u8(t.root_note)||!w.f32(t.level)||!w.f32(t.pan))return false;
 if(!w.u8(t.groove.preset)||!w.u8(t.groove.amount))return false;
 for(unsigned i=0;i<16;++i)if(!w.u8(uint8_t(t.groove.timing[i]))||!w.u8(uint8_t(t.groove.velocity[i])))return false;
 for(uint8_t cc:t.midi_cc)if(!w.u8(cc))return false;
 for(uint8_t used:t.midi_cc_used)if(!w.u8(used))return false;
 for(const auto& slot:t.nrpn)if(!w.u16(slot.number)||!w.u16(slot.value)||!w.u8(slot.enabled)||!w.u8(slot.registered))return false;
 if(!w.u16(t.pitch_bend)||!w.u8(t.pressure)||!w.u8(t.expression_used)||!w.u8(t.midi_program))return false;
 if(!w.u8(t.arp_mode)||!w.u8(t.arp_rate)||!w.u8(t.arp_octaves))return false;
 if(!w.u8(uint8_t(t.transpose))||!w.u16(t.scale_mask)||!w.u8(t.scale_root)||!w.f32(t.velocity_scale))return false;
 const float extras[]={t.cutoff,t.drive,t.delay_send,t.crusher,t.lfo_depth,t.lfo_rate,t.aftertouch_depth};
 for(float x:extras)if(!w.f32(x))return false;
 if(!w.f32(t.reverb_send))return false;
 if(!w.u8(t.output_bus)||!w.u8(t.send_prefader))return false;
 for(float v:t.insert_fx)if(!w.f32(v))return false;
 for(float v:t.compressor)if(!w.f32(v))return false;
 if(!w.u8(t.insert_disabled))return false;
 return w.f32(t.texture.values[12]) && w.f32(t.lfo_sync) && w.f32(t.lfo_division);
}
inline bool write_pseq_section(ChunkWriter<ChunkBuffer>& w,ConstTrackView t, const UnavailableTargetOwners* owners=nullptr){
 if(!w.u16(t.length))return false;
 for(unsigned cell=0;cell<64;++cell)if(!write_full_step(w,t.steps[cell],owners))return false;
 if(!write_extended_steps(w,t.steps,owners))return false;
 if(!w.u8(t.mutation.frozen))return false;
 if(t.mutation.frozen){
  for(unsigned cell=0;cell<64;++cell)if(!write_full_step(w,t.mutation_source[cell],owners))return false;
  if(!write_extended_steps(w,t.mutation_source,owners))return false;
 }
 if(!w.u32(t.mutation.seed)||!w.u8(t.mutation.amount))return false;
 const auto& a=t.accumulator;
 if(!w.u8(uint8_t(a.pitch))||!w.u8(uint8_t(a.minimum))||!w.u8(uint8_t(a.maximum))||!w.u8(uint8_t(a.velocity))||!w.u8(uint8_t(a.gate))||!w.u8(uint8_t(a.mode))||!w.u8(a.reset)||!w.u8(a.scale_degrees))return false;
 if(!w.u8(t.lanes_enabled))return false;
 for(const auto& lane:t.lanes){
  if(!w.u8(lane.length)||!w.u8(uint8_t(lane.direction)))return false;
  for(unsigned i=0;i<16;++i)if(!w.u8(lane.values[i])||!w.u8(lane.duration[i]))return false;
 }
 for(const auto& lane:t.automation){
  if(!w.u8(lane.enabled)||!write_parameter_target(w,lane.parameter,owners)||!w.u8(uint8_t(lane.length==256?0:lane.length))||!w.u8(uint8_t(lane.mode)))return false;
  for(unsigned i=0;i<64;++i)if(!w.f32(lane.values[i]))return false;
  if(!write_extended_automation(w,lane))return false;
 }
 if(!w.u8(uint8_t(t.direction))||!w.u8(t.speed_num)||!w.u8(t.speed_den))return false;
 if(!w.u8(t.frozen_asset)||!w.u16(t.sound_override))return false;
 return true;
}
// --- Engine payload (ENGN) -------------------------------------------------
// Engine state adapters carry no ownership and perform no copies.
inline EngineStateSource engine_state_source(ConstTrackView t){return {&t.texture,&t.resonator,&t.plaits_model,t.engine_parameters,&t.sample_slot,&t.sample_edit,&t.creative,&t.kit.definition(),&t.sample_loop,&t.sample_reverse,&t.sample_slice,&t.sample_slices,t.preset_name,t.daisy_adsr,t.instrument==Instrument::ExternalMidi?&t.midi_program:nullptr,t.midi_cc,t.midi_cc_used,&t.wavetable,&t.virtual_analog,&t.fm6,&t.additive,&t.phase_dist,&t.soundfont,&t.layer,&t.formant,&t.granular};}
inline EngineStateSource engine_state_source(const SoundState& s){return {&s.texture,&s.resonator,&s.model,s.parameters,&s.sample_slot,&s.sample_edit,&s.creative,&s.kit,&s.sample_loop,&s.sample_reverse,&s.sample_slice,&s.sample_slices,s.preset_name,s.daisy_adsr,&s.midi_program,s.midi_cc,s.midi_cc_used,&s.wavetable,&s.virtual_analog,&s.fm6,&s.additive,&s.phase_dist,&s.soundfont,&s.layer,&s.formant,&s.granular};}
inline EngineStateView engine_state_view(TrackView t){return {&t.texture,&t.resonator,&t.plaits_model,t.engine_parameters,&t.sample_slot,&t.sample_edit,&t.creative,&t.kit.definition(),&t.sample_loop,&t.sample_reverse,&t.sample_slice,&t.sample_slices,t.preset_name,t.daisy_adsr,t.instrument==Instrument::ExternalMidi?&t.midi_program:nullptr,t.midi_cc,t.midi_cc_used,&t.wavetable,&t.virtual_analog,&t.fm6,&t.additive,&t.phase_dist,&t.soundfont,&t.layer,&t.formant,&t.granular};}
inline EngineStateView engine_state_view(SoundState& s){return {&s.texture,&s.resonator,&s.model,s.parameters,&s.sample_slot,&s.sample_edit,&s.creative,&s.kit,&s.sample_loop,&s.sample_reverse,&s.sample_slice,&s.sample_slices,s.preset_name,s.daisy_adsr,&s.midi_program,s.midi_cc,s.midi_cc_used,&s.wavetable,&s.virtual_analog,&s.fm6,&s.additive,&s.phase_dist,&s.soundfont,&s.layer,&s.formant,&s.granular};}
inline bool write_engine_state(ChunkBuffer& out,Instrument instrument,const EngineStateSource& source,const PreservedTrackEntry* entry){
 ChunkWriter<ChunkBuffer> w(out);
 if(entry && !entry->engn.empty() && instrument==Instrument::Unknown)return w.block(entry->engn.data,entry->engn.size);
 const char* guid=engine_guid(instrument);unsigned length=0;while(guid[length])++length;
 if(!w.u8(uint8_t(length))||!w.block(reinterpret_cast<const uint8_t*>(guid),length)||!w.u16(1))return false;
 if(source.preset_name && source.preset_name[0]){unsigned n=0;while(n<96 && source.preset_name[n])++n;if(n==96)return false;uint32_t frame;if(!out.begin_frame(kChunkName,1,frame) || !w.block(reinterpret_cast<const uint8_t*>(source.preset_name),n) || !out.end_frame(frame))return false;}
 const auto* descriptor=engine_descriptor(unsigned(instrument));
 const auto& state=descriptor?descriptor->state:engine_state_contract();
 for(unsigned i=0;i<state.count;++i){const auto& chunk=state.chunks[i];
  if(!chunk.emit(source,(state.required&(1u<<i))!=0))continue;
  ChunkBuffer sub(out.copy,out.context);
  if(!chunk.encode(sub,source)||!write_framed(w,chunk.fourcc,chunk.version,sub))return false;
 }
 if(entry && entry->extra_for==int32_t(instrument) && !entry->extra_sub.empty())return w.block(entry->extra_sub.data,entry->extra_sub.size);
 return true;
}
inline bool write_track_engn(ChunkBuffer& out,ConstTrackView t,const PreservedTrackEntry* entry){return write_engine_state(out,t.instrument,engine_state_source(t),entry);}
inline bool write_sound_engn(ChunkBuffer& out,const SoundState& s,const PreservedTrackEntry* entry){return write_engine_state(out,s.instrument,engine_state_source(s),entry);}
template<class W> bool write_audio_section(W& w,const ProjectState& p){
 for(const auto& a:p.audio){if(!w.u8(unsigned(a.kind)) || !w.u8(unsigned(a.length)) || !w.u8(a.bars) || !w.u8(a.source) || !w.u8(a.enabled) || !w.f32(a.gain) || !w.f32(a.pan) || !w.f32(a.feedback))return false;
  for(const auto& c:a.clips)if(!w.u8(c.asset) || !w.u32(c.tick) || !w.u32(c.first) || !w.u32(c.frames) || !w.f32(c.gain))return false;}
 return true;
}
inline bool read_audio_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 for(auto& a:p.audio){uint8_t kind,length,enabled;if(!r.u8(kind) || !r.u8(length) || !r.u8(a.bars) || !r.u8(a.source) || !r.u8(enabled) || enabled>1 || !r.f32(a.gain) || !r.f32(a.pan) || !r.f32(a.feedback))return false;a.enabled=enabled!=0;a.kind=TrackKind(kind);a.length=LoopLength(length);
  for(auto& c:a.clips)if(!r.u8(c.asset) || !r.u32(c.tick) || !r.u32(c.first) || !r.u32(c.frames) || !r.f32(c.gain))return false;}
 return src.at_end() && valid_audio_lanes(p.audio);
}
// --- Top-level encode ------------------------------------------------------
// Build directly in the final owner buffer. Frame offsets survive growth;
// backpatching lengths avoids copying TCOM -> TRCK -> PTRN -> stream.
inline bool valid_preserved_root(ChunkSource raw){while(!raw.at_end()){ChunkReader<ChunkSource> r(raw);uint32_t kind;uint16_t version,flags;ChunkSource child;if(!read_framed(r,kind,version,child,&flags))return false;}return true;}
// PUNK v1 transports unknown children with their original parent scope.
// Each of root / standalone TRCK / Song SND is a u32 length + raw TLV stream.
inline bool write_preserved_root(ChunkBuffer& out,const PreservedTrackEntry* entry,bool sound_scope=false,bool standalone=false){
 if(!entry)return true;
 const PreservedBytes empty;const auto& root=standalone?empty:entry->preset_root;const auto& track=standalone?empty:entry->preset_track;const auto& sound=sound_scope?empty:entry->preset_sound;
 if(root.empty() && track.empty() && sound.empty())return true;
 uint32_t frame;ChunkWriter<ChunkBuffer> w(out);
 return out.begin_frame(kChunkPunk,1,frame) && w.u32(root.size) && w.block(root.data,root.size) && w.u32(track.size) && w.block(track.data,track.size) && w.u32(sound.size) && w.block(sound.data,sound.size) && out.end_frame(frame);
}
inline bool read_preserved_root(ChunkSource raw,PreservedStore* store,PreservedTrackEntry* entry){
 ChunkReader<ChunkSource> r(raw);PreservedBytes* fields[3]={entry?&entry->preset_root:nullptr,entry?&entry->preset_track:nullptr,entry?&entry->preset_sound:nullptr};
 for(auto* field:fields){uint32_t n;if(!r.u32(n) || n>raw.size-raw.pos)return false;ChunkSource stream;stream.data=raw.data+raw.pos;stream.size=n;if(!valid_preserved_root(stream))return false;
  if(store && field && (n>PreservedStore::kMaxEntryBytes || uint64_t(store->used_bytes())+n>PreservedStore::kMaxTotalBytes || !field->set(stream.data,n)))store->dropped_unknown=true;
  raw.pos+=n;
 }return raw.at_end();
}
inline bool finalize_definition_owners(ProjectState& p,PreservedStore& store){
 if(p.content_kind!=ContentKind::Song){
  auto& entry=store.tracks[0];entry.preset_track.steal_from(entry.auxiliary);
  ChunkBuffer raw;if(!raw.write(entry.preset_root.data,entry.preset_root.size))return false;
  for(unsigned i=0;i<store.chunk_count;++i){const auto& chunk=store.chunks[i];uint32_t frame;if(!raw.begin_frame(chunk.kind,chunk.version,frame,chunk.flags) || !raw.write(chunk.payload.data,chunk.payload.size) || !raw.end_frame(frame))return false;}
  if(raw.size>PreservedStore::kMaxEntryBytes || !entry.preset_root.set(raw.data,raw.size))return false;
  for(unsigned i=0;i<store.chunk_count;++i)store.chunks[i].clear();
  store.chunk_count=0;
 }
 for(unsigned t=0;t<kTracks;++t){auto& e=store.tracks[t];bool definition=!e.engn.empty() || !e.extra_sub.empty() || !e.preset_root.empty() || !e.preset_track.empty() || !e.preset_sound.empty();e.cookie=definition?next_definition_cookie():0;p.tracks[t].definition_cookie=e.cookie;}
 for(unsigned i=0;i<kPresetSlots;++i){auto& e=store.sounds[i];if(!e.auxiliary.empty())e.preset_sound.steal_from(e.auxiliary);e.cookie=e.empty()?0:next_definition_cookie();p.sounds[i].definition_cookie=e.cookie;}
 return true;
}
inline bool valid_pinned_references(const ProjectState& p){
 if(p.content_kind!=ContentKind::Song)return true;
 for(unsigned i=0;i<kPresetSlots;++i)if(p.sounds[i].identity.assigned())for(unsigned j=0;j<i;++j)if(same_preset_revision(p.sounds[i].identity,p.sounds[j].identity))return false;
 for(const auto& t:p.tracks)if(t.preset_identity.assigned()){if(t.preset_slot>=kPresetSlots)return false;const auto& sound=p.sounds[t.preset_slot];if(sound.instrument!=Instrument::Unknown && !same_preset_revision(t.preset_identity,sound.identity))return false;}
 return true;
}
inline bool encode_project_buffer(ChunkBuffer& out,const ProjectState& project,const PreservedStore* store=nullptr){
 if(out.size || !valid_pinned_references(project) || !valid_project(project) || (project.content_kind!=ContentKind::Song && !project.tracks[0].preset_identity.assigned()) || (project.content_kind==ContentKind::Kit && project.tracks[0].instrument!=Instrument::DrumKit))return false;
 ChunkWriter<ChunkBuffer> w(out);
 const uint8_t header[10]={'M','G','X','C',uint8_t(kFormatVersion),uint8_t(kFormatVersion>>8),0,0,0,0};
 if(!out.write(header,sizeof header))return false;
 uint32_t type_frame;if(!out.begin_frame(kChunkType,1,type_frame) || !w.u8(unsigned(project.content_kind)) || !out.end_frame(type_frame))return false;
 auto section=[&](uint32_t kind,uint16_t version,bool (*write)(ChunkWriter<ChunkBuffer>&,const ProjectState&)){
  uint32_t at;return out.begin_frame(kind,version,at) && write(w,project) && out.end_frame(at);
 };
 if(project.content_kind!=ContentKind::Song && !write_identity_frame(out,project.tracks[0].preset_identity))return false;
 if((project.content_kind==ContentKind::Song && !section(kChunkProj,3,write_proj_section)) || !section(kChunkAsst,1,write_asst_section))return false;
 if(project.content_kind==ContentKind::Song && (!section(kChunkAudio,1,write_audio_section) || !section(kChunkScen,1,write_scen_section) || !section(kChunkSong,2,write_song_section) || !section(kChunkMacr,5,write_macr_section) || !section(kChunkMfx,1,write_mfx_section)))return false;
 if(project.content_kind!=ContentKind::Song && (!write_fx_frame(out,project.preset_fx.present?project.preset_fx:project.tracks[0].preset_fx) || !write_output_trim_frame(out,project.tracks[0].output_trim)))return false;
 if(project.content_kind==ContentKind::Song){unsigned count=0;for(const auto& twin:project.twins)count+=twin.prepared();if(count){uint32_t frame;if(!out.begin_frame(kChunkTwin,1,frame) || !w.u8(uint8_t(count)))return false;for(unsigned t=0;t<kTracks;++t){const auto& twin=project.twins[t];if(!twin.prepared())continue;if(!w.u8(uint8_t(t)) || !w.u16(twin.source) || !w.u16(twin.sampled) || !w.u8(twin.active) || !w.block(twin.digest,32))return false;}if(!out.end_frame(frame))return false;}}
 uint32_t tracks_frame;if(!out.begin_frame(kChunkTrks,1,tracks_frame))return false;
 for(unsigned ti=0;ti<(project.content_kind==ContentKind::Song?kTracks:1u);++ti){
  const auto track=track_at(project,0,ti);uint32_t trck,sub;
  if(!out.begin_frame(kChunkTrck,1,trck) || !w.u8(uint8_t(ti)))return false;
  if(project.content_kind==ContentKind::Song && (!out.begin_frame(kChunkTcom,1,sub) || !write_tcom_section(w,track) || !out.end_frame(sub)))return false;
  const PreservedTrackEntry* entry=store?&store->tracks[ti]:nullptr;
  if(project.content_kind==ContentKind::Song){
   if(track.track_name[0]){unsigned n=0;while(track.track_name[n])++n;if(!out.begin_frame(kChunkTnam,1,sub) || !w.block(reinterpret_cast<const uint8_t*>(track.track_name),n) || !out.end_frame(sub))return false;}
   if(track.preset_identity.assigned() && (!out.begin_frame(kChunkPrid,1,sub) || !w.u16(track.preset_slot) || !write_preset_identity(w,track.preset_identity) || !out.end_frame(sub)))return false;
   if(!write_fx_frame(out,track.preset_fx) || !write_output_trim_frame(out,track.output_trim))return false;
  }
  if(!out.begin_frame(kChunkEngn,1,sub) || !write_track_engn(out,track,entry) || !out.end_frame(sub))return false;
  const auto* children=entry?(project.content_kind==ContentKind::Song?&entry->auxiliary:(!entry->preset_track.empty()?&entry->preset_track:&entry->auxiliary)):nullptr;
  if(children && !w.block(children->data,children->size))return false;
  if(project.content_kind==ContentKind::Song && !write_preserved_root(out,entry))return false;
  if(!out.end_frame(trck))return false;
 }
 if((store && !w.block(store->trks.data,store->trks.size)) || !out.end_frame(tracks_frame))return false;
 uint32_t pattern_offsets[kPatterns]{},pattern_sizes[kPatterns]{};
 unsigned pattern_count=project.content_kind==ContentKind::Song?kPatterns:0;
 for(unsigned pi=0;pi<pattern_count;++pi){
  const auto& pattern=project.patterns[pi];uint32_t pat;
  if(!out.begin_frame(kChunkPtrn,1,pat) || !w.u8(uint8_t(pi)) || !w.u8(pattern.swing))return false;
  for(unsigned ti=0;ti<(project.content_kind==ContentKind::Song?kTracks:1u);++ti){
   const auto track=track_at(project,pi,ti);uint32_t trck,sub;
   if(!out.begin_frame(kChunkPhrs,1,trck) || !w.u8(uint8_t(ti)) ||
      !out.begin_frame(kChunkPseq,4,sub) || !write_pseq_section(w,track,&project.unavailable_targets) || !out.end_frame(sub))return false;
   if(kit_sequence_present(track.kit.sequence()) && (!out.begin_frame(kChunkKseq,1,sub) || !write_kit_sequence(w,track.kit.sequence()) || !out.end_frame(sub)))return false;
   const auto& follow=track.clip_follow;
   if(follow.action!=ClipFollowAction::Off || follow.cycles!=1 || follow.target || follow.probability!=100){
    if(!out.begin_frame(kChunkClfa,1,sub) || !w.u8(uint8_t(follow.action)) || !w.u8(follow.cycles) ||
       !w.u8(follow.target) || !w.u8(follow.probability) || !out.end_frame(sub))return false;
   }
   if((store && !w.block(store->patterns[pi].phrases[ti].data,store->patterns[pi].phrases[ti].size)) || !out.end_frame(trck))return false;
  }
  if(pattern.mute_mask){uint32_t sub;if(!out.begin_frame(kChunkPmut,1,sub) || !w.u16(pattern.mute_mask) || !out.end_frame(sub))return false;}
  if((store && !w.block(store->patterns[pi].children.data,store->patterns[pi].children.size)) || !out.end_frame(pat))return false;
  const uint32_t size=out.size-pat;bool referenced=false;
  for(unsigned previous=0;previous<pi;++previous){
   if(pattern_sizes[previous]!=size || __builtin_memcmp(out.data+pattern_offsets[previous]+13,out.data+pat+13,size-13))continue;
   out.size=pat;uint32_t ref;if(!out.begin_frame(kChunkPref,1,ref) || !w.u8(uint8_t(pi)) || !w.u8(uint8_t(previous)) || !out.end_frame(ref))return false;
   pattern_offsets[pi]=pattern_offsets[previous];pattern_sizes[pi]=size;referenced=true;break;
  }
  if(!referenced){pattern_offsets[pi]=pat;pattern_sizes[pi]=size;}
 }
 uint32_t sounds;
 if(!out.begin_frame(kChunkSnds,1,sounds))return false;
 for(unsigned si=0;project.content_kind==ContentKind::Song && si<kPresetSlots;++si){
  uint32_t snd;const PreservedTrackEntry* entry=store?&store->sounds[si]:nullptr;
  if(project.sounds[si].instrument==Instrument::Empty && (!entry || entry->empty()))continue;
  if(entry && !entry->opaque.empty() && project.sounds[si].instrument==Instrument::Unknown){if(!w.block(entry->opaque.data,entry->opaque.size))return false;continue;}
  const auto& sound=project.sounds[si];uint32_t engine_frame;
  if(!out.begin_frame(kChunkSnd,2,snd) || !w.u16(uint16_t(si)))return false;
  if(sound.identity.assigned() && !write_identity_frame(out,sound.identity))return false;
  if(!write_fx_frame(out,sound.fx) || !write_output_trim_frame(out,sound.output_trim) || !out.begin_frame(kChunkEngn,1,engine_frame) || !write_sound_engn(out,sound,entry) || !out.end_frame(engine_frame))return false;
  const auto* children=entry?(!entry->preset_sound.empty()?&entry->preset_sound:&entry->auxiliary):nullptr;
  if(children && !w.block(children->data,children->size))return false;
  if(!write_preserved_root(out,entry,true))return false;
  if(!out.end_frame(snd))return false;
 }
 if((store && !w.block(store->snds.data,store->snds.size)) || !out.end_frame(sounds))return false;
 // Preserve foreign bytes and flags verbatim in encounter order.
 if(store)for(unsigned i=0;i<store->chunk_count;++i){
  const auto& chunk=store->chunks[i];uint32_t at;
  if(!out.begin_frame(chunk.kind,chunk.version,at,chunk.flags) || !w.block(chunk.payload.data,chunk.payload.size) || !out.end_frame(at))return false;
 }
 if(store && project.content_kind!=ContentKind::Song && !write_preserved_root(out,&store->tracks[0],false,true))return false;
 if(store && project.content_kind!=ContentKind::Song && !store->tracks[0].preset_root.empty() && !out.write(store->tracks[0].preset_root.data,store->tracks[0].preset_root.size))return false;
 if(out.size-10>kMaxFormatBytes)return false;
 out.patch_u32(6,out.size-10);
 ProjectCrc crc;for(uint32_t i=4;i<out.size;++i)crc.add(out.data[i]);
 return w.u32(crc.value());
}
template<class Sink> bool encode_project(Sink& sink,const ProjectState& project,const PreservedStore* store=nullptr){
 ChunkBuffer out;return encode_project_buffer(out,project,store) && sink.write(out.data,out.size);
}
// --- Section readers -------------------------------------------------------
inline bool read_bool8(ChunkReader<ChunkSource>& r,bool& out){uint8_t x;if(!r.u8(x)||x>1)return false;out=x!=0;return true;}
inline bool read_proj_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 if(!r.u16(p.tempo)||!r.u8(p.active_pattern)||!r.u8(p.active_scene)||!r.u16(p.muted_tracks)||!r.u16(p.solo_tracks))return false;
 for(uint8_t& route:p.midi_input_route)if(!r.u8(route))return false;
 if(!read_bool8(r,p.midi_clock_slave)||!r.u32(p.random_seed))return false;
 for(float& c:p.tuning)if(!r.f32(c))return false;
 if(!r.u16(p.gesture.count)||p.gesture.count>128||!r.u16(p.gesture.ticks))return false;
 for(unsigned i=0;i<p.gesture.count;++i){auto& e=p.gesture.points[i];uint8_t target;if(!r.u16(e.tick)||!read_parameter_target(r,e.parameter,&p.unavailable_targets)||!r.f32(e.value)||!r.u8(e.track)||!r.u8(e.pattern)||!r.u8(target))return false;e.target=GestureTarget(target);}
 auto& ui=p.ui;for(auto& o:ui.keyboard_octave){uint8_t v;if(!r.u8(v))return false;o=int8_t(v);if(o< -4 || o>4)return false;}
 uint8_t ignored_theme=0;ui.theme=0;if(!r.u8(ignored_theme)||!r.u8(ui.motion)||!r.u8(ui.order)||!r.u8(ui.strip)||!read_bool8(r,ui.preview565)||!read_bool8(r,ui.note_quantized)||!read_bool8(r,ui.note_replace)||!read_bool8(r,ui.metronome)||!read_bool8(r,ui.count_in)||!read_bool8(r,ui.click_main))return false;
 if(!r.u8(ui.arp_direction)||!r.u8(ui.arp_octaves)||!r.u8(ui.repeat_gate)||!r.u8(ui.repeat_rate)||!r.u8(ui.chord_shape)||!r.u8(ui.chord_inversion)||!r.u16(ui.wave_profile_mask)||!r.u16(ui.wave_clock_mask))return false;
 return src.at_end();
}
inline bool read_asst_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 for(auto& a:p.assets){
  for(char& c:a.path){uint8_t x;if(!r.u8(x))return false;c=char(x);}
  auto& m=a.metadata;uint8_t favorite;
  if(!r.u32(m.rate)||!r.u32(m.frames)||!r.u32(m.content_crc)||!r.u16(m.bits)||!r.u16(m.bpm)||!r.u8(m.channels)||!r.u8(m.root)||!r.u8(favorite)||favorite>1)return false;
  m.favorite=favorite;
 }
 return src.at_end();
}
inline bool read_morph_snapshot(ChunkReader<ChunkSource>& r,ParameterSnapshot& s){
 for(unsigned i=0;i<kCommonMorphParameters;++i)if(morph_common_parameter(morph_parameter_at(i)) && !r.f32(s.values[i]))return false;
 uint8_t length;uint16_t count;char guid[65];if(!r.u8(length) || !length || length>64)return false;
 for(unsigned i=0;i<length;++i){uint8_t c;if(!r.u8(c))return false;guid[i]=char(c);}guid[length]=0;
 Instrument engine;bool known=engine_from_guid(guid,length,engine);if(!r.u16(count) || count>kEngineMorphParameters)return false;
 if(known){const auto* d=engine_descriptor(unsigned(engine));if(!d || d->parameters.count!=count)return false;s.engine=engine;}else{s.engine=Instrument::Unknown;__builtin_memcpy(s.unknown_guid,guid,length+1);s.unknown_count=count;}
 for(unsigned i=0;i<count;++i){float v;if(!r.f32(v))return false;s.engine_values[i]=v;}
 return valid_engine_morph(s);
}
inline bool read_scen_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 for(auto& scene:p.scenes){for(uint8_t& x:scene.pattern_for_track)if(!r.u8(x))return false;if(!r.u16(scene.mute_mask))return false;}
 if(!r.u8(p.chain.length)||!r.u8(p.chain.bars)||!read_bool8(r,p.chain.enabled))return false;
 for(uint8_t& scene:p.chain.scenes)if(!r.u8(scene))return false;
 uint8_t mode;if(!r.u8(mode)||!r.u8(p.chain.probability))return false;p.chain.mode=FollowMode(mode);
 for(auto& rule:p.chain.follow){uint8_t condition;if(!r.u8(rule.bars)||!r.u8(rule.probability)||!r.u8(rule.weight)||!r.u8(condition))return false;rule.condition=FollowCondition(condition);}
 for(auto& clip:p.clips)if(!r.u8(clip.pattern)||!read_bool8(r,clip.playing))return false;
 return src.at_end();
}
inline bool read_song_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);auto& song=p.song;song=SongState{};
 if(!r.u8(song.length) || song.length>32 || !read_bool8(r,song.enabled) || !read_bool8(r,song.loop))return false;
 for(unsigned i=0;i<song.length;++i){auto& s=song.sections[i];uint8_t transpose;if(!r.u8(s.scene) || !r.u16(s.bars) || !r.u8(transpose) || !r.u16(s.mute_mask) || !r.u32(s.ticks) || !r.u16(s.playing_mask) || !r.u16(s.restart_mask))return false;s.transpose=int8_t(transpose);for(auto& pattern:s.patterns)if(!r.u8(pattern))return false;}
 return src.at_end() && valid_song(song);
}
 inline bool read_macr_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 for(auto& macro:p.macros){
  if(!read_bool8(r,macro.active)||!r.f32(macro.value))return false;
  for(auto& route:macro.routes){uint8_t on,mode;if(!r.u8(on)||on>1||!read_parameter_target(r,route.parameter,&p.unavailable_targets)||!r.u16(route.tracks)||!r.f32(route.low)||!r.f32(route.high)||!r.u8(mode))return false;route.enabled=on;route.mode=LayerMode(mode&3);route.curve=MacroCurve((mode>>2)&3);if(((mode>>4)&7)>unsigned(ModSource::Velocity))return false;route.source=ModSource((mode>>4)&7);}
 }
 if(!read_bool8(r,p.morph_enabled)||!r.f32(p.morph_position))return false;
 for(unsigned t=0;t<kTracks;++t){
  if(!read_morph_snapshot(r,p.morph_a[t]))return false;
  if(!read_morph_snapshot(r,p.morph_b[t]))return false;
 }
 {
  if(!r.u8(p.morph_set) || p.morph_set>3)return false;
  for(unsigned s=0;s<3;++s)for(unsigned t=0;t<kTracks;++t){
   if(!read_morph_snapshot(r,p.morph_extra_a[s][t]))return false;
   if(!read_morph_snapshot(r,p.morph_extra_b[s][t]))return false;
  }
 }
 return src.at_end();
}
inline bool read_mfx_section(ChunkSource& src,ProjectState& p){
 ChunkReader<ChunkSource> r(src);
 auto& e=p.effects;
 if(!r.f32(e.delay_mix)||!r.f32(e.delay_feedback)||!r.u16(e.delay_ms)||!r.f32(e.reverb_mix)||!r.f32(e.reverb_decay)||!r.f32(e.reverb_damp))return false;
 for(float& v:e.compressor)if(!r.f32(v))return false;
 if(!r.f32(e.live_fx_type)||!r.f32(e.live_fx_amount))return false;
 auto& m=p.routing;
 if(!r.f32(m.main)||!r.f32(m.cue)||!r.f32(m.preview)||!r.f32(m.monitor)||!r.u8(m.monitor_bus)||!r.u8(m.capture_bus))return false;
 for(auto& g:m.groups){
  if(!r.u16(g.members)||!r.f32(g.gain)||!r.u8(g.output))return false;
  for(float& v:g.insert_fx)if(!r.f32(v))return false;
  for(float& v:g.send)if(!r.f32(v))return false;
  if(!r.f32(g.parallel))return false;
 }
 return src.at_end();
}
// --- Persistent track (TCOM) and phrase sequence (PSEQ) ---------------------------------------------------
inline bool read_full_step(ChunkReader<ChunkSource>& r,StepState& s, UnavailableTargetOwners* owners=nullptr){
 uint8_t enabled,condition;uint16_t micro;
 if(!r.u8(enabled)||enabled>63||!r.u8(s.note)||!r.u8(s.velocity)||!r.u8(s.probability)||!r.u8(s.ratchet)||!r.u16(micro)||!r.u16(s.gate))return false;
 s.enabled=(enabled&1)!=0;s.lock_only=(enabled&2)!=0;s.chromatic=(enabled&4)!=0;s.native_drum=(enabled&8)!=0;s.slide((enabled&16)!=0);s.accent((enabled&32)!=0);s.microtiming=int16_t(micro);
 for(auto& lock:s.locks){uint8_t active;if(!read_parameter_target(r,lock.id,owners)||!r.f32(lock.value)||!r.u8(active)||active>1)return false;lock.enabled=active;}
 if(!r.u8(condition)||!r.u8(s.cycle_every)||!r.u8(s.cycle_phase)||!r.u16(s.sound))return false;
 s.condition=TrigCondition(condition);
 for(uint8_t& note:s.chord)if(!r.u8(note))return false;
 return true;
}
inline bool read_tcom_section(ChunkSource& src,TrackView t){
 ChunkReader<ChunkSource> r(src);
 if(!r.u8(t.midi_channel)||!r.u8(t.root_note)||!r.f32(t.level)||!r.f32(t.pan))return false;
 if(!r.u8(t.groove.preset)||!r.u8(t.groove.amount))return false;
 for(unsigned i=0;i<16;++i){uint8_t time,vel;if(!r.u8(time)||!r.u8(vel))return false;t.groove.timing[i]=int8_t(time);t.groove.velocity[i]=int8_t(vel);}
 for(uint8_t& cc:t.midi_cc)if(!r.u8(cc))return false;
 for(uint8_t& used:t.midi_cc_used)if(!r.u8(used))return false;
 for(auto& slot:t.nrpn){uint8_t enabled,registered;if(!r.u16(slot.number)||!r.u16(slot.value)||!r.u8(enabled)||enabled>1||!r.u8(registered)||registered>1)return false;slot.enabled=enabled;slot.registered=registered;}
 if(!r.u16(t.pitch_bend)||!r.u8(t.pressure)||!r.u8(t.expression_used)||!r.u8(t.midi_program))return false;
 if(!r.u8(t.arp_mode)||!r.u8(t.arp_rate)||!r.u8(t.arp_octaves))return false;
 uint8_t transpose;if(!r.u8(transpose)||!r.u16(t.scale_mask)||!r.u8(t.scale_root)||!r.f32(t.velocity_scale))return false;t.transpose=int8_t(transpose);
 float* extras[]={&t.cutoff,&t.drive,&t.delay_send,&t.crusher,&t.lfo_depth,&t.lfo_rate,&t.aftertouch_depth};
 for(float* x:extras)if(!r.f32(*x))return false;
 if(!r.f32(t.reverb_send))return false;
 if(!r.u8(t.output_bus)||!r.u8(t.send_prefader))return false;
 for(float& v:t.insert_fx)if(!r.f32(v))return false;
 for(float& v:t.compressor)if(!r.f32(v))return false;
 if(!r.u8(t.insert_disabled))return false;
 return r.f32(t.texture.values[12]) && r.f32(t.lfo_sync) && r.f32(t.lfo_division) && src.at_end();
}
inline bool read_pseq_section(ChunkSource& src,TrackView t, UnavailableTargetOwners* owners=nullptr){
 ChunkReader<ChunkSource> r(src);
 if(!r.u16(t.length))return false;
 for(unsigned cell=0;cell<64;++cell)if(!read_full_step(r,t.steps[cell],owners))return false;
 if(!read_extended_steps(r,t.steps,owners))return false;
 if(!read_bool8(r,t.mutation.frozen))return false;
 if(t.mutation.frozen){
  for(unsigned cell=0;cell<64;++cell)if(!read_full_step(r,t.mutation_source[cell],owners))return false;
  if(!read_extended_steps(r,t.mutation_source,owners))return false;
 }
 if(!r.u32(t.mutation.seed)||!r.u8(t.mutation.amount))return false;
 auto& a=t.accumulator;uint8_t pitch,lo,hi,velocity,gate,mode,scale;
 if(!r.u8(pitch)||!r.u8(lo)||!r.u8(hi)||!r.u8(velocity)||!r.u8(gate)||!r.u8(mode)||!r.u8(a.reset)||!r.u8(scale)||scale>1)return false;
 a.pitch=int8_t(pitch);a.minimum=int8_t(lo);a.maximum=int8_t(hi);a.velocity=int8_t(velocity);a.gate=int8_t(gate);a.mode=AccumulatorMode(mode);a.scale_degrees=scale;
 if(!read_bool8(r,t.lanes_enabled))return false;
 for(auto& lane:t.lanes){
  uint8_t direction;if(!r.u8(lane.length)||!r.u8(direction))return false;lane.direction=PlaybackDirection(direction);
  for(unsigned i=0;i<16;++i)if(!r.u8(lane.values[i])||!r.u8(lane.duration[i]))return false;
 }
 for(auto& lane:t.automation){
  uint8_t on,mode,len;if(!r.u8(on)||on>1||!read_parameter_target(r,lane.parameter,owners)||!r.u8(len)||!r.u8(mode))return false;
  lane.enabled=on;lane.length=len?len:256;lane.mode=LayerMode(mode);
  for(unsigned i=0;i<64;++i)if(!r.f32(lane.values[i]))return false;
  if(!read_extended_automation(r,lane))return false;
 }
 uint8_t direction;if(!r.u8(direction)||!r.u8(t.speed_num)||!r.u8(t.speed_den))return false;t.direction=PlaybackDirection(direction);
 if(!r.u8(t.frozen_asset)||!r.u16(t.sound_override))return false;
 return src.at_end();
}
// --- Engine payload (ENGN) decode with fallback ----------------------------
inline void reset_engine_fields(Instrument instrument,const EngineStateView& target){
 const auto* descriptor=engine_descriptor(unsigned(instrument));
 const auto& state=descriptor?descriptor->state:engine_state_contract();
 for(unsigned i=0;i<state.count;++i)state.chunks[i].defaults(target);
}
inline void reset_track_engine_fields(TrackView t){t.preset_name[0]=0;reset_engine_fields(t.instrument,engine_state_view(t));}
inline void reset_sound_engine_fields(SoundState& s){s.preset_name[0]=0;reset_engine_fields(s.instrument,engine_state_view(s));}
// Keep the raw ENGN payload for write-back. Cap rejections are recorded
// on the store (never a decode failure): the track still opens, muted.
inline void preserve_engn(PreservedStore* store,PreservedTrackEntry* entry,const uint8_t* raw,uint32_t size){
 if(!entry)return;
 if(size>PreservedStore::kMaxEntryBytes || !store || store->used_bytes()+size>PreservedStore::kMaxTotalBytes){
  if(store)store->dropped_unknown=true;
  entry->engn.clear();return;
 }
 if(!entry->engn.set(raw,size))store->dropped_unknown=true;
}
// Shared ENGN sub-chunk walk. The caller has already resolved the GUID
// to `instrument` and reset nothing; field sinks are the track/sound
// engine fields. Returns false when a known sub-chunk is malformed or
// fails validation: the caller then applies the preserve+mute fallback.
inline bool parse_engn_subchunks(ChunkSource& src,const EngineStateView& f,PreservedStore* store,PreservedTrackEntry* entry,Instrument instrument){
 ChunkReader<ChunkSource> r(src);
 ChunkBuffer extras;bool extras_overflow=false,name_seen=false;unsigned seen=0;
 while(!src.at_end()){
  uint32_t kind;uint16_t version,flags;ChunkSource sub;
  if(!read_framed(r,kind,version,sub,&flags))return false;
  bool handled=false;
  if(kind==kChunkName && version==1 && f.preset_name){if(name_seen || flags || sub.size>=96)return false;name_seen=true;for(unsigned i=0;i<sub.size;++i){unsigned c=sub.data[i];if(c<32 || c>126)return false;f.preset_name[i]=char(c);}f.preset_name[sub.size]=0;handled=true;}
  const auto* descriptor=engine_descriptor(unsigned(instrument));
  const auto& state=descriptor?descriptor->state:engine_state_contract();
  for(unsigned i=0;i<state.count;++i){const auto& chunk=state.chunks[i];
   if(chunk.fourcc!=kind || chunk.version!=version || flags || (kind==kChunkKit && !f.kit))continue;
   if((seen&(1u<<i)) || !chunk.decode(sub,f)){return false;}seen|=1u<<i;
   handled=true;break;
  }
  if(!handled){
   // Unknown kind or a newer sub-chunk version: keep the raw record so
   // it is written back after the typed sub-chunks (GUID unchanged).
   if(extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){
    ChunkWriter<ChunkBuffer> ew(extras);
    if(!ew.u32(kind)||!ew.u16(version)||!ew.u16(flags)||!ew.u32(sub.size)||!ew.block(sub.data,sub.size))extras_overflow=true;
   }else extras_overflow=true;
  }
 }
 if(entry){
  if(extras_overflow){if(store)store->dropped_unknown=true;}
  else if(extras.size){if(entry->extra_sub.set(extras.data,extras.size))entry->extra_for=int32_t(instrument);}
 }
 const auto* descriptor=engine_descriptor(unsigned(instrument));return descriptor && (seen&descriptor->state.required)==descriptor->state.required;
}
inline bool track_engine_fields_valid(ConstTrackView t){
 if(!valid_layer(t.layer) || !valid_additive(t.additive) || !valid_phase_dist(t.phase_dist) || !valid_formant(t.formant) || !valid_granular(t.granular) || !valid_soundfont(t.soundfont) || !valid_fm6(t.fm6) || !valid_virtual_analog(t.virtual_analog) || !valid_wavetable(t.wavetable) || !valid_texture(t.texture) || !valid_resonator(t.resonator) || t.plaits_model>=24 || t.sample_slot>=kSampleAssets || (t.frozen_asset!=255 && t.frozen_asset>=kSampleAssets))return false;
 for(float v:t.engine_parameters)if(!(v>=0 && v<=1))return false;
 if(!valid_sample_edit(t.sample_edit)||!valid_creative(t.creative))return false;
 if(t.instrument==Instrument::DrumKit && !valid_drum_kit(t.kit))return false;
 return true;
}
inline bool sound_engine_fields_valid(const SoundState& s){return valid_sound(s);}
// Parse one ENGN payload into a track. Structural ENGN problems and
// invalid engine payloads fall back to preserve+mute for this track
// only (SPEC 2.9); the function itself only fails on OOM-free paths —
// it always returns true so one engine can never reject the project.
inline bool parse_track_engn(ChunkSource& src,TrackView t,PreservedStore* store,PreservedTrackEntry* entry){
 const uint8_t* raw=src.data;uint32_t raw_size=src.size;
 ChunkReader<ChunkSource> r(src);
 uint8_t guid_length=0;uint16_t schema=0;Instrument instrument;
 bool resolved=false;
 if(r.u8(guid_length) && guid_length<=64 && src.remaining()>=guid_length+2u){
  char guid[65];bool ok=true;
  for(unsigned i=0;i<guid_length;++i){uint8_t c;if(!r.u8(c)){ok=false;break;}guid[i]=char(c);}
  guid[guid_length]=0;
  if(ok && r.u16(schema))resolved=engine_from_guid(guid,guid_length,instrument);
 }
 if(!resolved || schema!=1){
  reset_track_engine_fields(t);t.instrument=Instrument::Unknown;
  preserve_engn(store,entry,raw,raw_size);
  return true;
 }
 t.instrument=instrument;
 const auto f=engine_state_view(t);
 ChunkSource rest;rest.data=src.data+src.pos;rest.size=src.size-src.pos;
 if(!parse_engn_subchunks(rest,f,store,entry,instrument) || !track_engine_fields_valid(t)){
  reset_track_engine_fields(t);t.instrument=Instrument::Unknown;
  if(entry)entry->extra_sub.clear(),entry->extra_for=-1;
  preserve_engn(store,entry,raw,raw_size);
 }
 return true;
}
inline bool parse_sound_engn(ChunkSource& src,SoundState& s,PreservedStore* store,PreservedTrackEntry* entry){
 const uint8_t* raw=src.data;uint32_t raw_size=src.size;
 ChunkReader<ChunkSource> r(src);
 uint8_t guid_length=0;uint16_t schema=0;Instrument instrument;
 bool resolved=false;
 if(r.u8(guid_length) && guid_length<=64 && src.remaining()>=guid_length+2u){
  char guid[65];bool ok=true;
  for(unsigned i=0;i<guid_length;++i){uint8_t c;if(!r.u8(c)){ok=false;break;}guid[i]=char(c);}
  guid[guid_length]=0;
  if(ok && r.u16(schema))resolved=engine_from_guid(guid,guid_length,instrument);
 }
 if(!resolved || schema!=1){
  reset_sound_engine_fields(s);s.instrument=Instrument::Unknown;
  preserve_engn(store,entry,raw,raw_size);
  return true;
 }
 s.instrument=instrument;
 const auto f=engine_state_view(s);
 ChunkSource rest;rest.data=src.data+src.pos;rest.size=src.size-src.pos;
 if(!parse_engn_subchunks(rest,f,store,entry,instrument) || !sound_engine_fields_valid(s)){
  reset_sound_engine_fields(s);s.instrument=Instrument::Unknown;
  if(entry)entry->extra_sub.clear(),entry->extra_for=-1;
  preserve_engn(store,entry,raw,raw_size);
 }
 return true;
}
// --- Container readers -----------------------------------------------------
inline bool parse_trck(ChunkSource& src,ProjectState& p,PreservedStore* store,uint16_t& seen){
 ChunkReader<ChunkSource> r(src);uint8_t index;if(!r.u8(index) || index>=kTracks || (seen&(1u<<index)))return false;seen|=uint16_t(1u<<index);
 auto track=track_at(p,0,index);auto* entry=store?&store->tracks[index]:nullptr;bool common=false,engine=false,name=false,reference=false,fx=false,trim=false,root=false;ChunkBuffer extras;
 while(!src.at_end()){
  uint32_t kind;uint16_t version,flags;ChunkSource sub;if(!read_framed(r,kind,version,sub,&flags))return false;
  if(kind==kChunkTcom && version==1){if(common || flags || !read_tcom_section(sub,track))return false;common=true;}
  else if(kind==kChunkTnam && version==1){if(name || flags || !sub.size || sub.size>=96)return false;for(unsigned i=0;i<sub.size;++i){unsigned c=sub.data[i];if(c<32 || c>126)return false;track.track_name[i]=char(c);}track.track_name[sub.size]=0;name=true;}
  else if(kind==kChunkPrid && version==1){ChunkReader<ChunkSource> ir(sub);if(reference || flags || !ir.u16(track.preset_slot) || !read_preset_identity(ir,track.preset_identity) || !track.preset_identity.assigned() || !sub.at_end())return false;reference=true;}
  else if(kind==kChunkPfxs && version==1){if(fx || flags || !read_fx_frame(sub,track.preset_fx))return false;fx=true;}
  else if(kind==kChunkPtrm && version==1){if(trim || flags || p.content_kind!=ContentKind::Song || !read_output_trim_frame(sub,track.output_trim))return false;trim=true;}
  else if(kind==kChunkPunk && version==1){if(root || flags || !read_preserved_root(sub,store,entry))return false;root=true;}
  else if(kind==kChunkEngn){if(engine)return false;engine=true;if(version!=1 || flags){reset_track_engine_fields(track);track.instrument=Instrument::Unknown;preserve_engn(store,entry,sub.data,sub.size);}else if(!parse_track_engn(sub,track,store,entry))return false;}
  else if(extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(extras,kind,version,flags,sub))return false;}else if(store)store->dropped_unknown=true;
 }
 preserve_auxiliary(store,entry,extras);
 return engine && (p.content_kind!=ContentKind::Song || common);
}
inline bool parse_trks(ChunkSource& src,ProjectState& p,PreservedStore* store){
 ChunkReader<ChunkSource> r(src);uint16_t seen=0;ChunkBuffer extras;
 while(!src.at_end()){uint32_t kind;uint16_t version,flags;ChunkSource sub;if(!read_framed(r,kind,version,sub,&flags))return false;
  if(kind==kChunkTrck){if(version!=1 || flags || !parse_trck(sub,p,store,seen))return false;}
  else if(extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(extras,kind,version,flags,sub))return false;}else if(store)store->dropped_unknown=true;
 }
 preserve_scope(store,store?&store->trks:nullptr,extras);return seen==(p.content_kind==ContentKind::Song?0xffff:1);
}
inline bool parse_phrs(ChunkSource& src,ProjectState& p,unsigned pattern,uint16_t& seen,PreservedStore* store){
 ChunkReader<ChunkSource> r(src);uint8_t index;if(!r.u8(index) || index>=kTracks || (seen&(1u<<index)))return false;seen|=uint16_t(1u<<index);
 auto track=track_at(p,pattern,index);bool sequence=false,kit=false,follow=false;ChunkBuffer extras;
 while(!src.at_end()){
  uint32_t kind;uint16_t version,flags;ChunkSource sub;if(!read_framed(r,kind,version,sub,&flags))return false;
  if(kind==kChunkPseq){if(sequence || flags || version!=4 || !read_pseq_section(sub,track,&p.unavailable_targets))return false;sequence=true;}
  else if(kind==kChunkKseq){ChunkReader<ChunkSource> kr(sub);if(kit || flags || version!=1 || !read_kit_sequence(kr,track.kit.sequence()) || !sub.at_end())return false;kit=true;}
  else if(kind==kChunkClfa){ChunkReader<ChunkSource> fr(sub);uint8_t action;if(follow || flags || version!=1 || !fr.u8(action) || !fr.u8(track.clip_follow.cycles) || !fr.u8(track.clip_follow.target) || !fr.u8(track.clip_follow.probability) || !sub.at_end())return false;follow=true;track.clip_follow.action=ClipFollowAction(action);if(!valid_clip_follow(track.clip_follow))return false;}
  else if(extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(extras,kind,version,flags,sub))return false;}else if(store)store->dropped_unknown=true;
 }
 preserve_scope(store,store?&store->patterns[pattern].phrases[index]:nullptr,extras);return sequence;
}
inline bool parse_ptrn(ChunkSource& src,ProjectState& p,PreservedStore* store){
 ChunkReader<ChunkSource> r(src);uint8_t pattern_index,swing;if(!r.u8(pattern_index)||pattern_index>=kPatterns||!r.u8(swing))return false;
 p.patterns[pattern_index].swing=swing;uint16_t seen=0;bool mute=false;ChunkBuffer extras;
 while(!src.at_end()){
  uint32_t kind;uint16_t version,flags;ChunkSource sub;if(!read_framed(r,kind,version,sub,&flags))return false;
  if(kind==kChunkPmut && version==1){ChunkReader<ChunkSource> sr(sub);if(mute || flags || !sr.u16(p.patterns[pattern_index].mute_mask) || !sub.at_end())return false;mute=true;}
  else if(kind==kChunkPhrs){if(version!=1 || flags || !parse_phrs(sub,p,pattern_index,seen,store))return false;}
  else if(extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(extras,kind,version,flags,sub))return false;}else if(store)store->dropped_unknown=true;
 }
 preserve_scope(store,store?&store->patterns[pattern_index].children:nullptr,extras);return seen==0xffff;
}
inline bool parse_snds(ChunkSource& src,ProjectState& p,PreservedStore* store){
 ChunkReader<ChunkSource> r(src);bool seen[kPresetSlots]{};ChunkBuffer container_extras;
 while(!src.at_end()){
  unsigned at=src.pos;uint32_t kind;uint16_t version,flags;ChunkSource sub;if(!read_framed(r,kind,version,sub,&flags))return false;
  if(kind!=kChunkSnd){if(container_extras.size+12+sub.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(container_extras,kind,version,flags,sub))return false;}else if(store)store->dropped_unknown=true;continue;}
  ChunkReader<ChunkSource> sr(sub);uint16_t index;if(!sr.u16(index) || index>=kPresetSlots || seen[index])return false;seen[index]=true;
  auto& sound=p.sounds[index];auto* entry=store?&store->sounds[index]:nullptr;
  if(version!=2 || flags){sound.instrument=Instrument::Unknown;if(store && entry){unsigned n=src.pos-at;if(n>PreservedStore::kMaxEntryBytes || store->used_bytes()+n>PreservedStore::kMaxTotalBytes || !entry->opaque.set(src.data+at,n))store->dropped_unknown=true;}continue;}
  bool identity=false,fx=false,trim=false,engine=false,root=false;ChunkBuffer extras;
  while(!sub.at_end()){
   uint32_t child;uint16_t rev,child_flags;ChunkSource body;if(!read_framed(sr,child,rev,body,&child_flags))return false;
   if(child==kChunkIden && rev==1){ChunkReader<ChunkSource> ir(body);if(identity || child_flags || !read_preset_identity(ir,sound.identity) || !sound.identity.assigned() || !body.at_end())return false;identity=true;}
   else if(child==kChunkPfxs && rev==1){if(fx || child_flags || !read_fx_frame(body,sound.fx))return false;fx=true;}
   else if(child==kChunkPtrm && rev==1){if(trim || child_flags || !read_output_trim_frame(body,sound.output_trim))return false;trim=true;}
   else if(child==kChunkPunk && rev==1){if(root || child_flags || !read_preserved_root(body,store,entry))return false;root=true;}
   else if(child==kChunkEngn){if(engine)return false;engine=true;if(rev!=1 || child_flags){reset_sound_engine_fields(sound);sound.instrument=Instrument::Unknown;preserve_engn(store,entry,body.data,body.size);}else if(!parse_sound_engn(body,sound,store,entry))return false;}
   else if(extras.size+12+body.size<=PreservedStore::kMaxEntryBytes){if(!append_unknown_frame(extras,child,rev,child_flags,body))return false;}else if(store)store->dropped_unknown=true;
  }
  if(!engine)return false;
  preserve_auxiliary(store,entry,extras);
 }
 preserve_scope(store,store?&store->snds:nullptr,container_extras);return true;
}
// --- Top-level decode ------------------------------------------------------
template<class Source> bool decode_project_impl(Source& source,ProjectState* project,PreservedStore* store,ProjectState* workspace=nullptr){
 FormatReader<Source> r(source);
 const char magic[4]={'M','G','X','C'};
 for(char c:magic){uint8_t x;if(!r.raw(x)||x!=uint8_t(c))return false;}
 uint16_t version;uint32_t payload_length;
 if(!r.u16(version)||version!=kFormatVersion)return false;
 if(!r.u32(payload_length)||payload_length>kMaxFormatBytes)return false;
 auto* fresh=workspace?workspace:alloc_new<ProjectState>();

 if(!fresh)return false;
 PreservedStore temp_store;
 PreservedStore* active_store=store?&temp_store:nullptr;
 fresh->preset_fx={};for(auto& twin:fresh->twins)twin=TwinState{};
 float file_trim=1;uint32_t consumed=0;bool ok=true,saw_type=false,saw_tracks=false,saw_pfxs=false,saw_identity=false,saw_sounds=false,saw_punk=false;uint32_t singleton_seen=0;PresetIdentity file_identity{};bool saw_pattern[kPatterns]{};
 while(ok && consumed<payload_length){
  uint32_t kind;uint16_t chunk_version,flags;uint32_t length;
  if(payload_length-consumed<12 || !r.u32(kind)||!r.u16(chunk_version)||!r.u16(flags)||!r.u32(length)||length>payload_length-consumed-12){ok=false;break;}
  consumed+=12+length;
  ChunkBuffer payload;ChunkSource section;
  if(!r.payload(length,section,payload)){ok=false;break;}
  bool known_typed=(chunk_version==project_section_revision(kind)) && (kind==kChunkTwin || kind==kChunkPunk || kind==kChunkIden || kind==kChunkPtrm || kind==kChunkPfxs || kind==kChunkTrks || kind==kChunkType || kind==kChunkPref || kind==kChunkAudio || kind==kChunkProj || kind==kChunkAsst || kind==kChunkScen || kind==kChunkSong || kind==kChunkMacr || kind==kChunkMfx || kind==kChunkPtrn || kind==kChunkSnds);
  if(!known_typed){
   if(active_store)active_store->add_chunk(kind,chunk_version,flags,section.data,section.size);
   continue;
  }
  if(flags){ok=false;break;}
  static constexpr uint32_t singletons[]={kChunkTwin,kChunkType,kChunkIden,kChunkPfxs,kChunkPtrm,kChunkPunk,kChunkTrks,kChunkAudio,kChunkProj,kChunkAsst,kChunkScen,kChunkSong,kChunkMacr,kChunkMfx,kChunkSnds};
  for(unsigned i=0;i<sizeof singletons/sizeof singletons[0];++i)if(kind==singletons[i]){if(singleton_seen&(1u<<i))ok=false;singleton_seen|=1u<<i;}
  if(!ok)break;
  if(!saw_type && kind!=kChunkType){ok=false;break;}
  switch(kind){
   case kChunkType: {if(saw_type || section.size!=1 || section.data[0]<1 || section.data[0]>3){ok=false;break;}fresh->content_kind=ContentKind(section.data[0]);saw_type=true;break;}
   case kChunkPref: {
    if(section.size!=2){ok=false;break;}unsigned dest=section.data[0],from=section.data[1];
    if(dest>=kPatterns || from>=dest || !saw_pattern[from] || saw_pattern[dest]){ok=false;break;}
    fresh->patterns[dest]=fresh->patterns[from];if(active_store){if(uint64_t(active_store->used_bytes())+active_store->patterns[from].bytes()>PreservedStore::kMaxTotalBytes)active_store->dropped_unknown=true;else if(!active_store->patterns[dest].copy_from(active_store->patterns[from])){ok=false;break;}}saw_pattern[dest]=true;
    break;
   }
   case kChunkIden: {ChunkReader<ChunkSource> ir(section);if(saw_identity || flags || fresh->content_kind==ContentKind::Song || !read_preset_identity(ir,file_identity) || !file_identity.assigned() || !section.at_end()){ok=false;break;}saw_identity=true;break;}
   case kChunkPunk: {if(saw_punk || flags || fresh->content_kind==ContentKind::Song || !read_preserved_root(section,active_store,active_store?&active_store->tracks[0]:nullptr)){ok=false;break;}saw_punk=true;break;}
   case kChunkPfxs: {if(saw_pfxs || flags || fresh->content_kind==ContentKind::Song || !read_fx_frame(section,fresh->preset_fx)){ok=false;break;}saw_pfxs=true;break;}
   case kChunkPtrm: {ok=fresh->content_kind!=ContentKind::Song && read_output_trim_frame(section,file_trim);break;}
   case kChunkTwin: {ChunkReader<ChunkSource> tr(section);uint8_t count=0;uint16_t seen=0;if(fresh->content_kind!=ContentKind::Song || !tr.u8(count) || !count || count>kTracks){ok=false;break;}for(unsigned i=0;i<count && ok;++i){uint8_t t=0,active=0;uint16_t source=0,sampled=0;ok=tr.u8(t) && t<kTracks && !(seen&(1u<<t)) && tr.u16(source) && tr.u16(sampled) && source<kPresetSlots && sampled<kPresetSlots && source!=sampled && tr.u8(active) && active<=1;if(ok){seen|=uint16_t(1u<<t);auto& twin=fresh->twins[t];twin.source=source;twin.sampled=sampled;twin.active=active;ok=tr.block(twin.digest,32);}}ok=ok && section.at_end();break;}
   case kChunkTrks: if(saw_tracks){ok=false;break;}ok=parse_trks(section,*fresh,active_store);saw_tracks=ok;break;
   case kChunkAudio: ok=read_audio_section(section,*fresh);break;
   case kChunkProj: ok=read_proj_section(section,*fresh);break;
   case kChunkAsst: ok=read_asst_section(section,*fresh);break;
   case kChunkScen: ok=read_scen_section(section,*fresh);break;    case kChunkSong: ok=read_song_section(section,*fresh);break;
   case kChunkMacr: ok=read_macr_section(section,*fresh);break;
   case kChunkMfx: ok=read_mfx_section(section,*fresh);break;
   case kChunkPtrn: {unsigned index=section.size?section.data[0]:kPatterns;if(index>=kPatterns || saw_pattern[index]){ok=false;break;}ok=parse_ptrn(section,*fresh,active_store);saw_pattern[index]=ok;break;}
   case kChunkSnds: if(saw_sounds){ok=false;break;}ok=parse_snds(section,*fresh,active_store);saw_sounds=ok;break;
   default: ok=false;break;
  }
 }
 if(ok)ok=saw_type && saw_tracks && r.finish();
 if(ok && fresh->content_kind!=ContentKind::Song){ok=saw_identity && (fresh->content_kind!=ContentKind::Kit || fresh->tracks[0].instrument==Instrument::DrumKit);fresh->tracks[0].preset_identity=file_identity;fresh->tracks[0].output_trim=file_trim;fresh->tracks[0].preset_fx=fresh->preset_fx;fresh->preset_fx.apply(track_at(*fresh,0,0));}
 if(ok)ok=valid_project(*fresh) && valid_pinned_references(*fresh);
 if(ok && active_store)ok=finalize_definition_owners(*fresh,*active_store);
 if(ok){
  if(project)*project=*fresh;
  if(store)store->steal_from(temp_store);
 }
 if(!workspace)alloc_delete(fresh);
 return ok;
}
template<class Source> bool decode_project(Source& source,ProjectState& project,PreservedStore* store=nullptr){return decode_project_impl(source,&project,store);}
// Internal save check for the COMPLETE output of encode_project(snapshot).
// Encoding is over: its private snapshot can now be destroyed by validation.
// All serialized fields are read by the full parser, semantic checks and CRC;
// inactive, unencoded engine fields can retain their already-valid values.
// This is not an external-file loader: it must never publish this workspace
// or replace decode_project's fresh defaults and transactional destination.
template<class Source> bool validate_snapshot_encoding(Source& source,ProjectState& snapshot){return decode_project_impl(source,nullptr,nullptr,&snapshot);}
}
