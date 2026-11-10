#pragma once
#include "audio_lane.h"
#include "normal_copy.h"
#include "note_limits.h"
#include "chord_shapes.h"
#include "ui/theme.h"
#include <stdint.h>
#if defined(__linux__)
#include <new>
#else
#include <circle/new.h>
#endif
#include "clip_follow.h"
#include "parameters.h"
#include "parameter_target.h"
#include "creative_state.h"
#include "sample_state.h"
#include "preset_identity.h"
#include "preset_output.h"
#include "twin_state.h"
namespace groove {
inline bool valid_gesture(const GestureState& g){if(g.count>128 || g.ticks<96 || g.ticks>1536)return false;for(unsigned i=0;i<g.count;++i){const auto& e=g.points[i];if(e.tick>=g.ticks || e.track>=16 || e.pattern>=16 || unsigned(e.target)>2 || (i && e.tick<g.points[i-1].tick))return false;if(e.target==GestureTarget::Parameter){if(!valid_target(e.parameter,e.value))return false;}else if(!(e.value>=0 && e.value<=1) || (e.target==GestureTarget::Macro && e.parameter>=8))return false;}return true;}
// Shared, versioned edit/audio data model; runtime support is listed in docs/implementation-status.md.
struct SampleAssetState { char path[96]{}; SampleMetadata metadata{}; };
enum class TrigCondition : uint8_t { Always, Fill, NotFill, First, NotFirst, Cycle };
enum class PlaybackDirection : uint8_t { Forward, Reverse, PingPong };
struct ParameterLock { uint16_t id=0; float value=0; bool enabled=false; };
struct StepState {
 bool enabled=false,lock_only=false,chromatic=false,native_drum=false; uint8_t note=60, velocity=100, probability=100, ratchet=1;
 int16_t microtiming=0; uint16_t gate=96; uint8_t chord[kChordTones]={255,255,255,255,255,255,255}; uint8_t attributes=0; ParameterLock locks[kLocks]{};
 TrigCondition condition=TrigCondition::Always; uint8_t cycle_every=1,cycle_phase=0;uint16_t sound=kNoPreset;
 bool slide()const{return (attributes&1)!=0;}bool accent()const{return (attributes&2)!=0;}
 void slide(bool on){attributes=uint8_t(on?attributes|1:attributes&~1u);}void accent(bool on){attributes=uint8_t(on?attributes|2:attributes&~2u);}
};
inline bool same_step(const StepState& a,const StepState& b){if(a.attributes!=b.attributes || a.enabled!=b.enabled || a.lock_only!=b.lock_only || a.chromatic!=b.chromatic || a.native_drum!=b.native_drum || a.note!=b.note || a.velocity!=b.velocity || a.probability!=b.probability || a.ratchet!=b.ratchet || a.microtiming!=b.microtiming || a.gate!=b.gate || a.condition!=b.condition || a.cycle_every!=b.cycle_every || a.cycle_phase!=b.cycle_phase || a.sound!=b.sound)return false;for(unsigned i=0;i<kChordTones;++i)if(a.chord[i]!=b.chord[i])return false;for(unsigned i=0;i<kLocks;++i)if(a.locks[i].id!=b.locks[i].id || a.locks[i].value!=b.locks[i].value || a.locks[i].enabled!=b.locks[i].enabled)return false;return true;}
enum class AccumulatorMode : uint8_t { Wrap, Bounce, Clamp };
struct AccumulatorState {
 int8_t pitch=0,minimum=-12,maximum=12,velocity=0,gate=0;
 AccumulatorMode mode=AccumulatorMode::Wrap; uint8_t reset=0; bool scale_degrees=false;
};
struct GrooveState {
 uint8_t preset=0,amount=0; int8_t timing[16]{},velocity[16]{};
};
struct MutationState { uint32_t seed=0x5b4d3c21;uint8_t amount=0;bool frozen=false; };
// A successful grid trigger clocks all three lanes. Each cell may hold for
// several triggers; notes, velocity and gate need not have the same period.
struct TriggerLane {
 uint8_t length=4; PlaybackDirection direction=PlaybackDirection::Forward;
 uint8_t values[16]={60,64,67,72,60,64,67,72,60,64,67,72,60,64,67,72};
 uint8_t duration[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
};
inline TriggerLane flat_lane(uint8_t value){TriggerLane lane;for(auto& v:lane.values)v=value;return lane;}
struct NrpnSlot {uint16_t number=0,value=0;bool enabled=false,registered=false;};
struct SoundState:SoundLeaf {LayerState layer{};PresetIdentity identity{};PresetFxSnapshot fx{};float output_trim=1;};
const SoundState& empty_sound();
#include "track_model.inc"
inline bool valid_preset_name(const char (&name)[96]){for(unsigned i=0;i<96;++i){unsigned c=uint8_t(name[i]);if(!c)return true;if(c<32 || c>126)return false;}return false;}
template<class T> inline const char* track_display_name(const T& t,TrackKind kind=TrackKind::Instrument){if(t.track_name[0])return t.track_name;if(kind!=TrackKind::Instrument)return kind==TrackKind::Looper?"Looper":"Audio";if(t.preset_name[0])return t.preset_name;const auto* d=instrument_descriptor(unsigned(t.instrument));return d?d->name:"Unknown";}


struct ClipState { uint8_t pattern=0; bool playing=true; };
enum class FollowMode : uint8_t { Sequential, Random, NoRepeat };
enum class FollowCondition : uint8_t { Always, Fill, NotFill };
struct SceneFollow { uint8_t bars=0,probability=255,weight=1; FollowCondition condition=FollowCondition::Always; };
struct SceneChain { uint8_t scenes[16]{},length=0,bars=1; bool enabled=false; FollowMode mode=FollowMode::Sequential; uint8_t probability=100; SceneFollow follow[16]{}; };
inline void reset_scene_chain(SceneChain& c){for(auto& scene:c.scenes)scene=0;c.length=0;c.bars=1;c.enabled=false;c.mode=FollowMode::Sequential;c.probability=100;for(auto& rule:c.follow){rule.bars=0;rule.probability=255;rule.weight=1;rule.condition=FollowCondition::Always;}}
inline bool valid_scene_chain(const SceneChain& c){if(c.length>16 || !c.bars || c.bars>16 || unsigned(c.mode)>2 || c.probability>100)return false;for(auto scene:c.scenes)if(scene>=16)return false;for(const auto& f:c.follow)if(f.bars>16 || (f.probability>100 && f.probability!=255) || f.weight>100 || unsigned(f.condition)>2)return false;return true;}
constexpr uint32_t kSongTicksPerBeat=960,kSongTicksPerBar=4*kSongTicksPerBeat,kSongMaxTicks=1024*kSongTicksPerBar;
struct SongSection {
 uint8_t scene=0;uint16_t bars=1;int8_t transpose=0;uint16_t mute_mask=0;
 // ticks == 0: editable Scene section; otherwise a captured clip snapshot.
 uint32_t ticks=0;uint16_t playing_mask=0xffff,restart_mask=0xffff;uint8_t patterns[kTracks]{};
};
struct SongState {SongSection sections[32]{};uint8_t length=0;bool enabled=false,loop=true;};
inline uint32_t song_section_ticks(const SongSection& s){return s.ticks?s.ticks:uint32_t(s.bars)*kSongTicksPerBar;}
inline bool song_has_snapshots(const SongState& s){for(unsigned i=0;i<s.length;++i)if(s.sections[i].ticks)return true;return false;}
inline bool valid_song_section(const SongSection& s){if(s.scene>=kScenes || s.transpose< -24 || s.transpose>24)return false;if(s.ticks){if(s.ticks>kSongMaxTicks)return false;for(auto p:s.patterns)if(p>=kPatterns)return false;return true;}return s.bars>=1 && s.bars<=1024;}
inline bool valid_song(const SongState& s){if(s.length>32)return false;for(unsigned i=0;i<s.length;++i)if(!valid_song_section(s.sections[i]))return false;return true;}
struct MasterEffects { float live_fx_type=0,live_fx_amount=0.5f; float delay_mix=0,delay_feedback=0.35f; uint16_t delay_ms=250; float reverb_mix=0,reverb_decay=0.5f,reverb_damp=0.5f;float compressor[7]={0.5f,4,10,120,0,0,0}; };
struct ProjectState;
struct PhrasePattern {PhraseState tracks[kTracks]{};uint8_t swing=0;uint16_t mute_mask=0;};
// Edit/import transaction: settings are copied only when explicitly requested.
struct PresetRegistration {uint16_t slot=kNoPreset;SoundState sound{};};
struct AssetClone {uint8_t from=255,to=255;SampleAssetState asset{};};
struct AssetClonePlan {uint8_t count=0;AssetClone rows[kSampleAssets]{};};
struct TrackInstall {PresetRegistration preset{};bool lane_change=false;TrackKind lane_kind=TrackKind::Instrument;AssetClonePlan clones{};bool annotations=false;void clear(){preset.slot=kNoPreset;lane_change=false;clones.count=0;annotations=false;}};
struct PatternState {uint16_t name_mask=0;char track_names[kTracks][96]{};TrackInstall install{};TrackState tracks[kTracks]{};uint8_t swing=0;uint16_t mute_mask=0;bool include_settings=false,groove_only=false;TrackMorphState morph[kTracks]{};bool include_morph=false;
 PatternState()=default;PatternState(const ProjectState&,unsigned);
};
struct SceneState { uint8_t pattern_for_track[kTracks]{}; uint16_t mute_mask=0; };
enum class ContentKind:uint8_t {Song=1,Kit=2,Preset=3};

struct ProjectState {TwinState twins[kTracks]{};UnavailableTargetOwners unavailable_targets{};ContentKind content_kind=ContentKind::Song;PresetFxSnapshot preset_fx{};AudioLane audio[kTracks]{};uint16_t muted_tracks=0,solo_tracks=0;uint8_t midi_input_route[16]={15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15};RoutingState routing{};ProjectUi ui{};GestureState gesture{};float tuning[12]{};bool midi_clock_slave=false;uint16_t tempo=120; TrackSettings tracks[kTracks]{};PhrasePattern patterns[kPatterns]{}; SceneState scenes[kScenes]{}; uint8_t active_pattern=0, active_scene=0;
 MacroState macros[8]{};SampleAssetState assets[kSampleAssets]{}; uint32_t random_seed=0x13579bdf; ClipState clips[kTracks]{}; SoundState sounds[kPresetSlots]{}; SceneChain chain{}; SongState song{}; MasterEffects effects{};
 ParameterSnapshot morph_a[kTracks]{},morph_b[kTracks]{}; bool morph_enabled=false; float morph_position=0;
 // Stage 4: four morph sets. morph_a/morph_b always hold the ACTIVE
 // set; morph_extra_* hold the other three (ascending set number,
 // skipping the active one). morph_swap_sets rotates them.
 uint8_t morph_set=0; ParameterSnapshot morph_extra_a[3][kTracks]{},morph_extra_b[3][kTracks]{};
 };
inline bool twin_material(const ProjectState& p,unsigned asset){for(const auto& twin:p.twins)if(twin.prepared() && p.sounds[twin.sampled].sample_slot==asset)return true;return false;}
inline float parameter_base(const ProjectState&,ConstTrackView,uint16_t);
inline bool morph_project_parameter(uint16_t);
inline void capture_track_morph(TrackMorphState&,const ProjectState&,unsigned);
inline void restore_track_morph(ProjectState&,unsigned,const TrackMorphState&);
inline void reset_track_morph(ProjectState&,unsigned);
inline void capture_parameter_snapshot(ParameterSnapshot&,const ProjectState&,unsigned);
inline float morph_endpoint(const ParameterSnapshot&,ConstTrackView,uint16_t);
inline void set_morph_endpoint(ParameterSnapshot&,ConstTrackView,uint16_t,float);
inline TrackView track_at(ProjectState& p,unsigned pattern,unsigned t){return {p.tracks[t],p.patterns[pattern].tracks[t]};}
inline ConstTrackView track_at(const ProjectState& p,unsigned pattern,unsigned t){return {p.tracks[t],p.patterns[pattern].tracks[t]};}
inline void snapshot_pattern(PatternState& out,const ProjectState& p,unsigned pattern){out.install.clear();out.name_mask=0;out.swing=p.patterns[pattern].swing;out.mute_mask=p.patterns[pattern].mute_mask;out.include_settings=false;for(unsigned t=0;t<kTracks;++t)out.tracks[t]=track_at(p,pattern,t);}
inline bool same_track_snapshot(ConstTrackView a,ConstTrackView b){
#define SAME_FIELD(T,N,...) if(__builtin_memcmp(&a.N,&b.N,sizeof(a.N)))return false;
 TRACK_ALL_FIELDS(SAME_FIELD)
#undef SAME_FIELD
 // KitDefinition is a base subobject in detached snapshots. Its tail padding
 // can overlap KitSequence; comparing sizeof(KitDefinition) reads phrase bytes.
 if(a.kit.solo_mask!=b.kit.solo_mask)return false;
 for(unsigned i=0;i<16;++i){const auto& x=a.kit.pads[i];const auto& y=b.kit.pads[i];if(!same_sample_edit(x.edit,y.edit) || x.gm_note!=y.gm_note || x.sample[0]!=y.sample[0] || x.sample[1]!=y.sample[1] || x.velocity_split!=y.velocity_split || x.layer_mode!=y.layer_mode || x.muted!=y.muted || x.reverse!=y.reverse || x.level!=y.level || x.pan!=y.pan || x.filter!=y.filter || x.delay_send!=y.delay_send || x.reverb_send!=y.reverb_send)return false;for(unsigned j=0;j<4;++j)if(x.insert_fx[j]!=y.insert_fx[j])return false;}
 return !__builtin_memcmp(&a.kit.sequence(),&b.kit.sequence(),sizeof(KitSequence));
}
struct TrackRange {ProjectState& project;unsigned pattern;TrackView operator[](unsigned t)const{return track_at(project,pattern,t);}};
inline PatternState::PatternState(const ProjectState& p,unsigned pattern){include_morph=true;for(unsigned t=0;t<kTracks;++t)capture_track_morph(morph[t],p,t);swing=p.patterns[pattern].swing;mute_mask=p.patterns[pattern].mute_mask;for(unsigned t=0;t<kTracks;++t)tracks[t]=track_at(p,pattern,t);}
class TrackViews {
 alignas(TrackView) unsigned char storage_[sizeof(TrackView)*kPatterns*kTracks];
public:
 explicit TrackViews(ProjectState& p){bind(p);}
 void bind(ProjectState& p){for(unsigned pi=0;pi<kPatterns;++pi)for(unsigned t=0;t<kTracks;++t)new(storage_+sizeof(TrackView)*(pi*kTracks+t)) TrackView(p.tracks[t],p.patterns[pi].tracks[t]);}
 TrackView& at(unsigned p,unsigned t){return *reinterpret_cast<TrackView*>(storage_+sizeof(TrackView)*(p*kTracks+t));}
 const TrackView& at(unsigned p,unsigned t)const{return *reinterpret_cast<const TrackView*>(storage_+sizeof(TrackView)*(p*kTracks+t));}
};
inline void snapshot_leaf(SoundLeaf& s,ConstTrackView t){
 s.definition_cookie=t.definition_cookie;copy_track_field(s.preset_name,t.preset_name);s.instrument=t.instrument;s.model=t.plaits_model;copy_track_field(s.parameters,t.engine_parameters);copy_track_field(s.daisy_adsr,t.daisy_adsr);
 s.additive=t.additive;s.phase_dist=t.phase_dist;s.formant=t.formant;s.granular=t.granular;s.soundfont=t.soundfont;s.fm6=t.fm6;s.virtual_analog=t.virtual_analog;s.wavetable=t.wavetable;s.sample_edit=t.sample_edit;s.sample_slot=t.sample_slot;s.sample_slice=t.sample_slice;s.sample_slices=t.sample_slices;s.sample_loop=t.sample_loop;s.sample_reverse=t.sample_reverse;s.creative=t.creative;s.texture=t.texture;s.texture.values[12]=1;s.resonator=t.resonator;s.kit=t.kit.definition();
 s.midi_program=128;for(auto& cc:s.midi_cc)cc=0;for(auto& used:s.midi_cc_used)used=0;
 if(t.instrument==Instrument::ExternalMidi){s.midi_program=t.midi_program;copy_track_field(s.midi_cc,t.midi_cc);copy_track_field(s.midi_cc_used,t.midi_cc_used);}
}
inline void snapshot_sound(SoundState& s,ConstTrackView t){snapshot_leaf(s,t);s.layer=t.layer;s.identity=t.preset_identity;s.output_trim=t.output_trim;s.fx=t.preset_fx;if(s.fx.present)s.fx.capture(t);}
inline SoundState snapshot_sound(ConstTrackView t){SoundState s;snapshot_sound(s,t);return s;}
inline bool valid_leaf(const SoundLeaf& s){
 if(!valid_preset_name(s.preset_name) || unsigned(s.instrument)>unsigned(Instrument::Unknown) || s.model>=24 || !valid_sample_edit(s.sample_edit) || !valid_creative(s.creative) || !valid_additive(s.additive) || !valid_phase_dist(s.phase_dist) || !valid_formant(s.formant) || !valid_granular(s.granular) || !valid_soundfont(s.soundfont) || !valid_fm6(s.fm6) || !valid_virtual_analog(s.virtual_analog) || !valid_wavetable(s.wavetable) || !valid_texture(s.texture) || !valid_resonator(s.resonator) || s.sample_slot>=kSampleAssets || !s.sample_slices || s.sample_slices>64 || s.sample_slice>=s.sample_slices || s.midi_program>128)return false;
 for(auto value:s.parameters)if(!(value>=0 && value<=1))return false;
 for(unsigned i=0;i<4;++i)if(!valid_parameter(uint16_t(104+i),s.daisy_adsr[i]))return false;
 for(auto cc:s.midi_cc)if(cc>127)return false;
 return valid_kit_definition(s.kit);
}
inline bool valid_layer(const LayerState& s){if(!valid_layer_shape(s))return false;for(unsigned i=0;i<s.count;++i)if(!valid_leaf(s.parts[i].sound))return false;return true;}
inline bool valid_sound(const SoundState& s){return valid_leaf(s) && valid_layer(s.layer) && s.identity.valid() && s.fx.valid() && valid_output_trim(s.output_trim);}
// Mount copies sound values; edits to the pool never mutate an attached track.
// One shared mount body: avoid per-translation-unit IPA clones of this large copy.
__attribute__((noinline,noclone)) inline void mount_sound(const TrackView& t,const SoundState& s){
 t.definition_cookie=s.definition_cookie;copy_normal_record(t.layer,s.layer);t.preset_identity=s.identity;t.output_trim=s.output_trim;t.preset_fx=s.fx;s.fx.apply(t);
 copy_track_field(t.preset_name,s.preset_name);t.instrument=s.instrument;t.plaits_model=s.model;for(unsigned i=0;i<5;++i)t.engine_parameters[i]=s.parameters[i];
 t.additive=s.additive;t.phase_dist=s.phase_dist;t.formant=s.formant;t.granular=s.granular;t.soundfont=s.soundfont;t.fm6=s.fm6;t.virtual_analog=s.virtual_analog;t.wavetable=s.wavetable;t.sample_edit=s.sample_edit;t.sample_slot=s.sample_slot;t.sample_slice=s.sample_slice;t.sample_slices=s.sample_slices;t.sample_loop=s.sample_loop;t.sample_reverse=s.sample_reverse;
 copy_normal_record(t.kit.definition(),s.kit);copy_track_field(t.daisy_adsr,s.daisy_adsr);if(s.instrument==Instrument::ExternalMidi){t.midi_program=s.midi_program;copy_track_field(t.midi_cc,s.midi_cc);copy_track_field(t.midi_cc_used,s.midi_cc_used);}
 t.creative=s.creative;t.resonator=s.resonator;float input=t.texture.values[12];t.texture=s.texture;t.texture.values[12]=input;
}
// Preset attachment copies engine state, root output trim and optional FX/MIDI snapshots.
// The signal chain, MIDI channel, root note and phrase remain on their owners.
// One common body for UI/file mounts; avoid per-caller field-copy clones.
__attribute__((noinline,noclone)) inline void copy_engine_instance(const TrackView& target,const ConstTrackView& source){
 target.preset_identity=source.preset_identity;target.preset_slot=source.preset_slot;target.output_trim=source.output_trim;target.preset_fx=source.preset_fx;source.preset_fx.apply(target);
 const float input=target.texture.values[12];
#define COPY_INSTANCE(T,N,...) copy_track_field(target.N,source.N);
 ENGINE_INSTANCE_FIELDS(COPY_INSTANCE)
#undef COPY_INSTANCE
 target.texture.values[12]=input;target.kit.definition()=source.kit.definition();
 if(source.instrument==Instrument::ExternalMidi){target.midi_program=source.midi_program;copy_track_field(target.midi_cc,source.midi_cc);copy_track_field(target.midi_cc_used,source.midi_cc_used);}
}
inline unsigned song_scene_bars(const ProjectState& p,unsigned scene){unsigned bars=1;if(scene>=kScenes)return bars;for(unsigned t=0;t<kTracks;++t){unsigned len=p.patterns[p.scenes[scene].pattern_for_track[t]].tracks[t].length;unsigned b=(len+15)/16;if(b>bars)bars=b;}return bars;}
 inline unsigned song_section_bars(const ProjectState& p,const SongSection& s){(void)p;return s.bars;}
inline bool morph_snapshots_equal(const ProjectState& p){
 for(unsigned t=0;t<kTracks;++t)for(unsigned i=0;i<kCommonMorphParameters;++i)
  if(morph_common_parameter(morph_parameter_at(i)) && p.morph_a[t].values[i]!=p.morph_b[t].values[i])return false;
 for(unsigned t=0;t<kTracks;++t){const auto& a=p.morph_a[t];const auto& b=p.morph_b[t];if(a.engine!=b.engine)return false;const auto* d=engine_descriptor(unsigned(a.engine));unsigned count=d?d->parameters.count:0;if(a.engine==Instrument::Unknown){if(a.unknown_count!=b.unknown_count || __builtin_strcmp(a.unknown_guid,b.unknown_guid))return false;count=a.unknown_count;}for(unsigned i=0;i<count;++i)if(a.engine_values[i]!=b.engine_values[i])return false;}
 return true;
}
inline void morph_swap_sets(ProjectState& p,unsigned n){
 if(n>3)return;
 // Step through adjacent actives: for neighbouring sets the load
 // and stash slots coincide, so a plain exchange is exact; longer
 // jumps would need a rotation of the extras segment.
 while(p.morph_set!=n){
  unsigned cur=p.morph_set;
  unsigned slot=cur<n?cur:cur-1;
  for(unsigned tr=0;tr<kTracks;++tr){
   ParameterSnapshot tmp=p.morph_a[tr];p.morph_a[tr]=p.morph_extra_a[slot][tr];p.morph_extra_a[slot][tr]=tmp;
   tmp=p.morph_b[tr];p.morph_b[tr]=p.morph_extra_b[slot][tr];p.morph_extra_b[slot][tr]=tmp;
  }
  p.morph_set=uint8_t(cur<n?cur+1:cur-1);
 }
}
// Mixer state belongs to a logical track, independently of all patterns.
// Stage 4 two-layer mute: the global layer (muted_tracks) applies
// everywhere; the pattern layer lives on each PatternState and
// applies while the track plays that pattern. track_muted is the
// union the engine and displays gate on.
inline bool track_muted_global(const ProjectState& p,unsigned t){return t<kTracks && (p.muted_tracks&(uint16_t(1u)<<t));}
inline bool track_muted_pattern(const ProjectState& p,unsigned t){return t<kTracks && (p.patterns[p.clips[t].pattern].mute_mask&(uint16_t(1u)<<t));}
inline bool track_muted(const ProjectState& p,unsigned t){return track_muted_global(p,t) || track_muted_pattern(p,t);}
inline bool track_solo(const ProjectState& p,unsigned t){return t<kTracks && (p.solo_tracks&(uint16_t(1u)<<t));}
inline void set_track_mixer_flag(uint16_t& mask,unsigned t,bool value){if(t<kTracks){uint16_t bit=uint16_t(1u<<t);mask=value?uint16_t(mask|bit):uint16_t(mask&~bit);}}
inline uint16_t preset_slot_for(const ProjectState& p,const PresetIdentity& identity){if(identity.assigned())for(unsigned i=0;i<kPresetSlots;++i)if(same_preset_revision(p.sounds[i].identity,identity))return uint16_t(i);return kNoPreset;}
// A pinned definition remains alive while any independent track/phrase uses it.
// Undoing the import on one track must not invalidate a later MIDI attachment.
inline bool preset_slot_referenced(const ProjectState& p,unsigned slot,uint16_t except_tracks=0){
 for(const auto& twin:p.twins)if(twin.prepared() && (twin.source==slot || twin.sampled==slot))return true;
 for(unsigned t=0;t<kTracks;++t)if(!(except_tracks&(1u<<t)) && p.tracks[t].preset_slot==slot && p.tracks[t].preset_identity.assigned())return true;
 for(const auto& pattern:p.patterns)for(const auto& track:pattern.tracks){if(track.sound_override==slot)return true;for(const auto& step:track.steps)if(step.sound==slot)return true;if(track.mutation.frozen)for(const auto& step:track.mutation_source)if(step.sound==slot)return true;}
 return false;
}
// Saving an edited ACTIVE sample may allocate a new revision slot. Follow that
// registration, retaining the previous slot only if another owner references it.
inline void twin_registered(ProjectState& p,unsigned t,unsigned slot){
 auto& twin=p.twins[t];if(!twin.prepared() || !twin.active || slot==twin.source || slot>=kPresetSlots || p.sounds[slot].instrument!=Instrument::Sampler)return;
 unsigned old=twin.sampled;twin.sampled=uint16_t(slot);
 if(old!=slot && !preset_slot_referenced(p,old)){auto& sound=p.sounds[old];sound.instrument=Instrument::Empty;sound.identity={};sound.definition_cookie=0;sound.preset_name[0]=0;}
}
inline void install_pool_definition(ProjectState& p,unsigned slot,const SoundState& sound,uint16_t except_tracks=0){
 if(slot>=kPresetSlots)return;
 if((sound.instrument==Instrument::Empty || (p.sounds[slot].identity.assigned() && !same_preset_revision(p.sounds[slot].identity,sound.identity))) && preset_slot_referenced(p,slot,except_tracks))return;
 p.sounds[slot]=sound;
}
inline void install_track_resources(ProjectState& p,const TrackInstall& install,uint16_t mask){
 install_pool_definition(p,install.preset.slot,install.preset.sound,mask);
 if(install.lane_change)for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t))p.audio[t].kind=install.lane_kind;
 for(unsigned i=0;i<install.clones.count;++i)p.assets[install.clones.rows[i].to]=install.clones.rows[i].asset;
}
inline void install_track_phrase(TrackView dst,const TrackState& src){
#define COPY_PHRASE(T,N,...) copy_track_field(dst.N,src.N);
 PHRASE_FIELDS(COPY_PHRASE)
#undef COPY_PHRASE
 for(unsigned pad=0;pad<16;++pad)for(unsigned step=0;step<kSteps;++step)dst.kit.steps[pad][step]=src.kit.steps[pad][step];
}
inline void install_pattern(ProjectState& p,unsigned index,const PatternState& source,uint16_t mask=0xffff){
 if(source.groove_only){for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t))p.tracks[t].groove=source.tracks[t].groove;return;}
 for(unsigned t=0;t<kTracks;++t)if((mask&source.name_mask)&(1u<<t))copy_track_field(p.tracks[t].track_name,source.track_names[t]);
 install_track_resources(p,source.install,mask);
 for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t)){
 auto dst=track_at(p,index,t);const auto& src=source.tracks[t];const auto old_engine=dst.instrument;
install_track_phrase(dst,src);
 if(source.include_settings){
#define COPY_SETTINGS(T,N,...) copy_track_field(dst.N,src.N);
 ENGINE_INSTANCE_FIELDS(COPY_SETTINGS) TRACK_SETTINGS_FIELDS(COPY_SETTINGS)
#undef COPY_SETTINGS
 dst.kit.definition()=src.kit.definition();
 if(source.include_morph && source.morph[t].instrument==dst.instrument)restore_track_morph(p,t,source.morph[t]);
 else if(old_engine!=dst.instrument)reset_track_morph(p,t);
 }
 }
 if(mask==0xffff)p.patterns[index].mute_mask=source.mute_mask;
 if(mask==0xffff)p.patterns[index].swing=source.swing;
}
// Validate at edit/load boundary; values remain untouched on failure.
inline bool valid_step_sample_lock(uint16_t id,float value){return ((id==300 || id==301) && value>=0 && value<=63 && value==float(uint8_t(value))) || (id==302 && (value==0 || value==1));}
inline bool valid_step(const StepState& s,const UnavailableTargetOwners* owners=nullptr) {if(s.attributes>3)return false; if(unsigned(s.condition)>unsigned(TrigCondition::Cycle) || !s.cycle_every || s.cycle_phase>=s.cycle_every || (s.sound!=kNoPreset && s.sound>=kPresetSlots)) return false; if(!(s.note<128 && s.velocity<128 && s.probability<=100 && s.ratchet>=1 && s.ratchet<=8 && s.microtiming>=-192 && s.microtiming<=192 && s.gate>0 && s.gate<=384)) return false; for(uint8_t note:s.chord)if(note>127 && note!=255)return false; for(const auto& lock:s.locks) if((owners && !target_owner_present(lock.id,*owners)) || !(lock.value>=-1000000 && lock.value<=1000000) || (lock.enabled && !valid_target(lock.id,lock.value) && !valid_step_sample_lock(lock.id,lock.value))) return false; return true; }
inline bool valid_groove(const GrooveState& g){
 if(g.preset>4 || g.amount>100)return false;
 for(unsigned i=0;i<16;++i)if(g.timing[i]< -96 || g.timing[i]>96 || g.velocity[i]< -64 || g.velocity[i]>64)return false;
 return true;
}
inline bool valid_advanced_track(ConstTrackView  t) {
 const auto& a=t.accumulator;
 if(t.pitch_bend>16383 || t.pressure>127 || t.expression_used>3 || t.midi_program>128)return false;
 if(a.pitch< -24 || a.pitch>24 || a.minimum< -48 || a.minimum>0 || a.maximum<0 || a.maximum>48 || unsigned(a.mode)>2 || a.reset>64 || a.velocity< -32 || a.velocity>32 || a.gate< -48 || a.gate>48)return false;
 for(const auto& slot:t.nrpn){if(slot.number>16383 || slot.value>16383 || (slot.enabled && slot.registered && slot.number==16383))return false;} if(!valid_groove(t.groove)){return false;}for(const auto& lane:t.automation)if(!valid_automation(lane))return false;for(auto cc:t.midi_cc)if(cc>127)return false;
 for(unsigned l=0;l<3;++l){const auto& lane=t.lanes[l];if(!lane.length || lane.length>16 || unsigned(lane.direction)>2)return false;for(unsigned i=0;i<16;++i)if(lane.values[i]>127 || (l==2 && !lane.values[i]) || !lane.duration[i] || lane.duration[i]>16)return false;}
 return true;
}
inline bool valid_track(ConstTrackView t,const UnavailableTargetOwners* owners=nullptr) {if(!valid_parameter(11,t.lfo_sync) || !valid_parameter(12,t.lfo_division) || floorf(t.lfo_sync)!=t.lfo_sync || floorf(t.lfo_division)!=t.lfo_division)return false;if(owners)for(const auto& lane:t.automation)if(!target_owner_present(lane.parameter,*owners))return false; if(!valid_preset_name(t.track_name) || !t.preset_identity.valid() || !t.preset_fx.valid() || !valid_output_trim(t.output_trim) || (t.preset_slot!=kNoPreset && t.preset_slot>=kPresetSlots) || !valid_preset_name(t.preset_name) || !valid_layer(t.layer))return false; if(!valid_additive(t.additive) || !valid_phase_dist(t.phase_dist) || !valid_formant(t.formant) || !valid_granular(t.granular) || !valid_soundfont(t.soundfont) || !valid_fm6(t.fm6) || !valid_virtual_analog(t.virtual_analog) || !valid_wavetable(t.wavetable) || !valid_texture(t.texture) || !valid_resonator(t.resonator))return false; if(!valid_clip_follow(t.clip_follow) || (t.sound_override!=65535 && t.sound_override>=kPresetSlots))return false; for(unsigned i=0;i<8;++i)if(!valid_parameter(uint16_t(94+i),t.insert_fx[i]) || (i%4==0 && floorf(t.insert_fx[i])!=t.insert_fx[i]))return false;for(unsigned i=0;i<7;++i)if(!valid_parameter(uint16_t(87+i),t.compressor[i]))return false; if(t.arp_mode>4 || t.arp_rate>3 || t.arp_octaves<1 || t.arp_octaves>4 || !t.mutation.seed || t.mutation.amount>100 || !valid_parameter(10,t.reverb_send) || t.output_bus>3 || t.send_prefader>3 || t.insert_disabled>3 || !valid_sample_edit(t.sample_edit) || !valid_creative(t.creative) || !valid_advanced_track(t))return false; if(t.length<1 || t.length>kSteps || t.midi_channel>15 || t.root_note>127 || t.plaits_model>=24 || !(t.level>=0 && t.level<=1) || !(t.pan>=-1 && t.pan<=1) || unsigned(t.instrument)>unsigned(Instrument::Unknown)) return false; for(unsigned i=0;i<4;++i){if(!valid_parameter(uint16_t(104+i),t.daisy_adsr[i]))return false;} if(t.sample_slot>=kSampleAssets || (t.frozen_asset!=255 && t.frozen_asset>=kSampleAssets))return false; if(t.transpose< -48 || t.transpose>48 || !t.scale_mask || t.scale_mask>0x0fff || t.scale_root>11 || !(t.velocity_scale>=0 && t.velocity_scale<=2)) return false; if(unsigned(t.direction)>2 || !t.speed_num || t.speed_num>8 || !t.speed_den || t.speed_den>8 || !t.sample_slices || t.sample_slices>64 || t.sample_slice>=t.sample_slices) return false; const float extra[]={t.cutoff,t.drive,t.delay_send,t.crusher,t.lfo_depth,t.lfo_rate,t.aftertouch_depth}; for(unsigned i=0;i<7;++i) if(!valid_parameter(uint16_t(i+3),extra[i])) return false; for(float value:t.engine_parameters) if(!(value>=0 && value<=1)) return false; for(const auto& s:t.steps) if(!valid_step(s,owners)) return false;if(t.instrument==Instrument::DrumKit && !valid_drum_kit(t.kit))return false;if(t.mutation.frozen)for(const auto& s:t.mutation_source)if(!valid_step(s,owners))return false; return true; }
inline bool valid_project(const ProjectState& p) {
 for(unsigned t=0;t<kTracks;++t){const auto& twin=p.twins[t];if(!twin.prepared()){if(twin.source!=kNoPreset || twin.sampled!=kNoPreset || twin.active)return false;}else{if(twin.active!=(p.tracks[t].instrument==Instrument::Sampler && p.tracks[t].preset_slot==twin.sampled))return false;if(p.content_kind!=ContentKind::Song || p.sounds[twin.source].instrument==Instrument::Empty || p.sounds[twin.sampled].instrument!=Instrument::Sampler || !p.sounds[twin.sampled].sample_edit.zone_count)return false;for(unsigned j=0;j<t;++j)if(p.twins[j].prepared() && (twin.source==p.twins[j].source || twin.source==p.twins[j].sampled || twin.sampled==p.twins[j].source || twin.sampled==p.twins[j].sampled))return false;}}

 if(!p.preset_fx.valid() || (p.preset_fx.present && p.content_kind==ContentKind::Song))return false;
 if(!p.unavailable_targets.valid())return false;
 for(auto octave:p.ui.keyboard_octave)if(octave< -4 || octave>4)return false;
 auto owned=[&](ParameterTarget id){return target_owner_present(id,p.unavailable_targets);};
 for(unsigned i=0;i<p.gesture.count && i<128;++i)if(!owned(p.gesture.points[i].parameter))return false;
 for(const auto& macro:p.macros)for(const auto& route:macro.routes)if(!owned(route.parameter))return false;

 if(unsigned(p.content_kind)<1 || unsigned(p.content_kind)>3 || !valid_audio_lanes(p.audio))return false;
 for(uint8_t route:p.midi_input_route)if(route>16)return false;
 if(!valid_routing(p.routing) || p.ui.motion>2 || p.ui.order>1 || p.ui.strip>2 || p.ui.arp_direction>3 || p.ui.arp_octaves<1 || p.ui.arp_octaves>4 || p.ui.repeat_gate<10 || p.ui.repeat_gate>100 || p.ui.repeat_rate>7 || p.ui.chord_shape>=kChordShapes || p.ui.chord_inversion>3 || !valid_gesture(p.gesture)){return false;}for(float cents:p.tuning)if(!(cents>=-100 && cents<=100))return false;for(const auto& s:p.sounds)if(!valid_sound(s))return false;
 for(const auto& macro:p.macros){if(!(macro.value>=0 && macro.value<=1))return false;for(const auto& route:macro.routes)if(!route.tracks || unsigned(route.curve)>unsigned(MacroCurve::Smooth) || unsigned(route.source)>unsigned(ModSource::Velocity) || !valid_target_layer(route.parameter,route.mode,route.low) || !valid_target_layer(route.parameter,route.mode,route.high))return false;}
 if(p.tempo<20 || p.tempo>300 || p.active_pattern>=kPatterns || p.active_scene>=kScenes) return false;
 for(const auto& asset:p.assets){const auto& m=asset.metadata;if(m.root>=128 || m.channels>2 || m.bpm>300 || (m.bits && m.bits!=8 && m.bits!=16 && m.bits!=24 && m.bits!=32) || m.rate>768000)return false;bool terminated=false;for(char c:asset.path)if(!c)terminated=true;if(!terminated)return false;}
 if(!valid_scene_chain(p.chain) || !valid_song(p.song))return false;
 for(unsigned i=0;i<7;++i)if(!valid_parameter(uint16_t(70+i),p.effects.compressor[i]))return false;
 if(!valid_parameter(67,p.effects.reverb_mix) || !valid_parameter(68,p.effects.reverb_decay) || !valid_parameter(69,p.effects.reverb_damp))return false;
 if(!valid_parameter(472,p.effects.live_fx_type) || !valid_parameter(473,p.effects.live_fx_amount) || !p.random_seed || !(p.effects.delay_mix>=0 && p.effects.delay_mix<=1) || !(p.effects.delay_feedback>=0 && p.effects.delay_feedback<=0.95f) || p.effects.delay_ms<1 || p.effects.delay_ms>1000 || !(p.morph_position>=0 && p.morph_position<=1) || p.morph_set>3) return false;
 for(const auto& clip:p.clips) if(clip.pattern>=kPatterns) return false;
 for(const auto& sound:p.sounds) { if(unsigned(sound.instrument)>unsigned(Instrument::Unknown) || sound.model>=24) return false; for(auto v:sound.parameters) if(!(v>=0 && v<=1)) return false; }
 for(unsigned t=0;t<kTracks;++t){if(!valid_engine_morph(p.morph_a[t]) || !valid_engine_morph(p.morph_b[t]))return false;for(unsigned n=0;n<3;++n)if(!valid_engine_morph(p.morph_extra_a[n][t]) || !valid_engine_morph(p.morph_extra_b[n][t]))return false;}
 for(unsigned t=0;t<kTracks;++t) for(unsigned i=0;i<kCommonMorphParameters;++i) if(morph_common_parameter(morph_parameter_at(i)) && (!valid_parameter(morph_parameter_at(i),p.morph_a[t].values[i]) || !valid_parameter(morph_parameter_at(i),p.morph_b[t].values[i]))) return false;
 for(unsigned s=0;s<3;++s) for(unsigned t=0;t<kTracks;++t) for(unsigned i=0;i<kCommonMorphParameters;++i) if(morph_common_parameter(morph_parameter_at(i)) && (!valid_parameter(morph_parameter_at(i),p.morph_extra_a[s][t].values[i]) || !valid_parameter(morph_parameter_at(i),p.morph_extra_b[s][t].values[i]))) return false;
 for(const auto& scene:p.scenes) for(uint8_t pattern:scene.pattern_for_track) if(pattern>=kPatterns) return false;
 for(unsigned pi=0;pi<kPatterns;++pi){if(p.patterns[pi].swing>100)return false;for(unsigned t=0;t<kTracks;++t)if(!valid_track(track_at(p,pi,t),&p.unavailable_targets))return false;}
 return true;
}
}

namespace groove {
inline float track_parameter(ConstTrackView  t,uint16_t id) {
 if(id>=620 && id<635)return t.texture.values[id-620];
 if(id>=600 && id<608)return t.resonator.values[id-600];
 if(id>=474 && id<554){const auto& p=t.kit.pads[(id-474)/5];switch((id-474)%5){case 0:return p.level;case 1:return p.pan;case 2:return p.filter;case 3:return p.delay_send;default:return p.reverb_send;}}
 if(id>=320 && id<384)return t.kit.pads[(id-320)/4].insert_fx[(id-320)%4];
 if(id==260)return t.pitch_bend;
 if(id==261)return t.pressure;
 if(id>=256 && id<260)return t.nrpn[id-256].value;
 if(id>=94 && id<=101)return t.insert_fx[id-94];
 if(id==102 || id==103)return (t.insert_disabled>>(id-102))&1?0.0f:1.0f;
 if(id>=104 && id<=107)return t.daisy_adsr[id-104];
 if(id>=87 && id<=93)return t.compressor[id-87];
 if(id>=80 && id<=86){const auto& e=t.sample_edit;const float values[]={e.speed,e.start,e.end,e.fade_ms,e.crossfade_ms,e.attack_ms,e.release_ms};return values[id-80];}
 switch(id) { case 1:return t.level; case 2:return t.pan; case 3:return t.cutoff; case 4:return t.drive; case 5:return t.delay_send; case 6:return t.crusher; case 7:return t.lfo_depth; case 8:return t.lfo_rate; case 11:return t.lfo_sync; case 12:return t.lfo_division; case 9:return t.aftertouch_depth; case 10:return t.reverb_send; default: if(id>=128 && id<256)return t.midi_cc[id-128];return id>=32 && id<=36?t.engine_parameters[id-32]:0; }
}
inline bool set_track_parameter(TrackView  t,uint16_t id,float value) {
 if(!valid_parameter(id,value)) return false;
 if(id>=620 && id<635){t.texture.values[id-620]=clamp_parameter(id,value);return true;}
 if(id>=600 && id<608){t.resonator.values[id-600]=clamp_parameter(id,value);return true;}
 if(id>=474 && id<554){auto& p=t.kit.pads[(id-474)/5];switch((id-474)%5){case 0:p.level=value;break;case 1:p.pan=value;break;case 2:p.filter=value;break;case 3:p.delay_send=value;break;default:p.reverb_send=value;break;}return true;}
 if(id>=320 && id<384){t.kit.pads[(id-320)/4].insert_fx[(id-320)%4]=clamp_parameter(id,value);return true;}
 if(id==260){t.pitch_bend=uint16_t(clamp_parameter(id,value));t.expression_used|=1;return true;}if(id==261){t.pressure=uint8_t(clamp_parameter(id,value));t.expression_used|=2;return true;}
 if(id>=256 && id<260){t.nrpn[id-256].value=uint16_t(clamp_parameter(id,value));return true;}
 if(id>=94 && id<=101){t.insert_fx[id-94]=clamp_parameter(id,value);return true;}
 if(id==102 || id==103){unsigned bit=1u<<(id-102);if(value>0.5f)t.insert_disabled=uint8_t(t.insert_disabled&~bit);else t.insert_disabled=uint8_t(t.insert_disabled|bit);return true;}  if(id>=104 && id<=107){t.daisy_adsr[id-104]=clamp_parameter(id,value);return true;}
 if(id>=87 && id<=93){t.compressor[id-87]=clamp_parameter(id,value);return true;}
 if(id>=80 && id<=86){auto& e=t.sample_edit;switch(id){case 80:e.speed=value;break;case 81:if(value>=e.end)return false;e.start=value;break;case 82:if(value<=e.start)return false;e.end=value;break;case 83:e.fade_ms=value;break;case 84:e.crossfade_ms=value;break;case 85:e.attack_ms=value;break;case 86:e.release_ms=value;break;}return true;}
 switch(id) { case 1:t.level=value;break; case 2:t.pan=value;break; case 3:t.cutoff=value;break; case 4:t.drive=value;break; case 5:t.delay_send=value;break; case 6:t.crusher=value;break; case 7:t.lfo_depth=value;break; case 8:t.lfo_rate=value;break; case 11:t.lfo_sync=clamp_parameter(id,value);break; case 12:t.lfo_division=clamp_parameter(id,value);break; case 9:t.aftertouch_depth=value;break; case 10:t.reverb_send=value;break; default:if(id>=32 && id<=36)t.engine_parameters[id-32]=value;else if(id>=128 && id<256){t.midi_cc[id-128]=uint8_t(value);t.midi_cc_used[(id-128)/8]|=uint8_t(1u<<((id-128)%8));}else return false;break; } return true;
}
inline float engine_parameter_value(ConstTrackView t,unsigned local){
 const auto* d=engine_parameter(t.instrument,local);if(!d)return 0;
 switch(d->target){
 case EngineValueTarget::Model:return t.plaits_model;
 case EngineValueTarget::Macro:return t.engine_parameters[d->offset];
 case EngineValueTarget::Sample:return t.sample_edit.*sample_parameter_member(d->offset);
 case EngineValueTarget::Envelope:return t.daisy_adsr[d->offset];
 case EngineValueTarget::Resonator:return t.resonator.values[d->offset];
 case EngineValueTarget::Layer:{const auto& part=t.layer.parts[d->offset/3];return d->offset%3==0?part.level:d->offset%3==1?part.pan:part.detune;}
 case EngineValueTarget::Additive:return t.additive.values[d->offset];
 case EngineValueTarget::Granular:return t.granular.values[d->offset];
 case EngineValueTarget::Formant:return t.formant.values[d->offset];
 case EngineValueTarget::PhaseDist:return t.phase_dist.values[d->offset];
 case EngineValueTarget::SoundFont:return t.soundfont.values[d->offset];
 case EngineValueTarget::Fm6:return t.fm6.values[d->offset];
 case EngineValueTarget::VirtualAnalog:return t.virtual_analog.values[d->offset];
 case EngineValueTarget::Wavetable:return t.wavetable.values[d->offset];
 case EngineValueTarget::Texture:return t.texture.values[d->offset];
 case EngineValueTarget::None:return 0;
 }return 0;
}
inline bool set_engine_parameter(TrackView t,unsigned local,float value){
 const auto* d=engine_parameter(t.instrument,local);if(!d || !(value>=d->value.minimum && value<=d->value.maximum) || (d->value.interpolation==Interpolation::Discrete && floorf(value)!=value))return false;
 switch(d->target){
 case EngineValueTarget::Model:t.plaits_model=uint8_t(value);break;
 case EngineValueTarget::Macro:t.engine_parameters[d->offset]=value;break;
 case EngineValueTarget::Sample:if((d->offset==1 && value>=t.sample_edit.end) || (d->offset==2 && value<=t.sample_edit.start))return false;t.sample_edit.*sample_parameter_member(d->offset)=value;break;
 case EngineValueTarget::Envelope:t.daisy_adsr[d->offset]=value;break;
 case EngineValueTarget::Resonator:t.resonator.values[d->offset]=value;break;
 case EngineValueTarget::Layer:{auto& part=t.layer.parts[d->offset/3];if(d->offset%3==0)part.level=value;else if(d->offset%3==1)part.pan=value;else part.detune=value;break;}
 case EngineValueTarget::Additive:t.additive.values[d->offset]=value;break;
 case EngineValueTarget::Granular:t.granular.values[d->offset]=value;break;
 case EngineValueTarget::Formant:t.formant.values[d->offset]=value;break;
 case EngineValueTarget::PhaseDist:t.phase_dist.values[d->offset]=value;break;
 case EngineValueTarget::SoundFont:t.soundfont.values[d->offset]=value;break;
 case EngineValueTarget::Fm6:t.fm6.values[d->offset]=value;break;
 case EngineValueTarget::VirtualAnalog:t.virtual_analog.values[d->offset]=value;break;
 case EngineValueTarget::Wavetable:t.wavetable.values[d->offset]=value;break;
 case EngineValueTarget::Texture:t.texture.values[d->offset]=value;break;
 case EngineValueTarget::None:return false;
 }return true;
}

inline void capture_engine_morph(ParameterSnapshot& s,ConstTrackView t){if(t.instrument==Instrument::Unknown)return;s.engine=t.instrument;s.unknown_guid[0]=0;s.unknown_count=0;for(unsigned i=0;i<kEngineMorphParameters;++i)s.engine_values[i]=engine_parameter_value(t,i);}
inline void capture_parameter_snapshot(ParameterSnapshot& s,const ProjectState& p,unsigned t){auto track=track_at(p,p.clips[t].pattern,t);for(unsigned i=0;i<kCommonMorphParameters;++i)if(morph_common_parameter(morph_parameter_at(i)))s.values[i]=parameter_base(p,track,morph_parameter_at(i));capture_engine_morph(s,track);}
inline void capture_track_morph(TrackMorphState& s,const ProjectState& p,unsigned t){s.instrument=p.tracks[t].instrument;s.a[0]=p.morph_a[t];s.b[0]=p.morph_b[t];for(unsigned n=0;n<3;++n){s.a[n+1]=p.morph_extra_a[n][t];s.b[n+1]=p.morph_extra_b[n][t];}}
inline void restore_track_morph(ProjectState& p,unsigned t,const TrackMorphState& state){
 auto restore=[](ParameterSnapshot& dst,const ParameterSnapshot& src){for(unsigned i=0;i<kCommonMorphParameters;++i)if(!morph_project_parameter(morph_parameter_at(i)))dst.values[i]=src.values[i];dst.engine=src.engine;copy_track_field(dst.engine_values,src.engine_values);copy_track_field(dst.unknown_guid,src.unknown_guid);dst.unknown_count=src.unknown_count;};
 restore(p.morph_a[t],state.a[0]);restore(p.morph_b[t],state.b[0]);for(unsigned n=0;n<3;++n){restore(p.morph_extra_a[n][t],state.a[n+1]);restore(p.morph_extra_b[n][t],state.b[n+1]);}
}
inline bool morph_project_parameter(uint16_t id){return (id>=64 && id<=76) || (id>=384 && id<474);}
inline void reset_track_morph(ProjectState& p,unsigned t){
 auto reset=[&](ParameterSnapshot& s){auto track=track_at(p,p.clips[t].pattern,t);for(unsigned i=0;i<kCommonMorphParameters;++i){auto id=morph_parameter_at(i);if(morph_common_parameter(id) && !morph_project_parameter(id))s.values[i]=parameter_base(p,track,id);}capture_engine_morph(s,track);};
 reset(p.morph_a[t]);reset(p.morph_b[t]);for(unsigned n=0;n<3;++n){reset(p.morph_extra_a[n][t]);reset(p.morph_extra_b[n][t]);}
}
inline float morph_endpoint(const ParameterSnapshot& s,ConstTrackView t,uint16_t id){int local=engine_local_parameter(t.instrument,id);if(local>=0)return s.engine==t.instrument?s.engine_values[local]:engine_parameter_value(t,unsigned(local));unsigned index=morph_index(id);return index<kCommonMorphParameters && morph_common_parameter(id)?s.values[index]:track_parameter(t,id);}
inline void set_morph_endpoint(ParameterSnapshot& s,ConstTrackView t,uint16_t id,float value){int local=engine_local_parameter(t.instrument,id);if(local>=0){if(s.engine!=t.instrument)capture_engine_morph(s,t);s.engine_values[local]=value;}else if(morph_index(id)<kCommonMorphParameters && morph_common_parameter(id))s.values[morph_index(id)]=value;}
inline bool edit_engine_parameter(ProjectState& p,TrackView t,unsigned track,unsigned local,float value,unsigned mode=0,float a=0,float b=0){
 const auto* d=engine_parameter(t.instrument,local);if(!d || local>=kEngineMorphParameters)return false;
 auto valid=[&](float v){return v>=d->value.minimum && v<=d->value.maximum && (d->value.interpolation!=Interpolation::Discrete || floorf(v)==v);};
 if(mode==1 && (!valid(a) || !valid(b)))return false;
 if(!set_engine_parameter(t,local,value))return false;
 if(mode==1 || (mode==0 && p.morph_enabled)){auto& sa=p.morph_a[track];auto& sb=p.morph_b[track];if(sa.engine!=t.instrument)capture_engine_morph(sa,t);if(sb.engine!=t.instrument)capture_engine_morph(sb,t);sa.engine_values[local]=mode==1?a:value;sb.engine_values[local]=mode==1?b:value;}
 return true;
}

}

namespace groove {
inline float parameter_base(const ProjectState& p,ConstTrackView  t,uint16_t id){if(engine_target(id)){const auto* d=target_descriptor(id);return target_engine(id)==t.instrument?engine_parameter_value(t,target_local(id)):d?d->initial:0;}return id==472?p.effects.live_fx_type:id==473?p.effects.live_fx_amount:id>=464 && id<472?p.routing.groups[id-464].parallel:id>=448 && id<464?p.routing.groups[(id-448)/2].send[(id-448)%2]:id>=384 && id<448?p.routing.groups[group_fx_group(id)].insert_fx[group_fx_field(id)]:id>=70 && id<=76?p.effects.compressor[id-70]:id==64?p.effects.delay_mix:id==65?p.effects.delay_feedback:id==66?float(p.effects.delay_ms):id==67?p.effects.reverb_mix:id==68?p.effects.reverb_decay:id==69?p.effects.reverb_damp:track_parameter(t,id);}
inline bool set_parameter_base(ProjectState& p,TrackView  t,uint16_t id,float value){if(!valid_parameter(id,value))return false;if(id==472)p.effects.live_fx_type=clamp_parameter(id,value);else if(id==473)p.effects.live_fx_amount=value;else if(id>=464 && id<472)p.routing.groups[id-464].parallel=value;else if(id>=448 && id<464)p.routing.groups[(id-448)/2].send[(id-448)%2]=value;else if(id>=384 && id<448)p.routing.groups[group_fx_group(id)].insert_fx[group_fx_field(id)]=clamp_parameter(id,value);else if(id>=70 && id<=76)p.effects.compressor[id-70]=clamp_parameter(id,value);else if(id==64)p.effects.delay_mix=value;else if(id==65)p.effects.delay_feedback=value;else if(id==66)p.effects.delay_ms=uint16_t(value);else if(id==67)p.effects.reverb_mix=value;else if(id==68)p.effects.reverb_decay=value;else if(id==69)p.effects.reverb_damp=value;else return set_track_parameter(t,id,value);return true;}
// Control/file-service data reset. Reconstruct in place: a SoundState is
// ~62 KiB, so a fresh automatic copy can overflow Circle's 128 KiB stack.
// Callers reset owned scratch states and reacquire borrowed views afterwards.
template<class T> __attribute__((noinline)) void reset_to_default(T& target){static_assert(__is_trivially_copyable(T),"project defaults must be data-only");target.~T();::new(static_cast<void*>(&target)) T();}
inline void make_empty_project(ProjectState& p){for(auto& twin:p.twins)twin=TwinState{};p.unavailable_targets.count=0;p.content_kind=ContentKind::Song;reset_to_default(p.song);p.morph_set=0;for(auto& a:p.audio)a=AudioLane{};p.muted_tracks=p.solo_tracks=0;for(auto& route:p.midi_input_route)route=15;reset_to_default(p.routing);reset_to_default(p.ui);reset_to_default(p.gesture);for(float& c:p.tuning)c=0;p.midi_clock_slave=false;p.tempo=120;p.active_pattern=p.active_scene=0;p.random_seed=0x13579bdf;p.morph_enabled=false;p.morph_position=0;reset_scene_chain(p.chain);reset_to_default(p.effects);for(auto& a:p.assets)reset_to_default(a);for(auto& s:p.scenes)reset_to_default(s);for(auto& s:p.sounds)reset_to_default(s);for(auto& c:p.clips)reset_to_default(c);for(auto& m:p.macros)reset_to_default(m);for(unsigned t=0;t<16;++t){reset_to_default(p.morph_a[t]);reset_to_default(p.morph_b[t]);for(unsigned s=0;s<3;++s){reset_to_default(p.morph_extra_a[s][t]);reset_to_default(p.morph_extra_b[s][t]);}}auto* empty_phrase=new PhraseState();for(auto& pattern:p.patterns){pattern.swing=0;pattern.mute_mask=0;for(auto& t:pattern.tracks)t=*empty_phrase;}delete empty_phrase;
 for(unsigned t=0;t<kTracks;++t){reset_to_default(p.tracks[t]);p.tracks[t].midi_channel=uint8_t(t);}
 for(auto& pattern:p.patterns)for(auto& t:pattern.tracks)for(auto& lane:t.automation)for(float& v:lane.values)v=1;
 const uint16_t targets[]={3,4,10,1,5,6,7,8};for(unsigned m=0;m<8;++m){auto& r=p.macros[m].routes[0];r.enabled=true;r.parameter=targets[m];const auto* d=parameter_descriptor(r.parameter);r.low=m==0?1:d->minimum;r.high=m==0?0.05f:d->maximum;}
}
}
