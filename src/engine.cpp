#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
uint32_t Engine::audio_song_tick()const{
 uint32_t tick=song_elapsed_ticks_;for(unsigned i=0;i<song_section_ && i<project_->song.length;++i)tick+=song_section_ticks(project_->song.sections[i]);return tick;
}
void Engine::reset_audio_lanes(){audio_lane_mask_=audio_pending_=0;audio_last_tick_=0xffffffffu;for(unsigned t=0;t<16;++t){audio_clip_[t]=-1;if(project_->audio[t].kind!=TrackKind::Instrument){audio_lane_mask_|=uint16_t(1u<<t);audio_pending_|=uint16_t(1u<<t);}}}
void Engine::start_audio_clip(unsigned t,unsigned index,uint32_t offset){
 const auto& a=project_->audio[t];const auto& clip=a.clips[index];auto& voice=voices_[t].sampler;voice.stop();voices_[t].off_at=0;voices_[t].instrument=unsigned(Instrument::Empty);voices_[t].env=0;audio_clip_[t]=-1;
 if(!a.enabled || clip.asset>=kSampleAssets || offset>=clip.frames)return;
 if(voice.start_region(samples_[clip.asset],rate_,clip.first+offset,clip.frames-offset,a.kind==TrackKind::Looper)){audio_clip_[t]=int8_t(index);voices_[t].sample_slot=clip.asset;voices_[t].sample_origin=0;voices_[t].sample_extent=1;}
}
void Engine::tick_audio_lanes(bool boundary){
 const uint32_t tick=audio_song_tick();const bool song=project_->song.enabled && project_->song.length && !song_finished_ && !song_armed_;
 const bool restart=audio_last_tick_==0xffffffffu || tick<audio_last_tick_;
 for(uint32_t mask=audio_lane_mask_;mask;mask&=mask-1){unsigned t=unsigned(__builtin_ctz(mask));const auto& a=project_->audio[t];auto& voice=voices_[t].sampler;
  if(a.kind==TrackKind::Looper){if(boundary && (audio_pending_&(1u<<t))){start_audio_clip(t,0);audio_pending_&=uint16_t(~(1u<<t));}continue;}
  if(!song){voice.stop();audio_clip_[t]=-1;continue;}
  const bool lane_restart=restart || (audio_pending_&(1u<<t));audio_pending_&=uint16_t(~(1u<<t));
  if(lane_restart){voice.stop();audio_clip_[t]=-1;}
  int selected=-1;uint32_t latest=0;
  for(unsigned c=0;c<kAudioClips;++c){const auto& clip=a.clips[c];if(clip.asset<kSampleAssets && clip.tick<=tick && (lane_restart || clip.tick>audio_last_tick_) && (selected<0 || clip.tick>=latest)){selected=int(c);latest=clip.tick;}}
  if(selected>=0){const auto& clip=a.clips[selected];uint64_t offset=uint64_t(tick-clip.tick)*samples_[clip.asset].rate*60/(uint64_t(bpm_)*960);start_audio_clip(t,unsigned(selected),offset<clip.frames?uint32_t(offset):clip.frames);}
 }
 audio_last_tick_=song?tick:0xffffffffu;
}
Engine::Engine(uint32_t sample_rate):pattern_staging_(new PatternState()),pattern_revisions_(new PatternRevisions()),creative_(new CreativeVoice[16]),rate_(sample_rate>=8000?sample_rate:48000),project_(new ProjectState()),staging_(new ProjectState()),track_views_(*project_),delay_(rate_),reverb_(rate_),performance_throws_(rate_),compressor_(rate_),main_limiter_(rate_),cue_limiter_(rate_),captures_{MasterCapture(rate_*4),MasterCapture(rate_*4)},performance_history_(rate_),performance_fx_(rate_) {
 insert_resources_ready_=prepare_wavetable_bank();
 va_triggers_.init(rate_);
 // Complete descriptor initialization before the real-time owner starts.
 (void)engine_descriptor(0);
 if(rate_==48000){granular_=alloc_array<GranularVoice>(4);if(granular_){bool ready=true;for(unsigned i=0;i<4;++i)ready=ready && granular_[i].ready();if(!ready){delete[] granular_;granular_=nullptr;}else for(unsigned i=0;i<4;++i)granular_[i].init(rate_);}}
 if(rate_==48000){resonators_=alloc_array<ResonatorVoice>(16);if(resonators_){bool ready=true;for(unsigned t=0;t<16;++t)ready=ready && resonators_[t].ready();if(!ready){delete[] resonators_;resonators_=nullptr;}else for(unsigned t=0;t<16;++t)resonators_[t].init(0x12345678u+t);}}
 if(rate_==48000){textures_=alloc_array<TextureVoice>(16);if(textures_){bool ready=true;for(unsigned t=0;t<16;++t)ready=ready && textures_[t].ready();if(!ready){delete[] textures_;textures_=nullptr;}else for(unsigned t=0;t<16;++t)textures_[t].init(0x12345678u+t);}}
 inverse_rate_=1.0f/float(rate_);kit_mix_alpha_=1-expf(-1/(float(rate_)*0.005f));
 modulation_storage_=new float[kInsertSlots*TrackInsertSlot::kModFloats];
 leaf_runtime_=alloc_array<LayerVoiceRuntime>(kPolyVoiceCapacity);if(!leaf_runtime_)insert_resources_ready_=false;
 if(leaf_runtime_)for(unsigned i=0;i<kPolyVoiceCapacity;++i){auto& leaf=leaf_runtime_[i];leaf.creative.reset();for(unsigned n=0;n<2;++n){unsigned slot=kTrackInsertSlots+i*2+n;leaf.inserts[n].bind_pitch(insert_pitch_pool_,slot);leaf.inserts[n].bind_modulation(modulation_storage_+slot*TrackInsertSlot::kModFloats);leaf.inserts[n].init(rate_);insert_resources_ready_&=leaf.inserts[n].ready();}poly_voices_[i].voice.leaf=&leaf;}
 public_drums_=new DaisyDrumBank[16];for(unsigned t=0;t<16;++t)public_drums_[t].init(float(rate_));
 preset_preview_=new PresetPreview();
 click_length_=rate_/100; if(!click_length_)click_length_=1;record_erased_=new uint64_t[16][kSteps]();erase_payload_=new SpscQueue<StepState,65>();attached_payload_=alloc_new<PreparedSoundQueue>();if(!attached_payload_)insert_resources_ready_=false;attached_consumer_=new SoundState();live_record_queue_=new SpscQueue<LiveRecordFrame,kLiveRecordCapacity+1>();commands_=new SpscQueue<CommandState,256>();sound_commands_=new SpscQueue<SoundState,65>();sound_command_consumer_=new SoundState();morph_commands_=new SpscQueue<TrackMorphState,65>();morph_command_consumer_=new TrackMorphState();midi_capture_events_=new SpscQueue<MidiCaptureEvent,2049>();if(rate_==48000){plaits_=new PlaitsVoice[16];for(unsigned t=0;t<16;++t)plaits_[t].init();poly_plaits_=alloc_array<PlaitsVoice>(kSharedVoiceCapacity);if(poly_plaits_)for(unsigned i=0;i<kSharedVoiceCapacity;++i){poly_plaits_[i].seed(0x6a09e667u+i*123u);poly_plaits_[i].init();}}
 for(unsigned t=0;t<16;++t) {
  sampler_source_epoch_[t]=1;pending_pattern_[t]=-1;track_compressors_[t].init(rate_);for(unsigned i=0;i<2;++i){auto& slot=runtime_[t].insert[i];slot.bind_pitch(insert_pitch_pool_,t*3+i);slot.bind_modulation(modulation_storage_+(t*3+i)*TrackInsertSlot::kModFloats);slot.init(rate_);} auto& v=voices_[t];v.kit_insert.bind_pitch(insert_pitch_pool_,t*3+2);v.kit_insert.bind_modulation(modulation_storage_+(t*3+2)*TrackInsertSlot::kModFloats);v.kit_insert.init(rate_);  v.saw.init(float(rate_)); v.daisy.init(rate_);v.wavetable.init(rate_);v.va.init(rate_,&va_triggers_);v.additive.init(rate_);v.phase_dist.init(rate_);v.formant.init(rate_);v.fm6.init(rate_);v.soundfont.init(t,rate_); v.fm.Init(float(rate_));
  track_views_.at(0,t).level=1; for(auto& step:track_views_.at(0,t).steps) step.velocity=110;
  reset_track(t,true); for(unsigned i=0;i<kTrackParameters;++i){runtime_[t].current[i]=runtime_[t].target[i];}runtime_[t].reverb_current=runtime_[t].reverb_target;
 }
 for(unsigned g=0;g<8;++g)for(unsigned slot=0;slot<2;++slot){group_inserts_[g][slot].bind_pitch(insert_pitch_pool_,16*3+kPolyVoiceCapacity+g*2+slot);group_inserts_[g][slot].bind_modulation(modulation_storage_+(16*3+kPolyVoiceCapacity+g*2+slot)*TrackInsertSlot::kModFloats);group_inserts_[g][slot].init(rate_);unsigned f=slot*4;group_fx_current_[g][f+1]=group_fx_target_[g][f+1]=0.5f;group_fx_current_[g][f+2]=group_fx_target_[g][f+2]=0.25f;group_fx_current_[g][f+3]=group_fx_target_[g][f+3]=1;}
 for(unsigned i=0;i<kPolyVoiceCapacity;++i){auto& v=poly_voices_[i].voice;v.kit_insert.bind_pitch(insert_pitch_pool_,16*3+i);v.kit_insert.bind_modulation(modulation_storage_+(16*3+i)*TrackInsertSlot::kModFloats);v.kit_insert.init(rate_);v.saw.init(float(rate_));v.daisy.init(rate_);v.wavetable.init(rate_);v.va.init(rate_,&va_triggers_);v.additive.init(rate_);v.phase_dist.init(rate_);v.formant.init(rate_);v.fm6.init(rate_);v.soundfont.init(16+i,rate_);v.fm.Init(float(rate_));}
 for(unsigned t=0;t<16;++t){for(auto& slot:runtime_[t].insert)insert_resources_ready_&=slot.ready();insert_resources_ready_&=voices_[t].kit_insert.ready();}
 for(auto& p:poly_voices_)insert_resources_ready_&=p.voice.kit_insert.ready();
 for(auto& group:group_inserts_)for(auto& slot:group)insert_resources_ready_&=slot.ready();
}
Engine::~Engine() {delete twin_install_;delete[] twin_storage_;delete[] leaf_runtime_; delete[] granular_;delete preset_preview_; delete[] textures_; delete[] resonators_; delete[] freeze_storage_;delete[] modulation_storage_;delete[] plaits_;delete[] poly_plaits_;delete[] public_drums_;delete[] record_erased_;delete erase_payload_;delete attached_payload_;delete attached_consumer_;delete live_record_queue_;delete commands_;delete sound_commands_;delete sound_command_consumer_;delete morph_commands_;delete morph_command_consumer_;delete midi_capture_events_;delete[] creative_;delete project_; delete staging_;delete pattern_staging_;delete pattern_revisions_; }
#include "preset_preview.inc"
bool Engine::stage_project(const ProjectState& p,const WavView* assets) {
 if(!insert_resources_ready_)return false;
 if(__atomic_load_n(&snapshot_pending_,__ATOMIC_ACQUIRE) || !valid_project(p)) return false;
 for(const auto& t:p.tracks) if(!engine_rate_supported(t.instrument,rate_)) return false;
 if(!textures_)for(const auto& t:p.tracks)if(t.instrument==Instrument::Texture)return false;
 if(!resonators_)for(const auto& t:p.tracks)if(t.instrument==Instrument::Resonator)return false;
 staged_assets_=assets!=nullptr; if(assets)for(unsigned i=0;i<kSampleAssets;++i)staged_samples_[i]=assets[i];
 snapshot_generation_=published_generation_+1;*staging_=p; __atomic_store_n(&snapshot_pending_,1,__ATOMIC_RELEASE);
 if(!commands_->push({CommandType::ApplySnapshot,0,0,0})) { __atomic_store_n(&snapshot_pending_,0,__ATOMIC_RELEASE); return false; }
 __atomic_store_n(&module_route_,0,__ATOMIC_RELEASE);__atomic_add_fetch(&module_route_epoch_,1,__ATOMIC_RELEASE);published_generation_=snapshot_generation_;return true;
}
bool Engine::stage_morph(unsigned side,const ParameterSnapshot snapshots[kTracks]){
 if(!morph_stage_ready(side) || !commands_->free_space())return false;
 for(unsigned t=0;t<kTracks;++t)if(!valid_engine_morph(snapshots[t]))return false;
 for(unsigned t=0;t<kTracks;++t)morph_staging_[side][t]=snapshots[t];
 __atomic_store_n(&morph_stage_pending_[side],1,__ATOMIC_RELEASE);
 if(!commands_->push({CommandType::StoreMorph,0,0,uint16_t(side)})){
  __atomic_store_n(&morph_stage_pending_[side],0,__ATOMIC_RELEASE);return false;
 }
 return true;
}
bool Engine::request_capture() {
 if(__atomic_load_n(&capture_state_,__ATOMIC_ACQUIRE)) return false;
 __atomic_store_n(&capture_state_,1,__ATOMIC_RELEASE);
 if(!submit({CommandType::RequestCapture,0,0,0})) { __atomic_store_n(&capture_state_,0,__ATOMIC_RELEASE); return false; } return true;
}
unsigned Engine::copy_capture(float* stereo,unsigned maximum_frames) {
 if(!stereo || __atomic_load_n(&capture_state_,__ATOMIC_ACQUIRE)!=2) return 0;
 unsigned frames=captures_[capture_frozen_].copy_recent(stereo,maximum_frames);
 __atomic_store_n(&capture_state_,0,__ATOMIC_RELEASE); return frames;
}
bool Engine::start_live_record(bool quantized,uint32_t frames,uint8_t source,uint8_t restart_track,uint8_t restart_pattern,uint8_t bars){if(bars>kLoopMaxBars || (bars && (!quantized || frames || restart_track!=255)) || (source!=255 && source>kCaptureMax) || (restart_track!=255 && (restart_track>=16 || restart_pattern>=kPatterns || !quantized || !frames)))return false;unsigned expected=0;if(!__atomic_compare_exchange_n(&live_record_state_,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return false;__atomic_store_n(&live_record_failed_,0,__ATOMIC_RELEASE);__atomic_store_n(&live_record_conflict_,0,__ATOMIC_RELEASE);__atomic_store_n(&live_record_bar_done_,0,__ATOMIC_RELEASE);Command c{CommandType::StartLiveRecord,uint8_t(restart_track<16?restart_track:0),source,uint16_t(quantized)};c.frames=frames;c.pattern=restart_pattern;c.parameter=bars;if(!submit(c)){__atomic_store_n(&live_record_state_,0,__ATOMIC_RELEASE);return false;}return true;}
bool Engine::stop_live_record(bool quantized){unsigned state=__atomic_load_n(&live_record_state_,__ATOMIC_ACQUIRE);return (state==1 || state==2 || state==4 || state==5) && submit({CommandType::StopLiveRecord,0,0,uint16_t(quantized)});}
bool Engine::midi(uint8_t status,uint8_t a,uint8_t b,uint8_t length) {
 // Reserve room for track and polyphonic Note Off events during a flood of CC or
 // Program Change messages. Transport messages retain priority as well.
 const bool note_off=(status&0xf0)==0x80 || ((status&0xf0)==0x90 && b==0);
 const bool transport=status==0xf2 || status==0xfa || status==0xfb || status==0xfc;
 if(!note_off && !transport && midi_events_.free_space()<64){__atomic_add_fetch(&midi_dropped_,1,__ATOMIC_RELAXED);if(midi_capture_active_ && !midi_capture_internal_ && status<0xf0){midi_capture_active_=false;__atomic_store_n(&midi_capture_failed_,1,__ATOMIC_RELEASE);}return false;}
 MidiEvent event{sample_count_,0,length,{status,a,b}};if(!midi_events_.push(event)){__atomic_add_fetch(&midi_dropped_,1,__ATOMIC_RELAXED);if(midi_capture_active_ && !midi_capture_internal_ && status<0xf0){midi_capture_active_=false;__atomic_store_n(&midi_capture_failed_,1,__ATOMIC_RELEASE);}return false;}
 if(midi_capture_active_ && !midi_capture_internal_ && status<0xf0){if(midi_capture_events_->free_space()<2 || !midi_capture_events_->push({event,0})){midi_capture_active_=false;__atomic_store_n(&midi_capture_failed_,1,__ATOMIC_RELEASE);}}return true;
}
bool Engine::capture_internal(uint8_t status,uint8_t a,uint8_t b,uint8_t length){if(!midi_capture_active_ || !midi_capture_internal_)return false;MidiEvent event{sample_count_,0,length,{status,a,b}};if(midi_capture_events_->free_space()<2 || !midi_capture_events_->push({event,0})){midi_capture_active_=false;__atomic_store_n(&midi_capture_failed_,1,__ATOMIC_RELEASE);return false;}return true;}
bool Engine::capture_internal_track(unsigned t){if(t>=16)return false;if(track(t).instrument!=Instrument::ExternalMidi)return true;if(voices_[t].capture_note)return true;for(const auto& p:poly_voices_)if(p.owner==t && p.voice.capture_note)return true;return false;}
void Engine::capture_pressure(unsigned t,uint8_t value){if(!midi_capture_active_ || !midi_capture_internal_ || t>=16 || capture_pressure_[t]==value)return;if(capture_pressure_[t]==255 && value==0){capture_pressure_[t]=0;return;}if(capture_internal(uint8_t(0xd0|t),value,0,2))capture_pressure_[t]=value;}
// Kit wire gates are independent of sample availability and DSP voice stealing.
void Engine::kit_midi_on(unsigned t,unsigned pad,unsigned velocity,const KitStepState* step,ConstTrackView state){
 if(incoming_midi_ || t>=16 || pad>=16 || audio_muted(t) || (audio_has_solo() && !audio_solo_track(t)))return;
 auto& gate=kit_midi_[t][pad];if(gate.note<128){if(!midi(uint8_t(0x80|gate.channel),gate.note,0))return;capture_internal(uint8_t(0x80|t),gate.note,0);gate.note=255;}
 uint8_t note=kit_gm_note(state.kit,pad);if(!midi(uint8_t(0x90|state.midi_channel),note,uint8_t(velocity)))return;
 capture_internal(uint8_t(0x90|t),note,uint8_t(velocity));gate.note=note;gate.channel=state.midi_channel;gate.hardware=255;
 gate.due=sample_count_+step_period(t);if(!step)gate.due=0;
 if(gate.due && (!kit_midi_due_ || gate.due<kit_midi_due_))kit_midi_due_=gate.due;
}
void Engine::kit_midi_tick(){if(!kit_midi_due_ || sample_count_<kit_midi_due_)return;kit_midi_due_=0;for(unsigned t=0;t<16;++t)for(auto& gate:kit_midi_[t])if(gate.note<128 && gate.due){if(gate.due<=sample_count_){if(midi(uint8_t(0x80|gate.channel),gate.note,0)){capture_internal(uint8_t(0x80|t),gate.note,0);gate.note=255;continue;}gate.due=sample_count_+64;}if(!kit_midi_due_ || gate.due<kit_midi_due_)kit_midi_due_=gate.due;}}
void Engine::kit_midi_release(unsigned hardware,unsigned track_index){for(unsigned t=0;t<16;++t)if(track_index==255 || track_index==t)for(auto& gate:kit_midi_[t])if(gate.note<128 && (hardware==255 || gate.hardware==hardware)){if(midi(uint8_t(0x80|gate.channel),gate.note,0)){capture_internal(uint8_t(0x80|t),gate.note,0);gate.note=255;}else{gate.due=sample_count_+1;kit_midi_due_=gate.due;}}}
void Engine::capture_voice_on(Voice& voice,unsigned t,unsigned velocity){voice.capture_note=false;if(!midi_capture_active_ || !midi_capture_internal_ || !velocity || voice.instrument==unsigned(Instrument::ExternalMidi) || voice.kit_pad<16 || audio_muted(t) || (audio_has_solo() && !audio_solo_track(t)))return;unsigned bend=unsigned(bounded(int(voice.pitch_bend*4096+8192.5f),0,16383));if(capture_bend_[t]!=bend){if(capture_bend_[t]!=65535 || bend!=8192){if(!capture_internal(uint8_t(0xe0|t),uint8_t(bend&127),uint8_t(bend>>7)))return;}capture_bend_[t]=uint16_t(bend);}capture_pressure(t,uint8_t(bounded(int(runtime_[t].pressure_target*127+0.5f),0,127)));voice.capture_pitch=voice.instrument==unsigned(Instrument::SoundFont) && voice.soundfont_base.values[0]==128?uint8_t(voice.soundfont.key()):voice.note;if(capture_internal(uint8_t(0x90|t),voice.capture_pitch,uint8_t(velocity))){voice.capture_channel=uint8_t(t);voice.capture_note=true;}}
void Engine::capture_voice_off(Voice& voice){if(!voice.capture_note)return;voice.capture_note=false;capture_internal(uint8_t(0x80|voice.capture_channel),voice.capture_pitch,0);}
// UI producer owns this staging buffer until the ApplyPattern publication.
bool Engine::replace_track_phrase(unsigned pattern,unsigned t,const TrackState& state){
 if(pattern>=kPatterns || t>=kTracks || __atomic_load_n(&pattern_pending_,__ATOMIC_ACQUIRE))return false;
 pattern_staging_->name_mask=0;pattern_staging_->install.clear();pattern_staging_->tracks[t]=state;
 pattern_staging_->groove_only=false;pattern_staging_->include_settings=pattern_staging_->include_morph=false;pattern_staging_->swing=0;
 return replace_pattern(pattern,*pattern_staging_,uint16_t(1u<<t),true);
}
bool Engine::replace_track(unsigned pattern,unsigned t,const TrackState& state,bool preserve_phase,const Command* binding,const WavView* bindings,const SampleAssetState* assets,const TrackMorphState* morph,const TrackInstall* install){
 if(pattern>=kPatterns || t>=kTracks || __atomic_load_n(&pattern_pending_,__ATOMIC_ACQUIRE))return false;
 pattern_staging_->name_mask=0;pattern_staging_->install.clear();if(install)pattern_staging_->install=*install;
 pattern_staging_->tracks[t]=state;pattern_staging_->groove_only=false;pattern_staging_->include_settings=true;pattern_staging_->include_morph=morph!=nullptr;if(morph)pattern_staging_->morph[t]=*morph;pattern_staging_->swing=0;
 return replace_pattern(pattern,*pattern_staging_,uint16_t(1u<<t),preserve_phase,binding,bindings,assets);
}
bool Engine::replace_pattern(unsigned index,const PatternState& pattern,uint16_t changed_tracks,bool preserve_phase,const Command* sample_binding,const WavView* bindings,const SampleAssetState* assets){if(bool(bindings)!=bool(assets) || (bindings && sample_binding) || (sample_binding && (sample_binding->type!=CommandType::BindSample || sample_binding->value>=kSampleAssets || sample_binding->parameter!=1)))return false;if(index>=kPatterns || __atomic_load_n(&pattern_pending_,__ATOMIC_ACQUIRE) || !commands_->free_space() || pattern.swing>100)return false;if(pattern.install.preset.slot!=kNoPreset && (pattern.install.preset.slot>=kPresetSlots || !valid_sound(pattern.install.preset.sound)))return false;if(pattern.install.clones.count>kSampleAssets || ((bindings || sample_binding) && pattern.install.clones.count))return false;uint64_t clone_sources=0,clone_targets=0;for(unsigned i=0;i<pattern.install.clones.count;++i){const auto& clone=pattern.install.clones.rows[i];if(clone.from>=kSampleAssets || clone.to>=kSampleAssets || clone.from==clone.to || clone.asset.metadata.root>127 || clone.asset.metadata.bpm>300 || (clone_targets&(uint64_t(1)<<clone.to)))return false;clone_sources|=uint64_t(1)<<clone.from;clone_targets|=uint64_t(1)<<clone.to;}if(clone_sources&clone_targets)return false;pattern_staging_->name_mask=pattern.name_mask;for(unsigned n=0;n<kTracks;++n)if(pattern.name_mask&(1u<<n)){if(!valid_preset_name(pattern.track_names[n]))return false;copy_track_field(pattern_staging_->track_names[n],pattern.track_names[n]);}pattern_staging_->install=pattern.install;uint16_t mask=changed_tracks?changed_tracks:0xffff;for(unsigned t=0;t<kTracks;++t)if((mask&(1u<<t)) && !valid_track(pattern.tracks[t]))return false;for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t))pattern_staging_->tracks[t]=pattern.tracks[t];pattern_staging_->groove_only=pattern.groove_only;pattern_staging_->include_settings=pattern.include_settings;pattern_staging_->include_morph=pattern.include_morph;if(pattern.include_settings && pattern.include_morph)for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t))pattern_staging_->morph[t]=pattern.morph[t];if(mask==0xffff){pattern_staging_->swing=pattern.swing;pattern_staging_->mute_mask=pattern.mute_mask;}pattern_assets_=bindings!=nullptr;if(bindings)for(unsigned i=0;i<kSampleAssets;++i){pattern_samples_[i]=bindings[i];pattern_sample_metadata_[i]=assets[i].metadata;}__atomic_store_n(&pattern_pending_,1,__ATOMIC_RELEASE);Command apply{CommandType::ApplyPattern,0,uint8_t(preserve_phase),uint16_t(index)};apply.parameter=changed_tracks;uint32_t revision=pattern_revision_serial_+1;if(!revision)++revision;apply.pattern_revision=revision;if(sample_binding){apply.frames=unsigned(sample_binding->value)+1;apply.sample=sample_binding->sample;apply.metadata=sample_binding->metadata;}if(!submit(apply)){__atomic_store_n(&pattern_pending_,0,__ATOMIC_RELEASE);return false;}if(!pattern.groove_only){pattern_revision_serial_=revision;for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t))pattern_revisions_->published[index][t]=revision;}return true;}
// O-09: 1024-entry sine table with linear interpolation (max
// error ~5e-6) for the per-sample oscillator/coefficient sites
// that used to call sinf every sample. Built once, on first use.
}
