#pragma once
#include "wavetable_voice.h"
#include "fm6_voice.h"
#include "soundfont.h"
#include "spectral_voices.h"
#include "formant_voice.h"
#include "granular_voice.h"
#include "va_sample_slot.h"
#include <stdint.h>
#include <stddef.h>
#include "polyblep_saw.h"
#include "step_expression.h"
#include "spsc_queue.h"
#include "wav_sampler.h"
#include "daisy_synth.h"
#include "plaits_voice.h"
#include "resonator_voice.h"
#include "texture_voice.h"
#include "daisy_drum_bank.h"
#include "daisy_track_drive.h"
#include "track_insert.h"
#include "audio_stage_profile.h"
#include "dsp_profile.h"
#include "core_work_meter.h"
#include "render_audit.h"
#include "kit_voice_mix.h"
#include "Synthesis/fm2.h"
#include "project.h"
#include "prepared_sound_queue.h"
#include "scene_take.h"
#include "group_routing_cache.h"
#include "creative_voice.h"
#include "tempo_lfo.h"
#include "generative.h"
#include "midi_clock_estimator.h"
#include "parameter_resolver.h"
#include "visual_telemetry.h"
#include "backends.h"
#include "live_effects.h"
namespace groove {
enum class ClockEventType : uint8_t { Start, Stop, Tick };
struct ClockEvent { ClockEventType type; uint64_t sample; };
struct MidiEvent { uint64_t sample=0; uint8_t cable=0,length=0,bytes[3]{}; };
struct MidiCaptureEvent {MidiEvent event{};uint8_t kind=0;};
// File owner transfers this transaction to Engine until status is acknowledged.
struct TwinInstall {
 SoundState source{},sampled{};SampleAssetState material{};WavView view{};TwinState twin{};
 uint32_t generation=0,revision=0;unsigned track=0,asset=64,status=0,cancel=0;
};
enum class EngineEventType : uint8_t { Step, ClipLaunched, SceneLaunched, Note, ParameterChanged, TempoChanged, GesturePoint, GestureState, CaptureReady, RecordedNote, RecordedGate, RecordCapacity, RecordErase, RecordStopped, CountInState, Transport, SongPosition, ProgramChanged, RecordedKitHit, SoundAttached, ClipLaunchRejected, EngineParameterChanged, TwinRejected };
struct QueuedEngineEvent { EngineEventType type; uint64_t sample; uint8_t track,value,flags=0; uint16_t parameter=0; float scalar=0; uint32_t generation=0;uint16_t tick=0;uint32_t recording_id=0,pattern_revision=0; };
// attached_sound is consumer scratch, valid until the next pop_event call.
struct EngineEvent:QueuedEngineEvent {StepState prior_step{};const SoundState* attached_sound=nullptr;};
enum class LaunchQuantization : uint8_t { Immediate, Bar };
enum class CommandType : uint8_t { Trigger, SetStep, SetLength, SetTempo, SetMute, SetSolo, SetInstrument, SetProbability, SetVelocity, BindSample, SetSample, SetNote, SetEngineParameter, SetDx7Bank, ApplySnapshot, SetParameter, StoreMorph, SetStepLock, ClearStepLock, SetCondition, SetSoundLock, SetRatchet, SetGate, SetCycle, SetSampleRegion, SetMicrotiming, SetDirection, SetSpeed, SetFill, MidiInput, SetExternalClock, SetTranspose, SetScale, SetVelocityScale, SetMorph, SetMorphEnabled, LaunchClip, StopClip, LaunchScene, AppendChain, ClearChain, SetChainEnabled, SetStutter, SetPerformanceFx, SetDelayMix, SetDelayFeedback, SetDelayTime, RequestCapture, NoteOff, Continue, Start, Stop, SetControlAll, SetChainBars, SendMidiCC, Panic, SetAdvanced, SetFollow, SetGrooveSnapshot, SetAutomation, SetMacroValue, SetMacroRoute, ResetLive, SetScene, SetSound, ApplySound, ApplyPattern, SetMidiChannel, SetSwing, SetStepNote, SetChain, ResendMidiState, SetCreative, SetSampleSlot, SetTuning, GestureRecord, GesturePlay, GestureClear, SetSampleEdit, SetRouting, SetTrackBus, SetTrackSendMode, PreviewSample, StopPreview, DiagnosticTone, SetAssetPlayback, StartLiveRecord, StopLiveRecord, RestoreSampleAssignment, SetStepChord, SetRecordedStep, ArmNoteRecord, SetNoteInputQuantize, SetNoteReplace, SetClickSettings, StartCountIn, SetMutation, SetNoteRepeat, SetRepeatConfig, SetRepeatLatch, SetTrackArp, SeekSong, SetMidiInputRoute, StartMidiCapture, StopMidiCapture, SetNrpnConfig, SetChordConfig, ResendMidiTransport, SetMidiProgram, SetKitPad, SetKitStep, SetKitSolo, SetSongSection, SetSong, SetMorphSet, CaptureRetro, SceneTakeControl, SetClipFollow, StripExpression, SetAudioLane, SetSoundOverride, PreviewPreset, SetMultitimbral, SetInputGain, ReleaseLocal, LocalPad, SetStepAttributes, RegisterPreset, SetAssetFavorite, InstallTwin, ToggleTwin };
struct CommandState { CommandType type{}; uint8_t track=0; uint8_t step=0; uint16_t value=0; WavView sample{}; Dx7BankView bank{}; uint8_t pattern=255; uint16_t parameter=0; float scalar=0,scalar2=0,scalar3=0; AudioLane audio_lane{};ClipFollow clip_follow{};SampleMetadata metadata{};SampleEdit sample_edit{};RoutingState routing{};CreativeState creative{};GrooveState groove{};AutomationLane automation{};MacroRoute route{};SceneChain chain{};SceneState scene{};SongSection song_section{};SongState song_state{};uint32_t frames=0,pattern_revision=0; StepState recorded_step{};KitPadState kit_pad{};KitStepState kit_step{};bool restore_morph=false; };
// Task-side envelope. Large preset state travels on its own bounded SPSC queue;
// ordinary MIDI/parameter commands never copy it into the audio command ring.
struct Command:CommandState {
 Command()=default;
 template<class... Tail> Command(CommandType kind,uint8_t owner,uint8_t cell,uint16_t data,Tail... tail):CommandState{kind,owner,cell,data,tail...}{}
 Command(const Command& other):CommandState(other),sound_(other.sound_?new SoundState(*other.sound_):nullptr),morph_(other.morph_?new TrackMorphState(*other.morph_):nullptr){}
 Command(Command&& other)noexcept:CommandState(other),sound_(other.sound_),morph_(other.morph_){other.sound_=nullptr;other.morph_=nullptr;}
 Command& operator=(const Command& other){if(this==&other)return *this;Command copy(other);return *this=static_cast<Command&&>(copy);}
 Command& operator=(Command&& other)noexcept{if(this==&other)return *this;CommandState::operator=(other);delete sound_;delete morph_;sound_=other.sound_;morph_=other.morph_;other.sound_=nullptr;other.morph_=nullptr;return *this;}
 ~Command(){delete sound_;delete morph_;}
 SoundState& sound(){if(!sound_)sound_=new SoundState();return *sound_;}
 const SoundState& sound()const{return sound_?*sound_:empty_sound();}
 TrackMorphState& morph(){if(!morph_)morph_=new TrackMorphState();restore_morph=true;return *morph_;}
 const TrackMorphState* morph_payload()const{return morph_;}
private:
 SoundState* sound_=nullptr;TrackMorphState* morph_=nullptr;
};
struct LiveRecordFrame {float l=0,r=0;};
class Engine {
public:
 static constexpr unsigned kPolyVoiceCapacity=VoiceBudget::pool_slots;
 static constexpr unsigned kSharedVoiceCapacity=4;
 static_assert(kPolyVoiceCapacity<=32,"poly voice render mask is 32-bit");
 explicit Engine(uint32_t sample_rate=48000);
 ~Engine();
 Engine(const Engine&)=delete; Engine& operator=(const Engine&)=delete;
 bool stage_project(const ProjectState& project,const WavView* assets=nullptr); // producer context; one snapshot in flight
 bool stage_morph(unsigned side,const ParameterSnapshot snapshots[kTracks]); // producer context; one snapshot per side in flight
 bool morph_stage_ready(unsigned side)const{return side<2 && !__atomic_load_n(&morph_stage_pending_[side],__ATOMIC_ACQUIRE);}
 bool replace_track(unsigned pattern,unsigned track,const TrackState&,bool preserve_phase=false,const Command* binding=nullptr,const WavView* bindings=nullptr,const SampleAssetState* assets=nullptr,const TrackMorphState* morph=nullptr,const TrackInstall* install=nullptr);
 bool replace_track_phrase(unsigned pattern,unsigned track,const TrackState&);
 bool replace_pattern(unsigned index,const PatternState& pattern,uint16_t changed_tracks=0xffff,bool preserve_phase=false,const Command* sample_binding=nullptr,const WavView* bindings=nullptr,const SampleAssetState* assets=nullptr);
 DspProfile& dsp_profile(){return dsp_profile_;}
 RenderAudit& render_audit(){return render_audit_;}
 void begin_stage_profile(AudioStageProfile::Clock clock,unsigned interval=64){stage_profile_.begin(clock,interval);}
 AudioStageProfile::Snapshot end_stage_profile(){return stage_profile_.end();}
 uint32_t sample_rate()const{return rate_;}
 // Task-owned storage is handed to the sole audio owner with a release /
 // acquire mailbox. State 5 is a pending start; 1/2 are armed/capturing;
 // 3/4 acknowledge completion/abort. Release never frees an active buffer.
 bool submit_twin_install(TwinInstall*);TwinInstall* twin_install_result()const;void release_twin_install();
 bool twin_capture_begin(unsigned track,unsigned note,unsigned velocity,unsigned hold_ms,unsigned max_ms);
 const float* twin_capture_data()const{return freeze_capture_state()==3?twin_storage_:nullptr;}
 uint32_t twin_capture_revision()const{return freeze_capture_state()==3?twin_capture_revision_:0;}
 bool freeze_capture_begin(unsigned track,uint32_t maximum_frames=0);
 void freeze_capture_abort();
 void freeze_capture_release();
 void service_freeze_capture(); // task owner: collect an acknowledged release
 unsigned freeze_capture_state()const{return __atomic_load_n(&freeze_status_,__ATOMIC_ACQUIRE);}
 unsigned freeze_capture_pattern()const{return freeze_capture_state()==3?freeze_pattern_:255;}
 uint32_t freeze_capture_frames()const{return freeze_capture_state()==3?__atomic_load_n(&freeze_published_frames_,__ATOMIC_RELAXED):0;}
 float freeze_capture_progress()const;
 const int16_t* freeze_capture_data()const{return freeze_capture_state()==3?freeze_storage_:nullptr;}
 // Diagnostic only: count successfully started sequenced kit voices matching
 // two exact track/pattern/pad/step/velocity identities. No audio is copied.
 bool watch_kit_hits(unsigned track,unsigned pattern,unsigned step0,unsigned step1,unsigned velocity0,unsigned velocity1){if(track>=16 || pattern>=16 || step0>=kSteps || step1>=kSteps || !velocity0 || velocity0>127 || !velocity1 || velocity1>127)return false;__atomic_store_n(&kit_watch_active_,0,__ATOMIC_RELEASE);for(unsigned i=0;i<2;++i){unsigned step=i?step1:step0,velocity=i?velocity1:velocity0;uint32_t spec=track|(pattern<<4)|(i<<8)|(step<<12)|(velocity<<20);__atomic_store_n(&kit_watch_spec_[i],spec,__ATOMIC_RELAXED);__atomic_store_n(&kit_watch_hits_[i],0,__ATOMIC_RELAXED);__atomic_store_n(&kit_watch_notes_[i],0,__ATOMIC_RELAXED);__atomic_store_n(&kit_watch_velocity_[i],0,__ATOMIC_RELAXED);__atomic_store_n(&kit_watch_frames_[i],0,__ATOMIC_RELAXED);}__atomic_store_n(&kit_watch_active_,1,__ATOMIC_RELEASE);return true;}
 unsigned kit_watch_hits(unsigned pad)const{return pad<2?__atomic_load_n(&kit_watch_hits_[pad],__ATOMIC_ACQUIRE):0;}
 unsigned kit_watch_notes(unsigned pad)const{return pad<2?__atomic_load_n(&kit_watch_notes_[pad],__ATOMIC_ACQUIRE):0;}
 unsigned kit_watch_velocity(unsigned pad)const{return pad<2?__atomic_load_n(&kit_watch_velocity_[pad],__ATOMIC_ACQUIRE):0;}
 unsigned kit_watch_frames(unsigned pad)const{return pad<2?__atomic_load_n(&kit_watch_frames_[pad],__ATOMIC_ACQUIRE):0;}
 bool diagnostic_playing()const{return playing_;} // DSP consumer only
 // Diagnostic read by the DSP consumer only, never from a UI/task thread.
 void set_va_helper(VaSampleSlot* helper){va_helper_=helper;va_candidates_dirty_=true;} // before starting the DSP owner
 DspAssistCounts va_helper_counts()const{return va_helper_?va_helper_->counts():DspAssistCounts{};} // DSP owner only
 PreparedSoundQueue::Counters prepared_sound_counts()const{return attached_payload_?attached_payload_->counters():PreparedSoundQueue::Counters{};}
 PolyStealCounts poly_steal_counts()const{return poly_steals_;} // DSP owner only
 EfficiencyAudioMeter::Population efficiency_population()const{
  EfficiencyAudioMeter::Population out;out.event_work=event_work_;
  for(const auto& v:voices_)if(v.env>=.00001f){++out.main;if(v.instrument==unsigned(Instrument::VirtualAnalog)){++out.va;++out.va_main;}out.sampler+=v.instrument==unsigned(Instrument::Sampler) && v.sampler.active();}
  for(const auto& p:poly_voices_)if(p.owner<16){++out.owned;out.released+=p.voice.released;if(p.voice.env>=.00001f){++out.poly;if(p.voice.instrument==unsigned(Instrument::VirtualAnalog)){++out.va;++out.va_poly;}out.sampler+=p.voice.instrument==unsigned(Instrument::Sampler) && p.voice.sampler.active();}}
  return out;
 }
 unsigned native_refused_mask()const{return __atomic_load_n(&native_refused_,__ATOMIC_ACQUIRE);}
 unsigned native_active_mask()const{return __atomic_load_n(&native_active_,__ATOMIC_ACQUIRE);}
 void stop_kit_watch(){__atomic_store_n(&kit_watch_active_,0,__ATOMIC_RELEASE);}
 unsigned command_space()const{return commands_->free_space();}
 unsigned sound_command_space()const{return sound_commands_->free_space();}
 unsigned morph_command_space()const{return morph_commands_->free_space();}
 bool pop_midi(MidiEvent& event) { return midi_events_.pop(event); }
 unsigned midi_boundary()const{return midi_events_.consumer_boundary();}
 bool midi_at_boundary(unsigned boundary)const{return midi_events_.consumer_at_boundary(boundary);}
 bool pop_midi_capture(MidiCaptureEvent& event){return midi_capture_events_->pop(event);}
 bool midi_capture_failed()const{return __atomic_load_n(&midi_capture_failed_,__ATOMIC_ACQUIRE)!=0;}
 bool start_midi_capture(bool internal=false){__atomic_store_n(&midi_capture_failed_,0,__ATOMIC_RELEASE);return submit({CommandType::StartMidiCapture,0,0,uint16_t(internal)});}
 bool stop_midi_capture(){return submit({CommandType::StopMidiCapture,0,0,0});}
 void visual_focus(unsigned track,unsigned source=255){unsigned focus=(track<16?track:0)|((source<=60?source:255)<<8);if(__atomic_load_n(&visual_requested_track_,__ATOMIC_RELAXED)!=focus)__atomic_store_n(&visual_requested_track_,focus,__ATOMIC_RELAXED);}
 void visual_control(unsigned slot,unsigned track,uint16_t target){if(slot<8){uint32_t key=(uint32_t(track&15)<<16)|target;if(__atomic_load_n(&visual_control_request_[slot],__ATOMIC_RELAXED)!=key)__atomic_store_n(&visual_control_request_[slot],key,__ATOMIC_RELAXED);}}
 bool pop_visual(AudioVisualFrame& frame){return visual_frames_.pop(frame);}
 bool pop_event(EngineEvent& event) {QueuedEngineEvent queued;if(!events_.pop(queued))return false;static_cast<QueuedEngineEvent&>(event)=queued;event.attached_sound=nullptr;if(queued.type==EngineEventType::SoundAttached){if(attached_payload_ && attached_payload_->pop(*attached_consumer_))event.attached_sound=attached_consumer_;else __atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);}if(queued.type==EngineEventType::RecordErase && !erase_payload_->pop(event.prior_step)){event.type=EngineEventType::RecordCapacity;__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);}if(project_event(queued.type))__atomic_sub_fetch(&project_events_pending_,1,__ATOMIC_RELEASE);return true;}
 uint32_t event_dropped() const { return __atomic_load_n(&event_dropped_,__ATOMIC_RELAXED); }
 bool project_event_loss() const { return __atomic_load_n(&project_event_loss_,__ATOMIC_ACQUIRE)!=0; }
 bool project_events_pending() const {return __atomic_load_n(&project_events_pending_,__ATOMIC_ACQUIRE)!=0;}
 bool project_snapshot_safe() const {return !project_events_pending() && !project_event_loss();}
 uint32_t visual_event_dropped() const { return __atomic_load_n(&visual_event_dropped_,__ATOMIC_RELAXED); }
 uint32_t repeat_dropped() const { return __atomic_load_n(&repeat_dropped_,__ATOMIC_RELAXED); }
 uint32_t midi_dropped() const { return __atomic_load_n(&midi_dropped_,__ATOMIC_RELAXED); }
 bool receive_midi(const MidiPacket& packet,uint8_t target_track=15){if(packet.cable>=16 || !valid_midi_packet(packet) || target_track>=16)return false;Command c{CommandType::MidiInput,target_track,packet.bytes[1],packet.bytes[0]};c.scalar=packet.length==3?packet.bytes[2]:0;c.parameter=packet.cable;if(!packet.sample)return submit(c);unsigned epoch=__atomic_load_n(&midi_epoch_,__ATOMIC_ACQUIRE);if(last_timed_generation_!=published_generation_ || last_timed_epoch_!=epoch){last_timed_=0;last_timed_generation_=published_generation_;last_timed_epoch_=epoch;}if(packet.sample<last_timed_ || !timed_midi_.push({packet,target_track,published_generation_,epoch}))return false;last_timed_=packet.sample;return true;}

 SceneTake::State scene_take_state()const{return scene_take_.state();}
 bool copy_scene_take(SongState& out)const{return scene_take_.copy(out);}
 SceneTake::Error scene_take_error()const{return scene_take_.error();}
 void acknowledge_scene_take(){scene_take_.acknowledge();}
 bool request_capture(); // UI producer; result becomes available on an audio callback
 unsigned copy_capture(float* stereo,unsigned maximum_frames); // UI producer; immutable completed capture
 bool capture_ready() const { return __atomic_load_n(&capture_state_,__ATOMIC_ACQUIRE)==2; }
 uint32_t live_record_tick()const{return __atomic_load_n(&live_record_tick_,__ATOMIC_ACQUIRE);}
 bool start_live_record(bool quantized=false,uint32_t frames=0,uint8_t source=255,uint8_t restart_track=255,uint8_t restart_pattern=255,uint8_t bars=0); // task producer; audio owns sample production
 bool stop_live_record(bool quantized=false);
 static constexpr unsigned kLiveRecordCapacity=65536;
 unsigned live_record_highwater()const{return __atomic_load_n(&live_record_highwater_,__ATOMIC_ACQUIRE);}
 bool pop_live_record(LiveRecordFrame& frame){return live_record_queue_->pop(frame);}
 bool live_record_empty()const{LiveRecordFrame frame;return !live_record_queue_->peek(frame);}
 bool live_record_done()const{return __atomic_load_n(&live_record_state_,__ATOMIC_ACQUIRE)==3;}
 unsigned live_record_state()const{return __atomic_load_n(&live_record_state_,__ATOMIC_ACQUIRE);}
 // Failure codes: 1 transport, 2 input starvation, 3 input overflow, 4 recording queue.
 unsigned live_record_failure_reason()const{return __atomic_load_n(&live_record_failed_,__ATOMIC_ACQUIRE);}
 bool live_record_failed()const{return __atomic_load_n(&live_record_failed_,__ATOMIC_ACQUIRE)!=0;}
 bool live_record_bars_complete()const{return __atomic_load_n(&live_record_bar_done_,__ATOMIC_ACQUIRE)!=0;}
 bool live_record_conflict()const{return __atomic_load_n(&live_record_conflict_,__ATOMIC_ACQUIRE)!=0;}
 void finish_live_record(){if(live_record_done())__atomic_store_n(&live_record_state_,0,__ATOMIC_RELEASE);}

 uint32_t staged_generation() const {return published_generation_;} // UI producer only
 uint32_t pattern_revision(unsigned pattern,unsigned track)const{return pattern<kPatterns && track<kTracks?pattern_revisions_->published[pattern][track]:0;} // UI producer only
 bool preview_preset(unsigned track,const SoundState&,const WavView*,const SampleAssetState*,uint64_t mask,uint32_t ticket);
 bool supports_engine(Instrument engine)const{return instrument_descriptor(unsigned(engine)) && engine_rate_supported(engine,rate_);}
 bool leaf_assets_ready(const SoundLeaf& sound)const{
  if(!engine_rate_supported(sound.instrument,rate_))return false;
  const uint64_t ready=__atomic_load_n(&sample_ready_mask_,__ATOMIC_ACQUIRE);
  if(sound.instrument==Instrument::SoundFont)return sound.sample_slot<kSampleAssets && (__atomic_load_n(&soundfont_ready_mask_,__ATOMIC_ACQUIRE)&(uint64_t(1)<<sound.sample_slot));
  if(sound.instrument==Instrument::Granular && !granular_)return false;
  if(engine_asset_mode(sound.instrument)==EngineAssetMode::Sample)return sound.sample_slot<kSampleAssets && (ready&(uint64_t(1)<<sound.sample_slot));
  if(sound.instrument==Instrument::DrumKit)for(const auto& pad:sound.kit.pads)for(auto slot:pad.sample)if(slot<kSampleAssets && !(ready&(uint64_t(1)<<slot)))return false;
  return true;
 }

 bool preset_assets_ready(const SoundState& sound)const{if(sound.instrument!=Instrument::Layer)return leaf_assets_ready(sound);if(layer_status(sound.layer)!=LayerReason::Ready)return false;for(unsigned i=0;i<sound.layer.count;++i)if(!leaf_assets_ready(sound.layer.parts[i].sound))return false;return true;}
 bool pop_clock(ClockEvent& event) { return clock_events_.pop(event); }
 uint32_t clock_dropped() const { return __atomic_load_n(&clock_dropped_,__ATOMIC_RELAXED); }
 static bool carries_sound(CommandType type){return type==CommandType::SetSound || type==CommandType::ApplySound || type==CommandType::RegisterPreset;}
 bool multitimbral()const{return __atomic_load_n(&module_route_,__ATOMIC_ACQUIRE)!=0;}
 uint32_t midi_route_epoch()const{return __atomic_load_n(&module_route_epoch_,__ATOMIC_ACQUIRE);}
 bool submit(const Command& c,uint8_t pattern=255) {
  if(c.type==CommandType::SetMultitimbral && c.value>1)return false;
  if(c.type==CommandType::SetInputGain && (c.value>48 || c.step>2))return false;
  if((carries_sound(c.type) && !sound_commands_->free_space()) || (c.restore_morph && (!c.morph_payload() || !morph_commands_->free_space())))return false;
  auto* slot=commands_->push_begin();if(!slot)return false;
  if(carries_sound(c.type) && !sound_commands_->push(c.sound()))return false;
  if(c.restore_morph && !morph_commands_->push(*c.morph_payload()))return false;
  *slot=static_cast<const CommandState&>(c);if(pattern!=255)slot->pattern=pattern;
  if(c.type==CommandType::SetMultitimbral){__atomic_add_fetch(&midi_epoch_,1,__ATOMIC_RELEASE);__atomic_store_n(&module_route_,unsigned(c.value),__ATOMIC_RELEASE);__atomic_add_fetch(&module_route_epoch_,1,__ATOMIC_RELEASE);}
  commands_->push_commit();return true;
 }
 // Called by the single task-side command producer, never by the audio owner.
 uint32_t next_preview_ticket(){preview_serial_=(preview_serial_+1)&0x7fffffffu;if(!preview_serial_)preview_serial_=1;return preview_serial_;}
 uint32_t preview_state()const{return __atomic_load_n(&preview_state_,__ATOMIC_ACQUIRE);}
 void request_panic() { __atomic_store_n(&panic_requested_,1,__ATOMIC_RELEASE); }
 // Interleaved stereo floats; called by one audio consumer, no allocation or I/O.
 void render(float* output,size_t frames){render_ports(output,nullptr,nullptr,frames);}
 void render_ports(float* main,float* cue,const float* input,size_t frames);
 bool receive_audio(float l,float r){if(input_audio_.push({l,r}))return true;__atomic_add_fetch(&input_dropped_,1,__ATOMIC_RELAXED);return false;}
 unsigned input_audio_dropped()const{return __atomic_load_n(&input_dropped_,__ATOMIC_RELAXED);}
 uint64_t samples_rendered() const { return sample_count_; } // consumer thread only
 unsigned active_sample_voices()const{unsigned count=0;for(const auto& v:voices_)if(v.instrument==unsigned(Instrument::Sampler) && v.sampler.active())++count;for(const auto& p:poly_voices_)if(p.owner<16 && p.voice.instrument==unsigned(Instrument::Sampler) && p.voice.sampler.active())++count;return count;} // consumer thread only
#ifdef TEST
 unsigned poly_note_count(unsigned t)const{unsigned n=0;for(const auto& p:poly_voices_)if(p.owner==t)++n;return n;}
 float test_lfo_phase(unsigned t)const{return runtime_[t].lfo_phase;}
 ModRuntime test_mod_runtime(unsigned t)const{return mod_runtime(t);}
 unsigned layer_note_voices(unsigned t)const{unsigned n=0;for(const auto& p:poly_voices_)if(p.owner==t && p.voice.composite)++n;return n;}
 LayerReason layer_last_reason(unsigned t)const{return LayerReason(layer_failure_[t]);}
 unsigned plaits_note_count(unsigned t)const{unsigned n=voices_[t].instrument==unsigned(Instrument::PlaitsMacro) && voices_[t].env>=0.00001f?1:0;for(const auto& p:poly_voices_)if(p.owner==t && p.voice.instrument==unsigned(Instrument::PlaitsMacro) && p.voice.env>=0.00001f)++n;return n;}
 bool plaits_note_held(unsigned t,unsigned note)const{for(const auto& p:poly_voices_)if(p.owner==t && p.voice.instrument==unsigned(Instrument::PlaitsMacro) && p.voice.input_note==note && !p.voice.released)return true;return voices_[t].instrument==unsigned(Instrument::PlaitsMacro) && voices_[t].input_note==note && voices_[t].env>=0.00001f && !voices_[t].released;}
 PerformanceFx performance_fx_current()const{return performance_fx_applied_;}
 float group_effect_target(unsigned g,unsigned field)const{return g<8 && field<11?group_fx_target_[g][field]:0;}
 bool poly_owner_masks_valid()const{uint32_t expected[16]{};for(unsigned i=0;i<kPolyVoiceCapacity;++i)if(poly_voices_[i].owner<16)expected[poly_voices_[i].owner]|=uint32_t(1u)<<i;for(unsigned i=0;i<16;++i)if(expected[i]!=poly_track_mask_[i])return false;return true;}
#endif
private:
 AudioStageProfile stage_profile_{};DspProfile dsp_profile_{};RenderAudit render_audit_{};
 static constexpr unsigned kTrackInsertSlots=16*3+kPolyVoiceCapacity+16,kInsertSlots=kTrackInsertSlots+2*kPolyVoiceCapacity;
 PitchShiftStereo::Pool insert_pitch_pool_{kInsertSlots};
 VirtualAnalogTriggerBank va_triggers_{};
 void process_profiled_insert(TrackInsertSlot&,float&,float&,unsigned,float,float,float,float,float,unsigned);
 float group_gain_[8]={1,1,1,1,1,1,1,1};
 TrackInsertSlot group_inserts_[8][2];float group_fx_current_[8][11]{},group_fx_target_[8][11]{};
 struct LayerVoiceRuntime {CreativeVoice creative;TrackInsertSlot inserts[2];PresetFxSnapshot fx{};};
 LayerVoiceRuntime* leaf_runtime_=nullptr;
 struct Voice {
  LayerVoiceRuntime* leaf=nullptr;
  uint8_t shared_slot=255;
  Voice* layer_peer=nullptr;bool composite=false;uint8_t layer_cost=0;float layer_gain=1,layer_pan=0,layer_detune=0;
  CreativeState creative_state{};
  StepPitchSlide step_pitch{};bool step_mono=false,step_accent=false,tie_pending=false;uint16_t step_sound=kNoPreset;
  float a=0,env=0,frequency=261.6256f,pitch_bend=0,acid_low=0,acid_band=0;
  TextureState texture_state{};float texture_values[15]{};ResonatorState resonator_state{};float resonator_values[8]{};
  bool sound_locked=false; float base_parameters[5]={0.5f,0.5f,0.5f,0.5f,0.5f};
  float targets[5]={0.5f,0.5f,0.5f,0.5f,0.5f};float parameters[5]={0.5f,0.5f,0.5f,0.5f,0.5f};
  AdditiveState additive_base{};AdditiveVoice additive;PhaseDistState phase_dist_base{};PhaseDistVoice phase_dist;FormantState formant_base{};FormantVoice formant;GranularState granular_base{};SoundFontState soundfont_base{};SoundFontVoice soundfont;Fm6State fm6_base{};Fm6Voice fm6;
  VirtualAnalogState va_base{};VirtualAnalogVoice va;
  WavetableState wavetable_base{};WavetableVoice wavetable;
  PolyBlepSaw saw; WavView sample{}; WavVoice sampler; DaisySynth daisy; daisysp::Fm2 fm;
  uint8_t instrument=unsigned(Instrument::Empty),channel=0,note=60,source_pattern=0,model=0,sample_slot=0,sample_root=60,choke_group=0;float sample_origin=0,sample_extent=1; SampleEdit base_sample_edit{};float sample_speed=1;uint32_t sampler_source_epoch=0;SampleMode sample_mode=SampleMode::OneShot;bool midi_note=false,capture_note=false,released=false,looping=false;uint8_t capture_channel=0,capture_pitch=60; uint8_t input_note=60,kit_pad=255,kit_watch_lane=255;float kit_mix_target[5]={0.8f,0,1,0,0};ParameterLock kit_mix_locks[kLocks]{};bool kit_mix_locked=false;KitVoiceMix kit_mix{};float kit_speed_bend=0,kit_speed_tuning=0,kit_speed_value=1;TrackInsertSlot kit_insert{};ParameterLock kit_fx_locks[kLocks]{};bool kit_fx_locked=false;float kit_insert_target[4]={0,0.5f,0.25f,1},kit_insert_current[3]={0.5f,0.25f,1};uint16_t received_channel=65535; uint64_t off_at=0;
 };
 struct TrackRuntime {
  DaisyTrackDrive drive;TrackInsertSlot insert[2];float insert_target[8]={0,0.5f,0.25f,1,0,0.5f,0.25f,1},insert_current[8]={0,0.5f,0.25f,1,0,0.5f,0.25f,1};float send_pre[2]{};
  uint8_t pattern=0; uint16_t position=0; bool playing=true,backward=false,automation_live=false; uint32_t cycle=0;bool follow_due=false;
  uint64_t automation_cell=0,automation_tick=0;uint16_t nrpn_sent[4]={65535,65535,65535,65535},nrpn_number[4]={65535,65535,65535,65535};uint8_t nrpn_channel=255,nrpn_type[4]={255,255,255,255};int16_t strip_pitch=0;float strip_mod=0,strip_mod_current=0;bool strip_pitch_active=false,strip_mod_active=false;uint16_t expression_bend=65535,expression_sent_bend=65535;uint8_t expression_sent_pressure=255,expression_channel=255;bool expression_held_bend=false,expression_held_pressure=false;uint8_t program_channel=255,program_sent=255,cc_channel=255,cc_sent[128]{};bool cc_held[128]{};uint64_t cc_hash=0;bool cc_primed=false,cc_dirty=true;uint64_t lane_triggers=0; StepState effective_step{}; int16_t accumulator_offset=0; bool accumulator_degrees=false;
  uint64_t period_q32=1,due=0;uint16_t timing_bpm=0;uint8_t timing_num=0,timing_den=0;
  int64_t grid=0,last_grid=0;uint64_t last_period_q32=0,step_serial=0; uint32_t fraction=0;
  float reverb_target=0,reverb_current=0;bool compressor_active=false;float compressor_target[7]={0.5f,4,10,120,0,0,0},compressor_current[7]={0.5f,4,10,120,0,0,0};
  float target[kTrackParameters]={0.8f,0,1,0,0,0,0,1},current[kTrackParameters]={0.8f,0,1,0,0,0,0,1};
  ParameterLock active_locks[kLocks]{};uint64_t step_fp=0;bool step_fp_valid=false;float crush_amount=-1,crush_steps=0;
  TempoLfo tempo_lfo{};
  bool pressure_from_pad=false;float last_velocity=0,pressure=0,pressure_target=0,pressure_depth=0,pressure_depth_target=0,filter_l=0,filter_r=0,lfo_phase=0,audible_gain=1; int16_t retrigger_note=60; uint8_t retriggers=0,retrigger_velocity=0,last_step=0;
  uint64_t retrigger_at=0,retrigger_period=1;int64_t retrigger_origin=0;uint32_t retrigger_fraction=0;uint8_t retrigger_index=1,retrigger_count=1;uint32_t arp_random=1;
 };
 TrackView& track(unsigned t) { return track_views_.at(runtime_[t].pattern,t); }
 TrackView& edit_track(const CommandState& c) { return track_views_.at(c.pattern<kPatterns?c.pattern:runtime_[c.track].pattern,c.track); }
 void command(const CommandState& c,const SoundState* sound=nullptr);
 PlaitsVoice* plaits_dsp(unsigned track,Voice& voice);
 struct PolyVoice;
 unsigned module_route_=0,module_route_epoch_=0;bool multitimbral_=false;uint8_t module_bank_[16][16][2]{},module_reason_[16]{};uint16_t module_slot_[16]{};
 bool layer_launching_=false;uint8_t layer_failure_[16]{};
 LayerReason layer_admission(unsigned,const LayerState&,unsigned,unsigned,uint8_t&)const;
 PolyVoice* oldest_shared(VoiceResource resource,unsigned owner,const Voice* protect);
 uint8_t reserve_shared(unsigned track,Instrument engine,unsigned model,bool poly,const Voice* protect);
 GestureOverlay gesture_overlay_{};
 void gesture_capture(const CommandState& c);void gesture_tick();
 Voice* trigger(unsigned track,unsigned velocity,int note=-1,const StepState* step=nullptr,const TrackView* source=nullptr,int accumulator=0,bool degrees=false,bool live_poly=false,int slice_override=-1,int kit_pad=-1,const KitStepState* kit_step=nullptr,int kit_layer=-1,Voice* protected_voice=nullptr,const LayerPart* layer_part=nullptr,const float* layer_mix=nullptr,bool chromatic=false);
 void trigger_frozen(unsigned t);
 void publish_voice_visual(AudioVisualFrame&,unsigned,Voice&);
 struct PolyVoice {Voice voice{};uint64_t age=0;uint8_t owner=255;bool sequenced=false,pending=false;};PolyVoice poly_voices_[kPolyVoiceCapacity]{};uint32_t poly_track_mask_[16]{};void set_poly_owner(PolyVoice& voice,unsigned owner){va_candidates_dirty_=true;unsigned index=unsigned(&voice-poly_voices_);uint32_t bit=uint32_t(1u)<<index;if(voice.owner<16)poly_track_mask_[voice.owner]&=~bit;voice.owner=uint8_t(owner);if(owner<16)poly_track_mask_[owner]|=bit;}uint8_t kit_round_robin_[16][16]{};
 struct KitMidiGate {uint64_t due=0;uint8_t note=255,channel=0,hardware=255;};KitMidiGate kit_midi_[16][16]{};uint64_t kit_midi_due_=0;bool incoming_midi_=false;
 void kit_midi_on(unsigned track,unsigned pad,unsigned velocity,const KitStepState* step,ConstTrackView state);void kit_midi_tick();void kit_midi_release(unsigned hardware=255,unsigned track=255);
 struct KitRepeat {uint64_t due=0,period=0;uint8_t remaining=0,step=0,velocity=0;};KitRepeat kit_repeat_[16][16]{};uint16_t kit_repeat_pads_[16]{},kit_repeat_tracks_=0;
 uint32_t kit_watch_active_=0,kit_watch_spec_[2]{},kit_watch_hits_[2]{},kit_watch_notes_[2]{},kit_watch_velocity_[2]{},kit_watch_frames_[2]{};
 KitRepeat kit_early_[16][16]{};uint16_t kit_early_pads_[16]{},kit_early_planned_[16]{},kit_early_started_[16]{},kit_early_tracks_=0;
 void plan_kit_early(unsigned track);void plan_kit_early_pad(unsigned track,unsigned pad);void clear_kit_early(unsigned track);void retime_kit_early(unsigned track);
 bool note_off_main(unsigned track);bool note_off(unsigned track);bool release_poly(PolyVoice& voice,bool immediate=false);bool release_sequence(unsigned track);bool hold_for_slide(unsigned track,Voice& voice);bool slide_voice(unsigned track,unsigned velocity,int note,const StepState& step,ConstTrackView source,int accumulator,bool degrees);void trigger_step_chord(unsigned track,unsigned velocity,int note,const StepState& step,int accumulator,bool degrees);void trigger_step_arp(unsigned track,unsigned velocity,int note,const StepState& step,int accumulator,bool degrees,unsigned index);bool release_note(unsigned track,uint8_t note,uint16_t channel);void release_midi_channel(unsigned track,uint16_t channel,bool immediate);void clear_poly(unsigned track); bool admit_native(unsigned t,Instrument engine,const float* values);
 uint64_t native_retry_sample_[16]{};NativeBudget native_budget_;float native_values_[16][15]{};unsigned native_refused_=0,native_active_=0;
bool plain_visual_parameters(unsigned track);
void resolve_voice_bank(unsigned track,Instrument engine,const float* bases,float* values);
 void render_sampler_voice(Voice& voice,float& left,float& right);
 void render_non_sampler_voice(unsigned track,Voice& voice,float& left,float& right,bool poly,unsigned voice_key);
 void advance_legacy_voice(Voice& voice);

 void reset_track(unsigned t,bool reset_position);
 void set_tempo(unsigned tempo);void schedule_retrigger(unsigned track);bool seek_song(unsigned position,bool send_midi);
 // A source replacement retires the old sampler instance, while ordinary
 // live parameter edits still reach voices attached to the current instance.
 uint32_t sampler_source_epoch_[kTracks]{};
 void retire_sampler_source(unsigned t){if(++sampler_source_epoch_[t]==0)++sampler_source_epoch_[t];}
 float sampler_speed(unsigned t,Voice& voice,const ModRuntime& modulation);
 bool mount_override(unsigned t,unsigned slot,bool twin=false);
 struct PresetPreview {PresetFxSnapshot before_fx{};TrackKind before_kind=TrackKind::Instrument;bool before_audio=false;SoundState staged{},before{};WavView staged_views[kSampleAssets]{},before_views[kSampleAssets]{};SampleMetadata staged_metadata[kSampleAssets]{},before_metadata[kSampleAssets]{};uint64_t staged_mask=0,active_mask=0;};
 PresetPreview* preset_preview_=nullptr;uint32_t preset_preview_pending_=0;uint8_t preset_preview_track_=255;
 void start_preset_preview(const CommandState&);void stop_preset_preview();

 uint64_t sample_ready_mask_=0,soundfont_ready_mask_=0;
 void bind_sample(unsigned slot,const WavView& sample){samples_[slot]=sample;const uint64_t bit=uint64_t(1)<<slot;if(sample.soundfont)__atomic_fetch_or(&soundfont_ready_mask_,bit,__ATOMIC_RELEASE);else __atomic_fetch_and(&soundfont_ready_mask_,~bit,__ATOMIC_RELEASE);if(sample.frames)__atomic_fetch_or(&sample_ready_mask_,bit,__ATOMIC_RELEASE);else __atomic_fetch_and(&sample_ready_mask_,~bit,__ATOMIC_RELEASE);}

 void refresh_parameters(unsigned t,const StepState* step=nullptr);bool refresh_parameters_step(unsigned t,const StepState* step);
 void refresh_effects();
 void refresh_performance_fx(uint8_t layered);
 bool refresh_group_effects();
 bool recording_position(unsigned track,unsigned velocity,unsigned& cell,uint16_t& timing,bool& use_previous,bool require_armed=true,bool quantized=true);
 // Stage 4 retrospective capture: the last two bars of played
 // notes/kit hits are always retained in this ring; CaptureRetro
 // replays them as ordinary RecordedNote/RecordedKitHit events.
 struct RetroEvent{uint64_t serial=0;uint16_t note=0,timing=0;uint8_t track=0,cell=0,pattern=0,kind=0,velocity=0;};
 RetroEvent retro_[128];unsigned retro_head_=0,retro_count_=0;
 void retro_push(unsigned t,unsigned cell,unsigned pattern,unsigned kind,unsigned note,unsigned velocity,uint16_t timing,uint64_t serial){RetroEvent& e=retro_[retro_head_];e.serial=serial;e.note=uint16_t(note);e.timing=timing;e.track=uint8_t(t);e.cell=uint8_t(cell);e.pattern=uint8_t(pattern);e.kind=uint8_t(kind);e.velocity=uint8_t(velocity);retro_head_=(retro_head_+1)%128;if(retro_count_<128)++retro_count_;}void record_kit_hit(unsigned track,unsigned pad,unsigned velocity);void record_note(unsigned track,unsigned note,unsigned velocity,unsigned channel,bool gated,bool chromatic=false,bool native_drum=false);
 bool erase_record_step(unsigned track,unsigned step,uint64_t serial);
 void finish_recorded_note(unsigned track,unsigned note,unsigned channel);void finish_recorded_channel(unsigned track,unsigned channel);void finish_recorded_all(bool emit);
 struct RepeatPad {bool held=false;uint8_t track=0,note=0,velocity=0;uint16_t channel=65535;uint8_t pattern=0,slice=0,recorded_note=255;uint64_t gate_due=0;bool down=true,chromatic=false;uint8_t kit_pad=255;bool native_drum=false;};RepeatPad repeat_pads_[16*kStepNotes]{},repeat_midi_[16]{};uint32_t repeat_dropped_=0,repeat_index_=0,repeat_random_=0x41c6ce57;uint64_t repeat_phase_=0,repeat_next_gate_=0;uint8_t repeat_mode_=0,repeat_rate_=2;bool repeat_latch_=false;
 void repeat_tick();void finish_repeat_pad(RepeatPad& pad);void refresh_repeat_gate();void clear_latched_notes();void prepare_repeat_input();void replace_repeat_pad(unsigned hardware);
 void refresh_cc(unsigned t);
 void play_step(unsigned t);
 void launch_clip(unsigned t,unsigned pattern,bool playing);
 void launch_scene(unsigned scene);
 void apply_launches();
 uint64_t step_period(unsigned t) const;
 uint64_t step_due(unsigned t) const;
 void advance(unsigned t);
 void update_timing(unsigned t);
 void follow_clip(unsigned track);void reset_clip_follow_seeds();
 void follow_scene();  bool follow_song(bool boundary,bool tick);void apply_song_section(unsigned index,bool force_restart=false);
 bool audio_muted(unsigned t)const{return song_snapshot_?bool(song_mute_mask_&(1u<<t)):track_muted(*project_,t);}
 bool audio_has_solo()const{return song_snapshot_?snapshot_solo_!=0:project_->solo_tracks!=0;}
 bool audio_solo_track(unsigned t)const{return song_snapshot_?bool(snapshot_solo_&(1u<<t)):track_solo(*project_,t);}
 uint16_t captured_mute_mask()const;
 uint16_t audio_lane_mask_=0,audio_pending_=0;uint32_t audio_last_tick_=0xffffffffu,live_record_tick_=0;int8_t audio_clip_[16]{};
 void reset_audio_lanes();void start_audio_clip(unsigned t,unsigned clip,uint32_t offset=0);void tick_audio_lanes(bool boundary);uint32_t audio_song_tick()const;
 uint64_t arrangement_tick_phase_=0,arrangement_tick_=0;uint32_t song_elapsed_ticks_=0,song_duration_ticks_=kSongTicksPerBar;
 uint16_t arrangement_restarts_=0,snapshot_solo_=0;bool arrangement_dirty_=false,song_snapshot_=false;
 bool midi(uint8_t status,uint8_t a=0,uint8_t b=0,uint8_t length=3);
 bool capture_internal(uint8_t status,uint8_t a,uint8_t b,uint8_t length=3);void capture_voice_on(Voice& voice,unsigned track,unsigned velocity);void capture_voice_off(Voice& voice);void capture_pressure(unsigned track,uint8_t value);bool capture_internal_track(unsigned track);bool refresh_expression(unsigned track);ModRuntime mod_runtime(unsigned track) const;
 static bool project_event(EngineEventType type){return type==EngineEventType::SoundAttached || type==EngineEventType::ParameterChanged || type==EngineEventType::ProgramChanged || type==EngineEventType::ClipLaunched || type==EngineEventType::SceneLaunched || type==EngineEventType::GesturePoint || type==EngineEventType::GestureState || type==EngineEventType::TempoChanged || type==EngineEventType::RecordedNote || type==EngineEventType::RecordedKitHit || type==EngineEventType::RecordedGate || type==EngineEventType::RecordErase;}
 void event(EngineEventType type,uint8_t t,uint8_t value,uint8_t flags=0,uint16_t parameter=0,float scalar=0,uint16_t tick=0,uint32_t recording_id=0,uint32_t recording_revision=0) {
  // A display flood must not occupy the space needed for recorded notes,
  // external MIDI edits and transport notifications. Never block the DSP.
  const bool visual=type==EngineEventType::Step || type==EngineEventType::Note;
  if(visual && events_.free_space()<=256){__atomic_add_fetch(&visual_event_dropped_,1,__ATOMIC_RELAXED);return;}
  uint32_t revision=type==EngineEventType::RecordedGate?recording_revision:((type==EngineEventType::RecordedNote || type==EngineEventType::RecordedKitHit) && t<kTracks && flags<kPatterns)?pattern_revisions_->audio[flags][t]:0;
  const bool affects_project=project_event(type);if(affects_project)__atomic_add_fetch(&project_events_pending_,1,__ATOMIC_RELEASE);
  if(!events_.push({type,sample_count_,t,value,flags,parameter,scalar,generation_,tick,recording_id,revision})){
   __atomic_add_fetch(visual?&visual_event_dropped_:&event_dropped_,1,__ATOMIC_RELAXED);
   if(affects_project){__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);__atomic_sub_fetch(&project_events_pending_,1,__ATOMIC_RELEASE);}
  }
 }
 void start_click(bool accent){click_remaining_=click_length_;click_phase_=0;click_increment_=6.2831853f*(accent?1500.0f:1000.0f)/float(rate_);}
 void clock_event(ClockEventType type,bool transmit=true) { if(type==ClockEventType::Tick){if(project_->ui.metronome && metro_ticks_%24==0)start_click(metro_ticks_%96==0);++metro_ticks_;}if(!clock_events_.push({type,sample_count_})) __atomic_add_fetch(&clock_dropped_,1,__ATOMIC_RELAXED); if(transmit && !external_clock_)midi(type==ClockEventType::Start?0xfa:type==ClockEventType::Stop?0xfc:0xf8,0,0,1); }
 struct PatternRevisions {uint32_t published[kPatterns][kTracks]{},audio[kPatterns][kTracks]{};};
 PatternState* pattern_staging_;PatternRevisions* pattern_revisions_;uint32_t pattern_pending_=0,pattern_revision_serial_=0;float effects_target_[13]={0,0.35f,250,0,0.5f,0.5f,0.5f,4,10,120,0,0,0},effects_current_[13]={0,0.35f,250,0,0.5f,0.5f,0.5f,4,10,120,0,0,0};
 CreativeVoice* creative_;uint32_t rate_;float inverse_rate_=0,kit_mix_alpha_=0; ProjectState* project_; ProjectState* staging_;TrackViews track_views_; uint32_t snapshot_pending_=0,snapshot_generation_=0,published_generation_=0,generation_=0,panic_requested_=0;
 uint64_t lfo_transport_beat_=0,lfo_transport_fraction_=0;
 uint64_t stopped_at_=0; MidiClockEstimator external_clock_estimator_{};bool external_clock_=false,external_tempo_pending_=false,seek_armed_=false,transport_resend_=false,restart_pending_=false;
 uint32_t clip_follow_seed_[16]{};
 SceneTake scene_take_{};
 uint32_t bpm_=120,random_=0x13579bdf,meta_random_=0x13579bdf; uint64_t clock_phase_=0,bar_phase_=0,sample_count_=0,count_phase_=0,song_phase_=0;uint16_t song_position_=0;uint32_t metro_ticks_=0,click_remaining_=0,click_length_=0;uint8_t count_beat_=0;bool counting_=false;float click_phase_=0,click_increment_=0; bool note_record_armed_=false;uint8_t note_record_track_=0;uint64_t (*record_erased_)[kSteps];SpscQueue<StepState,65>* erase_payload_;bool playing_=false,first_=false,fill_=false;
 struct RecordedHeld {uint64_t start=0,period=0;uint32_t id=0;uint8_t track=0,note=0;uint16_t channel=0;uint8_t pattern=0,step=0;uint32_t pattern_revision=0;};RecordedHeld recorded_held_[64]{};uint32_t recording_serial_=0;
 struct VaResolved {uint64_t sample=~uint64_t(0);unsigned generation=0;float values[kVirtualAnalogParameterCount]{};};VaResolved va_resolved_[16]{};
 bool insert_resources_ready_=true;float* modulation_storage_=nullptr;WavView samples_[kSampleAssets]{},staged_samples_[kSampleAssets]{},pattern_samples_[kSampleAssets]{};SampleMetadata pattern_sample_metadata_[kSampleAssets]{};bool pattern_assets_=false,staged_assets_=false; TrackRuntime runtime_[16]{}; Voice voices_[16]{}; uint8_t sample_zone_round_robin_[16]{}; GranularVoice* granular_=nullptr;TextureVoice* textures_=nullptr;bool textures_suspended_=false;TextureStereo texture_previous_[61]{},texture_next_[61]{};ResonatorVoice* resonators_=nullptr;PlaitsVoice* plaits_=nullptr;PlaitsVoice* poly_plaits_=nullptr; DaisyDrumBank* public_drums_=nullptr;
 ParameterSnapshot morph_staging_[2][kTracks]{};uint8_t morph_stage_pending_[2]{};uint16_t control_refresh_mask_=0;uint8_t control_refresh_cursor_=0;uint64_t effects_refresh_due_=0;
 int8_t pending_pattern_[16]{}; int8_t pending_scene_=-1; bool chain_armed_=true; uint8_t chain_position_=0,chain_bars_=0; bool song_armed_=true,song_finished_=false; uint8_t song_section_=0; uint16_t song_bars_=0,song_section_bars_=1; int8_t song_transpose_=0; uint16_t song_mute_mask_=0; uint16_t pending_stop_=0,pending_midi_off_=0,panic_channels_=0,expression_resend_mask_=0;
 struct InputAudio {float l,r;};SpscQueue<InputAudio,4097> input_audio_;unsigned input_dropped_=0; float diagnostic_phase_=0,diagnostic_gain_=0.05f;unsigned diagnostic_remaining_=0;
 WavVoice preview_;float preview_gain_=1;uint32_t preview_serial_=0,preview_ticket_=0,preview_state_=0;
 PreparedSoundQueue* attached_payload_;SoundState* attached_consumer_;
 SpscQueue<LiveRecordFrame,kLiveRecordCapacity+1>* live_record_queue_;uint32_t live_record_highwater_=0;uint32_t live_record_state_=0,live_record_failed_=0,live_record_conflict_=0,live_record_remaining_=0,live_record_input_drops_=0,live_record_bar_done_=0;uint8_t live_record_bars_=0;bool live_record_bounded_=false;uint8_t live_record_source_=0,live_record_restart_track_=255,live_record_restart_pattern_=255;
 StereoDelay delay_;
 // O-06 idle gates for the master send effects: an output-peak
 // envelope per processor; the processor sleeps (and is reset on
 // wake, like PerformanceThrows) once its input is quiet and the
 // envelope has decayed below audibility.
 float delay_env_=0,reverb_env_=0;bool delay_sleep_=true,reverb_sleep_=true;uint32_t cc_generation_=0; StereoReverb reverb_; PerformanceThrows performance_throws_; StereoCompressor compressor_; StereoCompressor track_compressors_[16]; StereoSafetyLimiter main_limiter_,cue_limiter_; MasterCapture captures_[2],performance_history_; PerformanceFxBus performance_fx_; PerformanceFx performance_fx_applied_=PerformanceFx::Off,performance_fx_pending_mode_=PerformanceFx::Off,performance_fx_manual_mode_=PerformanceFx::Off,performance_fx_auto_mode_=PerformanceFx::Off; float performance_fx_pending_amount_=0,performance_fx_auto_amount_=0.5f,performance_fx_manual_amount_=0; uint8_t performance_fx_quantize_pending_=0; BeatRepeat beat_repeat_; unsigned capture_active_=0,capture_frozen_=1; uint32_t capture_state_=0;// Only the audio owner touches the runtime below.
 uint8_t freeze_state_=0,freeze_track_=0,freeze_pattern_=0;int16_t* freeze_buffer_=nullptr;uint32_t freeze_capacity_=0,freeze_frames_=0,freeze_estimate_=1;bool freeze_write_=false,freeze_audio_active_=false;
 // Storage and request are task-owned while idle and immutable until an
 // audio completion/abort acknowledgement. Only status/cancel/progress cross
 // owners concurrently; no pointer is freed merely because cancel was sent.
 int16_t* freeze_storage_=nullptr;uint32_t freeze_storage_frames_=0,freeze_request_generation_=0;unsigned freeze_request_track_=0,freeze_status_=0,freeze_cancel_=0,freeze_published_frames_=0,freeze_published_estimate_=1;bool freeze_release_pending_=false;
 struct TwinHeld {uint64_t off=0;uint16_t channel=65535;uint8_t note=60,velocity=100;bool sequenced=false;};
 TwinHeld twin_held_[kPolyVoiceCapacity+1]{};unsigned twin_held_count_=0;
 TwinInstall* twin_install_=nullptr;uint16_t twin_pending_=0;float twin_last_l_[16]{},twin_last_r_[16]{},twin_tail_l_[16]{},twin_tail_r_[16]{};uint8_t twin_fade_[16]{};
 float* twin_storage_=nullptr;bool twin_capture_request_=false,twin_isolated_=false,twin_heard_=false;uint8_t twin_note_=60,twin_velocity_=100;uint32_t twin_hold_frames_=0,twin_quiet_=0,twin_capture_revision_=0;
 bool capture_buffer_begin(unsigned,uint32_t,bool,unsigned=60,unsigned=100,unsigned=0);
 bool twin_capture_allowed(unsigned);void stop_twin_voice(unsigned);void twin_capture_frame(float,float);void apply_twin_switch(unsigned);void collect_twin_held(unsigned);void retrigger_twin_held(unsigned);
 void consume_freeze_capture();void publish_freeze_capture();
 bool gesture_recording_=false,gesture_playing_=false,gesture_replay_=false;uint64_t gesture_phase_=0;unsigned gesture_cursor_=0;float feedback_input_=0,follower_master_=0,follower_[16]{},follower_next_[16]{},follower_amount_[16]{};
 bool stutter_=false,control_all_=false; uint8_t stutter_subdivision_=4; uint16_t control_all_id_=3; float stutter_wet_=0,control_all_delta_=0;
 GroupRoutingCache group_routing_cache_;
 struct LocalMidiNote {bool held=false;uint8_t channel=0,note=0;bool releasing=false;};LocalMidiNote local_midi_[16][kStepNotes]{};
 void release_local_midi(unsigned pad);void release_local();
 float input_gain_=1,input_gain_target_=1;uint8_t tuner_source_=0;float tuner_ring_[1024]{},tuner_sum_=0;unsigned tuner_write_=0,tuner_div_=0;float input_peaks_[2]{};
 float visual_peak_[16]{},visual_wave_[16][64]{},visual_master_peak_=0,visual_input_peak_=0,visual_delay_input_=0,visual_cue_peak_=0,visual_limit_main_=0,visual_limit_cue_=0,visual_master_wave_[64]{};bool visual_invalid_audio_=false;uint64_t visual_next_=0;float record_wave_[512]{};unsigned record_write_=0,record_column_count_=0;uint64_t record_columns_total_=0;float record_peak_=0;
 uint32_t visual_control_request_[8]{};
 unsigned visual_requested_track_=0xff00,visual_tap_track_=0,visual_tap_source_=255;uint32_t visual_tap_generation_=0;float visual_taps_[3][512]{},visual_nodes_[6]{};
 SpscQueue<AudioVisualFrame,3> visual_frames_;AudioVisualFrame visual_build_{};unsigned visual_build_track_=0;bool visual_build_active_=false;
 struct TimedMidi {MidiPacket packet{};uint8_t track=0;uint32_t generation=0,epoch=0;};SpscQueue<TimedMidi,257> timed_midi_;uint64_t last_timed_=0;uint32_t midi_epoch_=0,last_timed_generation_=0,last_timed_epoch_=0;
 SpscQueue<CommandState,256>* commands_;SpscQueue<SoundState,65>* sound_commands_;SoundState* sound_command_consumer_;SpscQueue<TrackMorphState,65>* morph_commands_;TrackMorphState* morph_command_consumer_; SpscQueue<ClockEvent,256> clock_events_; SpscQueue<MidiEvent,2048> midi_events_;SpscQueue<MidiCaptureEvent,2049>* midi_capture_events_;bool midi_capture_active_=false,midi_capture_internal_=false;uint16_t capture_bend_[16]{};uint8_t capture_pressure_[16]{};uint32_t midi_capture_failed_=0; SpscQueue<QueuedEngineEvent,1024> events_;
 uint32_t clock_dropped_=0,midi_dropped_=0,event_dropped_=0,visual_event_dropped_=0,project_event_loss_=0,project_events_pending_=0;
 // Append helper metadata without changing the established voice/runtime
 // strides or offsets of the existing signal-path members.
 struct VaCandidate {Voice* voice=nullptr;unsigned key=0;};
 VaSampleSlot* va_helper_=nullptr;VaCandidate va_candidates_[16+kPolyVoiceCapacity]{};unsigned va_candidate_count_=0;
 uint64_t va_job_sample_[16+kPolyVoiceCapacity]{};uint8_t va_job_[16+kPolyVoiceCapacity]{};
 PolyStealCounts poly_steals_{};
 uint64_t sound_payload_epoch_=1;bool sound_payload_plan_dirty_=true;
 void prepare_sound_payloads();
 RenderAudit::EventCounts event_work_{}; // append: retain voice/runtime offsets
 bool va_candidates_dirty_=true; // DSP-only; live eligibility is still checked per sample
 void prepare_va_candidates();void dispatch_va_samples();
};
}
