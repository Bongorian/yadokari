#pragma once
#include "sequence_operations.h"
#include "audio_controls.h"
#include "transfer_view.h"
#include "channel_controls.h"
#include "device_settings.h"
#include "input_tuner.h"
#include "engine.h"
#include "project.h"
#include "backends.h"
#include "instrument_registry.h"
#include "engine_pages.h"
#include "mk3_surface.h"
#include "ui/pad_feedback.h"
#include "browser_state.h"
#include "hardware_diagnostics.h"
#include "scales.h"
#include "recovery_source.h"
#include "ui_axes.h"
#include "composition_controls.h"
#include "ui/modal_state.h"
#include "modal_controls.h"
#include "sequence_commit.h"
#include "common_top_controls.h"
#include "status_controls.h"
#include "song_controls.h"
#include "clip_controls.h"
#include "synth_controls.h"
#include "recording_controls.h"
#include "diagnostics_controls.h"
#include "default_controls.h"
#include "scene_controls.h"
#include "mixer_controls.h"
#include "input_controls.h"
#include "library_controls.h"
#include "note_transform.h"
#include "midi_import.h"
#include "settings_controls.h"
#include "generate_controls.h"
#include "project_controls.h"
#include "modulation_controls.h"
#include "step_controls.h"
#include "browser_controls.h"
#include "sampler_controls.h"
#include "effects_controls.h"
#include "routing_fx_controls.h"
#include "perform_controls.h"
#include "ui/toast.h"
#include "overlay_controls.h"
namespace groove {
enum class UiNotice:uint8_t { None, PartialApplied, Saved, SavedLocationFailed, SampleRecorded, MidiSaved, MidiImported, SaveFailed, HostUpdateTools, HostFileTools, FilesBusy, Loaded, KitLoaded, NewProject, Failed, Copied, LocksFull, GestureFull, GestureClear, LoopConfirm, LoopReady, LoopInvalid, OverdubDone, OverdubFailed, ChordFull, ChordMono, RecordLost, ChordRepeat, ChordLiveMono, GrooveClipped, ScaleRequired, LoopImported, LoopUnsupported, LoopPrecision, ProjectUnsynced, ProjectPending, SaveQueued, Saving, SnapshotSaved, Recovered, SnRefConfirm, SnRefQueued, StripUnavailable, StepCopied, StepPasted, StepRhythmPasted, ChopPhrase, KitPadSwapped, ExtendConfirm, Extended, SceneStored, SceneWait , MemoryFull, AssetReload, RestartQueued, HintAvailable, ChordVoiceLimit, ControllerReconnect, ModuleEntered, ModuleExited, ModuleTransport, LayerRefused, TwinChanged, TwinFailed, CompositionChanged, CompositionFailed};
enum class UiMode : uint8_t { Pad, Step, Clip, Scene, Mixer, Synth, Effects, Perform, Generate, Keyboard, Sampler, Project, Settings, Modulation, Browser, Routing, Diagnostics, Chord, Song, Library };
inline const char* mode_name(UiMode mode) { static const char* names[]={"PAD","STEP","CLIP","SCENE","MIXER","SYNTH","FX","PERFORM","GENERATE","KEYBOARD","SAMPLER","PROJECT","SETTINGS","MODULATION","BROWSER","ROUTING","DIAGNOSTICS","CHORDS","SONG","PRESETS"}; return names[unsigned(mode)]; }
// Single UI producer. Accepted commands and project edits move together.
class Performance {
 LayerReason notice_layer_reason_=LayerReason::Ready;
 ModalState modal_;
 ui::FileToast file_notice_;
 bool help_down_=false,help_shown_=false;UiMode help_mode_=UiMode::Pad;uint64_t help_since_=0;uint32_t help_seen_=0;
 MidiImportRequest midi_import_request_{};uint32_t midi_import_ticket_=0,midi_import_cancel_ticket_=0;
 ModalField<uint8_t> stretch_modal_{modal_,ModalOwner::Stretch};
 ModalField<uint8_t> composition_modal_{modal_,ModalOwner::Composition};
 ModalField<uint8_t> twin_modal_{modal_,ModalOwner::Twin};
 ModalField<uint8_t> freeze_modal_{modal_,ModalOwner::Freeze};
 ModalField<bool> source_arm_unknown_{modal_,ModalOwner::SourceDiscard};
 ModalField<bool> sn_ref_armed_{modal_,ModalOwner::Reference};
 ModalField<bool> song_clear_armed_{modal_,ModalOwner::SongClear};
 ModalField<bool> song_import_armed_{modal_,ModalOwner::SongImport};
 ModalField<bool> extend_pending_{modal_,ModalOwner::Extend};
 ModalField<bool> loop_confirm_{modal_,ModalOwner::Loop};
 ModalField<bool> update_confirm_{modal_,ModalOwner::Update};
 UiMode track_name_return_mode_=UiMode::Pad;uint8_t track_name_target_=0;uint32_t track_name_generation_=0;
 PresetGuidIssuer preset_guids_;uint64_t asset_clone_pending_=0;uint8_t asset_clone_alias_[kSampleAssets]{};
 void mirror_track_install(const TrackInstall&,unsigned);
 struct TrackHistory : TrackState {TrackInstall install{};TrackMorphState morph{};bool phrase_only=false;explicit TrackHistory(const ConstTrackView& t):TrackState(t){}};
 struct History {Command before{},after{};uint32_t group=0;PatternState* pattern_before=nullptr;PatternState* pattern_after=nullptr;TrackHistory* track_before=nullptr;TrackHistory* track_after=nullptr;};
 struct RotationRevision {uint32_t from=0,to=0;uint16_t length=0;uint8_t pattern=0,track=0;int8_t direction=0;};
 static constexpr uint32_t kRotationHistory=0x524f5441u;
 struct NavigationScope {Performance& ui;explicit NavigationScope(Performance& p):ui(p){if(!ui.navigation_depth_++)ui.save_navigation();}~NavigationScope(){if(!--ui.navigation_depth_)ui.record_navigation();}};
 bool step_chord_edit_=false;UiMode step_chord_origin_=UiMode::Step;
 TransferView transfer_view_{};bool transfer_request_=false,transfer_close_=false,transfer_start_=false,transfer_browse_=false;
 unsigned navigation_depth_=0;bool home_navigation_pending_=false;
public:
 enum class UiErase:uint8_t {SongDelete=1,AudioDelete,AudioClear,MapPads,ChopSteps,AutoChop,LayerRemove,GestureClear,ZonesClear,FileLoop};
 bool confirm_erase(UiErase action);
 const char* confirmation_heading()const;
 bool erase_confirmation()const{return modal_.owner==ModalOwner::UiCommand;}
 const char* erase_title()const;
 bool erase_audio(bool all){auto a=audio_lane();if(all)for(auto& clip:a.clips)clip=AudioClip{};else a.clips[audio_clip_cursor_]=AudioClip{};return set_audio_lane(selected_,a);}
 bool accept_erase();

 explicit Performance(Engine& engine,ProjectState& project):engine_(engine),project_(project),track_views_(project) { modal_.axis=&axes_.modal;project_loaded(); }
 bool dispatch(const ControllerInput& input) ;
 bool multitimbral()const{return engine_.multitimbral();}
 bool set_multitimbral(bool on);
 LayerReason layer_warning(bool runtime=true)const;
 bool module_transport_blocked(){if(!multitimbral())return false;copy_text(browser_.status,sizeof browser_.status,"SETTINGS > MIDI > MULTITIMBRAL: OFF, then PLAY");notify(UiNotice::ModuleTransport);return true;}
 bool play() ;
 bool restart_transport(bool quantized=false);
 bool start_count_in_record();
 bool seek_song(unsigned position);
 unsigned song_position()const{return playing_ && visual_.playing?visual_.song_position:song_position_;}
 bool stop() ;
 bool panic();
 uint32_t navigation_key()const;
 NavigationLocation navigation()const;
 void save_navigation(){if(!axes_.history.empty() && axes_.history.current().key==navigation().key)axes_.history.replace_current(navigation());}
 void record_navigation();
 void restore_navigation(const NavigationLocation& n);
 void set_mode(UiMode mode) {NavigationScope scope(*this);if(modal_active())return;if(mode!=mode_)ticket_valid_=false;apply_mode(mode);}
 void apply_mode(UiMode mode) ;
 UiMode mode() const { return mode_; }
 // Record the resolved page, including audio-track redirects. apply_mode
 // synchronizes grammar metadata for direct, history and HOME navigation;
 // actual pad dispatch remains page-specific.
 const UiAxes& axes() const { return axes_; }
 PadMode pad_mode() const { return axes_.pad_mode; }
 void set_focus(Focus focus) { axes_.focus=focus; }
 bool page_back();
 bool page_forward();
 // SPEC 2.4/2.6: PAGE both-buttons long-press is HOME (from any page)
 // and RETURN (on Pad). The ticket lives outside the EditPage history
 // so the return cost never grows with navigation depth. It is
 // discarded by RETURN itself, by set_mode to another page, and by a
 // project load. While a project confirm (the Modal that owns input)
 // is pending, HOME does not fire.
 bool home_or_return() ;
 bool return_ticket_valid() const { return ticket_valid_; }
 // PAGE edge handling: a lone press fires Back/Forward after a short
 // grace period (or on release, for quick taps); a press that lands
 // while the other PAGE button is held is a HOME candidate instead
 // and never fires its own navigation. A lone long-press fires only
 // the navigation, never HOME (SPEC 2.6).
 void page_button_edge(bool right,bool down) ;
 void service_page_buttons() ;
 // Wave 5 item 2: STEP long-press enters note edit (the door EVENTS
 // used to own). A plain press keeps its behaviour: it opens the Step
 // page with note edit off.
 void step_button_edge(bool down) ;
 void cycle_step_view();
 bool track_shortcut(unsigned slot,bool down);
 void service_track_shortcut();
 unsigned track_bank()const{return track_bank_;}
 void service_step_button() ;
 uint64_t assignment_revision(unsigned pattern,unsigned track)const{return pattern<kPatterns && track<kTracks?assignment_revision_[track]:0;}
 bool playing() const { return playing_; }bool counting_in()const{return counting_in_;}unsigned count_beat()const{return count_beat_;}
 void audio_available(bool ready){audio_available_=ready;}bool audio_available()const{return audio_available_;}
 void live_recording(bool on,uint8_t source=255,const RecordingTarget* target=nullptr,bool quantized=false,RecordingOrigin origin=RecordingOrigin::None);bool live_recording()const{return live_recording_;}uint8_t live_record_source()const{return live_record_source_;}bool note_recording()const{return note_recording_;}unsigned note_record_track()const{return note_record_track_;}bool arm_note_record(bool on);void resampling(bool on,const ResampleRequest* request=nullptr);
 void recording_file_busy(bool on){if(recording_file_busy_!=on){recording_file_busy_=on;dirty_=true;}}
 void recording_cancel_pending(bool on){if(resample_cancel_pending_!=on){resample_cancel_pending_=on;dirty_=true;}}
 bool recording_file_busy()const{return recording_file_busy_;}bool resample_cancel_pending()const{return resample_cancel_pending_;}
 void recording_stop_pending(bool on){if(record_stop_pending_!=on){record_stop_pending_=on;dirty_=true;}}
 bool record_stop_requested()const{return record_stop_request_;}
 bool recording_request_pending()const{return !live_recording_ && (file_action_==FileAction::SampleRecordToggle || file_action_==FileAction::RecordToggle || file_action_==FileAction::Resample);}
 const LiveRecordRequest& live_record_request()const{return queued_record_;}
 RecordingOrigin recording_origin()const;
 bool recording_quantized()const;
 RecordingTarget recording_target()const;
 uint32_t queued_audio_revision()const{return queued_audio_revision_;}unsigned queued_audio_clip()const{return queued_audio_clip_;}
 const LiveRecordRequest& take_request()const{return queued_take_;}
 const ResampleRequest& resample_request()const{return queued_resample_;}
 ResampleRequest resample_view()const;
 unsigned recording_source()const;
 RecordingTarget current_recording_target()const;
 RecordingContext recording_controls()const;
 const char* recording_status()const{return recording_hint(recording_controls(),record_state());}
 const char* sampler_recording_status()const{return sampler_recording_hint(recording_controls(),record_state());}bool resampling()const{return resampling_;}unsigned resample_bars()const{return resample_bars_;}bool record_quantized()const{return record_quantized_;}unsigned record_state()const{return engine_.live_record_state();}
 void record_destination(unsigned& track,unsigned& slot,int& kit_pad) const ;
 bool shifted() const { return (controls_[0]&0x40)!=0; }
 // SPEC 2.6: parameter locks have two creation paths with the same
 // target - Shift + knob, and holding a step pad while turning the
 // knob. Pressing a step pad selects that step, so both paths write
 // the selected step's lock.
 bool lock_path() const { return shifted() || held_step_hardware_!=255; }
 // While a step pad is held in Step view, the knob band presents
 // the track's macro parameters for lock editing (SPEC 2.6 hold
 // path); the normal step-field knobs return on release.
 bool step_attribute_knob(unsigned i)const{return mode_==UiMode::Step && shifted() && !step_hold_edit() && track_const(selected_).instrument!=Instrument::DrumKit && (i==2 || i==3);}
 bool step_hold_edit() const { return mode_==UiMode::Step && held_step_hardware_!=255; }
 ParameterTarget held_model_target(unsigned index)const;
 void toggle_edit() { set_mode(mode_==UiMode::Step?UiMode::Pad:UiMode::Step); }
 void set_shift(bool shift) { controls_[0]=uint8_t((controls_[0]&~0x40)|(shift?0x40:0)); }
 bool audio_page()const{return mode_==UiMode::Perform && performance_page_==5;}
 unsigned audio_clip_cursor()const{return audio_clip_cursor_;}
 const AudioLane& audio_lane()const{return project_.audio[selected_];}
 uint32_t audio_revision(unsigned t)const{return t<16?audio_revision_[t]:0;}
 bool set_audio_lane(unsigned t,const AudioLane& a,bool history=true,unsigned slot=255,WavView view={},SampleMetadata metadata={});
 bool undo_audio(){if(!audio_undo_valid_[selected_])return false;auto previous=audio_undo_[selected_];return set_audio_lane(selected_,previous);}
 bool audio_key(unsigned index);
 float audio_knob_value(unsigned i)const;
 bool audio_knob(unsigned i,int delta);
 ui::ControlDescriptor<CommonTopAction> common_top(unsigned index)const;
 bool common_top_key(unsigned index);
 const char* page_title()const;
 unsigned top_slot(unsigned index)const;
 bool input_values()const{return !repeat_mode_ && !browser_editing() && !modal_active() && (mode_==UiMode::Keyboard || mode_==UiMode::Chord);}
 bool input_top(unsigned i)const;
 bool step_chord_editor()const{return step_chord_edit_ || (mode_==UiMode::Keyboard && shifted());}
 int keyboard_note(unsigned pad)const{return keyboard_input(track_const(selected_),pad,project_.ui.keyboard_octave[selected_]);}
 int keyboard_octave()const{return project_.ui.keyboard_octave[selected_];}
 bool input_scale(int delta);
 bool input_root(int note);
 void release_input_pads(){for(unsigned i=0;i<16;++i)if(notes_held_[i]){pending_release_[i]=true;held_[i]=false;pressures_[i]=0;}retry_releases();clear_pad_feedback();}
 bool input_octave(int delta);
 bool input_shape(int delta,bool inversion);
 bool input_key(unsigned i);
 uint32_t newer_preset_revision[kTracks]{};bool embedded_preset_fallback[kTracks]{};
 bool take_asset_clone(unsigned& to,unsigned& from);
 bool open_preset_library(bool kits=false);
 bool inspect_source_library();
 bool select_source(unsigned engine);
 bool move_preset_library(int delta);
 bool move_browser(int delta);
 bool library_catalog_controls()const{return mode_==UiMode::Library && !modal_active() && !browser_editing();}
 ui::ControlDescriptor<LibraryAction> library_catalog_button(unsigned i)const;
 bool library_catalog_key(unsigned i);
 ui::ControlPresentation soft_presentation(unsigned i)const;
 bool soft_enabled(unsigned i)const;
 bool soft_selected(unsigned i)const;
 bool soft_key(unsigned i);
 ui::ControlPresentation page_top_presentation(unsigned index)const ;
 const char* soft_label(unsigned index)const{return soft_presentation(index).label;}
 uint8_t soft_icon(unsigned index)const{return soft_presentation(index).icon;}
 StatusControl status_control(unsigned index)const;
 StatusControl family_control()const{return engine_family_control(source_select_ && mode_==UiMode::Synth?Instrument(source_cursor_):track_const(selected_).instrument);}
 bool page_top_enabled(unsigned index)const;
 bool page_top_selected(unsigned index)const;
 bool page_top_key(unsigned index);
 uint8_t knob_overlay_mask()const;
 unsigned knob_turning_mask()const{unsigned mask=0;for(unsigned i=0;i<8;++i)if(knob_turn_until_[i] && now_ms_<knob_turn_until_[i])mask|=1u<<i;return mask;}
 bool knob_overlay()const{return knob_overlay_mask()!=0;}
 bool knob_touched(unsigned index)const{return index<8 && (controls_[9]&(0x80>>index));}
 bool modifier(uint8_t bit,bool on){return control(5,uint8_t((controls_[4]&~bit)|(on?bit:0)));}
 bool control_all()const{return control_all_;}
 uint16_t control_all_parameter()const{return control_all_parameter_;}
 bool wave_profile()const{return (project_.ui.wave_profile_mask & uint16_t(1u<<selected_)) && track_const(selected_).instrument==Instrument::ExternalMidi;}
 bool wave_clock()const{return wave_profile() && (project_.ui.wave_clock_mask & uint16_t(1u<<selected_));}
 unsigned wave_ref_target()const{return wave_ref_[selected_];}
 bool sn_ref_confirmation()const;
 int wave_control(unsigned knob)const{if(!wave_profile() || mode_!=UiMode::Synth || knob<2)return -1;if(wave_clock())return knob<8?int(knob+2):-1;return knob<6?int(knob-2):-1;}
 static unsigned wave_max(unsigned i){return i==4?7:i==9?1:127;}
 // Project CC state is authoritative across pattern switches, undo and reload.
 // Shift edits the selected step lock without replacing the saved base value.
 unsigned wave_value(unsigned item)const;
 bool set_wave_profile(bool on);
 float control_all_position()const{return control_all_position_;}
 bool set_control_all(bool on);
 bool select_control_all_target(int delta,bool category=false);
 bool reset_control_all_offset();
 bool mute_or_solo(unsigned t,bool solo,bool pattern_layer=false);
 bool select_track(unsigned index) ;
 unsigned selected_kit_pad()const{return selected_kit_pad_;}
 unsigned selected_sample_slot()const{const auto& t=track_const(selected_);return t.instrument==Instrument::DrumKit?t.kit.pads[selected_kit_pad_].sample[0]:t.sample_slot;}
 bool select_held()const{return (controls_[4]&0x08)!=0;}
 bool select_kit_pad(unsigned pad){if(pad>=16)return false;selected_kit_pad_=uint8_t(pad);refresh_steps();dirty_=true;return true;}
 unsigned capture_source()const{return project_.routing.capture_bus;}
 bool select_capture_source(int delta,bool track_only=false);
 bool set_kit_pad(unsigned pad,const KitPadState& next,WavView view={},const SampleMetadata* metadata=nullptr){
  return set_kit_pad_at(selected_,project_.active_pattern,pad,next,view,metadata);
 }
 bool set_kit_pad_at(unsigned track_index,unsigned pattern,unsigned pad,const KitPadState& next,WavView view={},const SampleMetadata* metadata=nullptr);
 bool set_kit_step(unsigned pad,unsigned step,const KitStepState& next);
 bool select_pattern(unsigned index) ;
 bool tracker_view()const{return tracker_view_;}bool step_overview()const{return step_overview_;}unsigned creative_lane()const{return creative_lane_;}unsigned creative_cell()const{return creative_cell_;}unsigned tuning_note()const{return tuning_note_;}
 float morph_position()const{return visual_.gesture_morph?visual_.morph_position:project_.morph_position;}
 bool gesture_recording()const{return gesture_recording_;}bool gesture_playing()const{return gesture_playing_;}
 bool gesture_record(bool on);
 bool gesture_play(bool on);
 bool creative(const CreativeState& c);
 bool mutation_set(uint32_t seed,unsigned amount,bool history);
 bool mutation_generate(){uint32_t seed=track_const(selected_).mutation.seed;next_random(seed);if(!seed)seed=0x5b4d3c21;return mutation_set(seed,track_const(selected_).mutation.amount,true);}
 bool mutation_freeze();
 bool mutation_commit();
 bool dice();
 bool rotate_track(int direction);
 void step_page(unsigned page) {if(mode_==UiMode::Step && tracker_view_)follow_display_=false; page_=uint8_t(page%16);selected_step_=uint8_t(page_*16+(selected_step_%16));refresh_steps();dirty_=true; }
 unsigned page() const { return page_; }
 unsigned selected_step() const { return selected_step_; }
 unsigned position(unsigned t) const { return t<16?positions_[t]:0; }
 int pending_scene()const{return queued_scene_;}
 bool scene_matches_live(unsigned scene)const;
 bool pending_stop(unsigned t)const{return t<16 && (queued_stops_&(1u<<t));}
 int pending_clip(unsigned t) const { return t<16?pending_[t]:-1; }
 void accept_event(const EngineEvent& input) ;
 BrowserState& browser(){return browser_;}const BrowserState& browser()const{return browser_;}const SampleMetadata& preview_metadata()const{if(browser_.preview_path[0]){const auto& track=track_const(selected_);unsigned slot=track.instrument==Instrument::DrumKit?track.kit.pads[selected_kit_pad_].sample[0]:track.sample_slot;if(slot<kSampleAssets){const auto& selected=project_.assets[slot];if(selected.path[0] && !__builtin_strcmp(selected.path,browser_.preview_path))return selected.metadata;}for(const auto& asset:project_.assets)if(asset.path[0] && !__builtin_strcmp(asset.path,browser_.preview_path))return asset.metadata;}return browser_.metadata;}bool midi_browser()const{return browser_.cursor<browser_.count && !browser_.entries[browser_.cursor].directory && is_midi_file(browser_.entries[browser_.cursor].name);}bool other_preview_target()const;bool previewing_displayed()const{return browser_.previewing && browser_.preview_path[0] && !__builtin_strcmp(browser_.preview_path,browser_.audition_path);}bool sample_auditioning()const;
 HardwareDiagnostics& diagnostics(){return diagnostics_;}const HardwareDiagnostics& diagnostics()const{return diagnostics_;}
 unsigned diagnostic_offset()const{return diagnostic_offset_;}
 unsigned diagnostic_page()const{return 0;}unsigned diagnostic_test()const{return diagnostic_test_;}unsigned sampler_page()const{return sampler_page_;}
 void midi_capture_state(unsigned state){if(midi_capture_state_!=state){midi_capture_state_=uint8_t(state);dirty_=true;}}unsigned midi_capture_state()const{return midi_capture_state_;}
 void midi_capture_path(const char* path);const char* midi_capture_path()const{return midi_capture_path_;}
 uint8_t stretch_modal()const{return stretch_modal_;}uint16_t stretch_ratio_pct()const{return stretch_ratio_pct_;}unsigned stretch_progress_pct()const{return stretch_progress_;}const char* stretch_text()const{return stretch_text_;}
 void stretch_set_modal(uint8_t m){if(stretch_modal_!=m){stretch_modal_=m;stretch_format();dirty_=true;}}
 void stretch_progress(unsigned pct){unsigned v=pct>100?100:pct;if(stretch_progress_!=uint8_t(v)){stretch_progress_=uint8_t(v);stretch_format();dirty_=true;}}
 void stretch_format();
 uint8_t stretch_progress_=0;uint16_t stretch_ratio_pct_=150;char stretch_text_[40]="Ratio x1.50 - START / CANCEL";
 uint8_t freeze_modal()const{return freeze_modal_;}unsigned freeze_progress_pct()const{return freeze_progress_;}const char* freeze_text()const{return freeze_text_;}
 void freeze_set_modal(uint8_t m){if(freeze_modal_!=m){freeze_modal_=m;freeze_format();dirty_=true;}}
 void freeze_progress(unsigned pct){unsigned v=pct>100?100:pct;if(freeze_progress_!=uint8_t(v)){freeze_progress_=uint8_t(v);freeze_format();dirty_=true;}}
 void freeze_format();
 uint8_t freeze_progress_=0;char freeze_text_[40]="Capturing 00% - CANCEL stops";
 bool preset_fx_editing()const;
 bool browser_editing()const{return browser_.edit.operation!=BrowserOperation::None;}
 const char* project_path()const{return project_path_;}
 void project_path(const char* path){project_unnamed_=false;copy_text(project_path_,sizeof project_path_,path);dirty_=true;}
 void new_project_name(){project_unnamed_=true;dirty_=true;}
 const char* project_name()const{if(project_unnamed_)return "Untitled";const char* name=project_path_;for(const char* p=name;*p;++p)if(*p=='/')name=p+1;return *name?name:"Untitled";}
 bool open_project_browser();
 bool begin_project_save_as();
 enum class BakeStage:uint8_t {Off,Capture,Trim,Tune,Name,Collect,Done};
 BakeStage bake_stage_=BakeStage::Off;unsigned bake_track_=0,bake_generation_=0;uint64_t bake_revision_=0;char bake_path_[160]{};
 bool bake_current()const{return bake_stage_!=BakeStage::Off && bake_track_==selected_ && bake_generation_==engine_.staged_generation() && bake_revision_==assignment_revision_[selected_];}
 TwinConfig twin_config_{};bool twin_switch_pending_[16]{};unsigned twin_pct_=0,twin_note_index_=0,twin_note_count_=0;char twin_message_[96]="DRY capture / source stays intact";
 mutable TwinStatus twin_cached_=TwinStatus::Empty;mutable unsigned twin_cached_track_=16;mutable uint64_t twin_checked_ms_=0,twin_checked_edit_=uint64_t(-1);
 const TwinConfig& twin_config()const{return twin_config_;}TwinConfig& twin_config(){return twin_config_;}
 unsigned twin_dialog()const{return twin_modal_;}unsigned twin_progress()const{return twin_pct_;}unsigned twin_note_index()const{return twin_note_index_;}unsigned twin_note_count()const{return twin_note_count_;}const char* twin_message()const{return twin_message_;}
 void twin_invalidate(){twin_cached_track_=16;dirty_=true;}
 TwinStatus twin_status(unsigned track=16)const;
 bool twin_button();bool twin_setup();void twin_begin_ui(){twin_modal_=2;twin_pct_=0;dirty_=true;}
 void twin_end_ui(const char* message){twin_modal_=0;copy_text(twin_message_,96,message);copy_text(browser_.status,sizeof browser_.status,message);twin_invalidate();}
 void twin_progress_ui(unsigned index,unsigned count,unsigned percent){if(twin_note_index_!=index || twin_note_count_!=count || twin_pct_!=percent){twin_note_index_=index;twin_note_count_=count;twin_pct_=percent;dirty_=true;}}
 bool twin_queued()const{return twin_switch_pending_[selected_];}
 bool bake_control()const;
 const char* bake_hint()const;
 const char* bake_next_label()const;
 bool begin_bake();
 void bake_recorded(unsigned track,bool assigned);
 void bake_saved(const char* path);
 const char* bake_collect_path()const{return bake_current() && bake_stage_==BakeStage::Collect?bake_path_:nullptr;}
 void bake_collected(bool ok){if(ok && bake_current() && bake_stage_==BakeStage::Collect){bake_stage_=BakeStage::Done;dirty_=true;}}
 bool bake_next();
 bool begin_preset_save(bool duplicate=false);
 bool begin_track_rename();
 bool rename_track(unsigned,const char*);
 bool preset_save_target_current()const{return preset_save_track_<kTracks && preset_save_generation_==engine_.staged_generation() && preset_save_revision_==assignment_revision_[preset_save_track_];}
 unsigned preset_save_track()const{return preset_save_track_;}
 void confirm_preset_overwrite(const char* path);
 bool browser_confirmation()const{return browser_.edit.operation==BrowserOperation::Delete || browser_.edit.operation==BrowserOperation::OverwritePreset;}
 void cancel_browser_edit(){bool naming=browser_.edit.operation==BrowserOperation::RenameTrack;browser_.edit.operation=BrowserOperation::None;axes_.modal=ModalKind::None;main_encoder_mode_=0;dirty_=true;if(naming)set_mode(track_name_return_mode_);}
 bool begin_browser_edit(BrowserOperation operation);
 bool browser_edit_character(int delta);
 bool browser_edit_key(unsigned index);
 bool request_file(FileAction action);
 FileAction pending_file_action()const;
 // Auto-refresh is consumed after explicit actions; it never locks recording controls.
 bool file_action_pending()const{return resample_cancel_request_ || record_stop_request_ || file_action_!=FileAction::None;}
 FileAction take_file_action();

 const AudioVisualFrame& visual()const{return visual_;}
 const DeviceSettings& device_settings()const{return device_;}
 uint32_t device_revision()const{return device_revision_;}
 void device_saved(bool ok){device_saved_=ok;dirty_=true;}
 bool device_saved()const{return device_saved_;}
 bool set_device_settings(const DeviceSettings& next);
 void storage_space(bool ok,uint64_t free,uint64_t total){storage_valid_=ok;storage_free_=free;storage_total_=total;dirty_=true;}
 bool storage_valid()const{return storage_valid_;}uint64_t storage_free()const{return storage_free_;}uint64_t storage_total()const{return storage_total_;}
 const TunerReading& tuner()const{return tuner_;}
 const ui::VisualSettings& visual_settings()const{return settings_;}
 unsigned settings_cursor()const{return settings_cursor_;}
 unsigned settings_page()const{return settings_page_;}
 unsigned settings_count()const{static const uint8_t counts[]={1,4,2,1,2,2,1};return counts[settings_page_];}
 float setting_raw_value(unsigned id)const;
 float setting_row_normalized(unsigned row)const;
 unsigned setting_knob_control(unsigned index)const{return index==0?setting_control(settings_cursor_):255;}
 unsigned project_list_cursor()const{return project_list_cursor_;}
 unsigned system_space_unit()const{return system_space_unit_;}
 unsigned setting_control(unsigned index)const;
 const char* setting_name(unsigned index)const;
 const char* setting_text(unsigned index)const;
 void format_device_number(int n,const char* unit)const;

 // One navigation contract: turn/up/down selects, push activates or edits,
 // left/right changes the page, SHIFT+turn selects the track.
 const char* encoder_hint()const;
 bool encoder(int delta);
 bool encoder_push();
 // Only local edit pages participate in 4-D paging. Navigation/actions such
 // as MODULATION, TRIM, STORE SOUND and reference writes require their Top key.
 int synth_adjacent_page(int delta)const;
 // Direction lamps describe the same bounded navigation used below.
 bool encoder_direction_available(uint8_t direction)const;
 bool encoder_direction(uint8_t rising);
 const char* help_button()const;
 bool help_button_down()const{const auto* b=mk3::button(help_button());return b && (controls_[b->report_byte-1]&b->mask);}
 const char* file_notice_text()const{return file_notice_.active(now_ms_)?file_notice_.text:"";}
 bool help_visible()const{return !modal_active() && help_down_ && help_mode_==mode_ && help_button_down() && now_ms_-help_since_>=kPageHomeMs;}
 void service_help();
 unsigned asset_cursor()const{return asset_cursor_;}
 uint64_t ui_time()const{return now_ms_;}
 uint64_t visual_elapsed_samples()const{uint64_t ms=now_ms_>=visual_received_ms_?now_ms_-visual_received_ms_:0;return (ms>100?100:ms)*visual_.rate/1000;}
 float display_bar_phase()const;
 float display_song_tick()const;
 bool fill_active()const{return fill_active_;}
 bool fx_active()const{return stutter_active_;}
 bool mute_modifier()const{return controls_[4]&0x20;}
 bool solo_modifier()const{return controls_[4]&0x10;}
 void animate(uint64_t ms);
 void warning(bool on){warning_=on;}
 bool project_save_safe()const{return engine_.project_snapshot_safe();}
 bool require_project_save_safe(){if(project_save_safe())return true;notify(engine_.project_event_loss()?UiNotice::ProjectUnsynced:UiNotice::ProjectPending);return false;}
 void request_save(){if(project_unnamed_ || factory_path(project_path_)){set_mode(UiMode::Project);begin_project_save_as();return;}save_requested_=true;}
 void sync_ui_settings();
 void notify(UiNotice notice){if(notice==UiNotice::Loaded || notice==UiNotice::NewProject)recovery_warning_=false;notice_=notice;notice_ms_=now_ms_;dirty_=true;}
 void notify_recovery(RecoverySource source);
 UiNotice notice()const{return now_ms_-notice_ms_<(notice_==UiNotice::Recovered?12000u:2000u)?notice_:(recovery_warning_?UiNotice::Recovered:UiNotice::None);}
 const char* notice_text()const;
 bool notice_error()const;
 bool warning()const;
 // UI producer; hardware processes the slot on core 2, Desktop on its UI thread.
 void set_visual_analysis_worker(LiveVisualAnalysisSlot* worker,bool inline_worker=false){visual_analysis_worker_=worker;visual_analysis_inline_=inline_worker;analysis_valid_=false;}
 unsigned visual_source()const;
 bool audio_undo_available(unsigned track)const{return track<16 && audio_undo_valid_[track];}
 const VisualAnalysisFrame* analysis()const;
 bool preview_frame_current()const{return visual_.preview_ticket==(engine_.preview_state()>>1);}
 bool visual_fresh()const{return visual_.sample && now_ms_-visual_received_ms_<=250;}
 void service_visual_analysis();
 void poll_visual();
 void poll() ;
 void clear_launches(){queued_scene_=-1;queued_stops_=0;for(auto& pending:pending_)pending=-1;}
 bool launch_clip(unsigned t,unsigned pattern,bool quantized=true) ;
 bool stop_clip(unsigned t,bool quantized=true) ;
 bool launch_scene(unsigned scene,bool quantized=true) ;
 bool set_chain(const SceneChain& chain){if(!valid_scene_chain(chain))return false;Command after{CommandType::SetChain,0,0,0},before=after;after.chain=chain;before.chain=project_.chain;return edit(after,before);}
 bool clip_follow_page()const{return clip_follow_page_;}
 bool set_clip_follow(const ClipFollow& f);
 bool audible_muted(unsigned t)const{return arrangement_ui_active_?bool((arrangement_ui_mute_&(1u<<t)) || (arrangement_ui_solo_ && !(arrangement_ui_solo_&(1u<<t)))):track_muted(project_,t);}
 bool scene_take_requested()const{return scene_take_requested_;}
 const char* scene_take_status()const;
 bool scene_take_toggle();
 bool scene_take_cancel(){return scene_take_requested_ && send({CommandType::SceneTakeControl,0,2,0});}
 void service_scene_take();
 ClipControlContext clip_controls()const{return {shifted(),clip_follow_page_,pattern_copied_,track_const(selected_).length<=kSteps/2,history_cursor_>0,history_cursor_<history_size_};}
 ModulationContext modulation_controls()const{return {shifted(),modulation_page_,multitimbral(),live_recording_ || resampling_ || recording_file_busy_,history_cursor_>0,history_cursor_<history_size_};}
 StepContext step_controls()const;
 BrowserContext browser_controls()const;
 SamplerContext sampler_controls()const{return {shifted(),sampler_page_,track_const(selected_).instrument==Instrument::DrumKit,recording_controls(),sample_auditioning(),page_,history_cursor_,history_size_};}
 EffectsContext effects_controls()const;
 RoutingFxContext routing_fx_controls()const{return {shifted(),live_recording_,group_send_page_,group_fx_slot_,project_.routing.monitor>0,record_quantized_};}
 PerformContext perform_controls()const;
 BrowserEditContext browser_edit_controls()const{return {browser_.edit.operation,browser_confirmation()};}
 SongControlContext song_controls()const;
 bool song_send(const SongState& s);
 bool song_section_send(unsigned i,const SongSection& s);
 bool song_materialize();
 bool song_add();
 bool song_delete();
 bool song_clear(){auto s=project_.song;s.length=0;song_cursor_=0;return song_send(s);}
 bool append_chain(unsigned scene){if(scene>=16 || project_.chain.length>=16)return false;auto chain=project_.chain;chain.scenes[chain.length++]=uint8_t(scene);return set_chain(chain);}
 bool clear_chain(){return set_chain(SceneChain{});}
 bool kit_edit_page()const{return mode_==UiMode::Synth && !source_select_ && instrument_view_==0 && track_const(selected_).instrument==Instrument::DrumKit;}
 bool synth_core_page()const;
 bool native_synth_page()const;
 uint16_t core_parameter(unsigned i)const;
 uint16_t synth_parameter(unsigned index)const;
 bool layer_page()const{return mode_==UiMode::Synth && !source_select_ && instrument_view_==0 && track_const(selected_).instrument==Instrument::Layer;}
 unsigned layer_edit_page()const{return layer_edit_page_;}
 unsigned layer_cursor()const{return layer_cursor_;}unsigned layer_pool_cursor()const{return layer_pool_cursor_;}
 const SoundState& layer_candidate()const{return project_.sounds[layer_pool_cursor_];}
 bool layer_use(bool remove=false);
 bool layer_knob(unsigned i,int change);
 unsigned instrument_view()const{return instrument_view_;} bool source_select()const{return source_select_;} unsigned source_cursor()const{return source_cursor_;} bool source_arm_unknown()const{return source_arm_unknown_;}
 unsigned generate_page()const{return gen_page_;}unsigned fx_page()const{return fx_page_;} unsigned chain_node()const{return chain_node_;}unsigned song_cursor()const{return song_cursor_;} unsigned song_display_count()const{return project_.song.length?project_.song.length:project_.chain.length;} unsigned slice_cursor()const{return slice_cursor_;} bool zone_vel()const{return zone_vel_;} bool note_edit()const{return note_edit_;} bool follow_display()const{return follow_display_;} bool update_confirm_pending()const{return update_confirm_;} StripMode strip_mode()const{return axes_.strip_mode;}unsigned fx_focus_slot()const{return fx_focus_slot_;}
 unsigned generate_lane()const{return gen_lane_;}
 unsigned generate_cell()const{return gen_cell_;}
 bool advanced(AdvancedGroup group,unsigned key,int value,unsigned cell=0);
 bool follow(FollowMode mode,unsigned probability);
 bool chain_bars(unsigned bars){if(bars<1 || bars>16)return false;auto chain=project_.chain;chain.bars=uint8_t(bars);return set_chain(chain);}
 bool chain_enabled(bool on){if(on && !project_.chain.length)return false;auto chain=project_.chain;chain.enabled=on;return set_chain(chain);}
 float snap_parameter(uint16_t id,float value)const;
 bool set_parameter(uint16_t id,float value) ;
 ParameterTarget lock_target(unsigned step,uint16_t id)const;
 bool set_lock(unsigned step,uint16_t id,float value,unsigned slot=kLocks) ;
 float step_sample_lock_value(unsigned step,uint16_t id)const{if(step>=kSteps)return -1;for(const auto& lock:track_const(selected_).steps[step].locks)if(lock.enabled && lock.id==lock_target(selected_step_,id))return lock.value;return -1;}
 bool set_step_sample_lock(unsigned step,uint16_t id,int value);

 bool clear_step_locks();

 uint8_t control_value(unsigned byte)const{return byte>=1 && byte<=10?controls_[byte-1]:0;}
 bool set_sound_override(unsigned slot);
 bool set_sound_lock(unsigned step,unsigned sound);
 bool sample_favorite(const char* path)const;
 uint32_t favorite_revision()const{return favorite_revision_;}
 bool set_sample_favorite(const char* path,bool enabled);
 bool asset_playback(unsigned slot,unsigned root,unsigned bpm);
 bool sample_edit(const SampleEdit& e);
 bool set_sample_region(unsigned slice,unsigned slices,bool loop,bool reverse);
 bool set_fill(bool on) {if(!send({CommandType::SetFill,0,0,uint16_t(on)}))return false;fill_active_=on;dirty_=true;return true;}
 unsigned chord_shape()const{return project_.ui.chord_shape;} unsigned chord_inversion()const{return project_.ui.chord_inversion;}
 bool note_repeat(unsigned mode,unsigned rate);
 unsigned note_repeat_mode()const{return repeat_mode_;}unsigned note_repeat_rate()const{return repeat_rate_;}bool repeat_latch()const{return repeat_latch_;}
 bool set_repeat_latch(bool on);
 bool track_arp(unsigned mode,unsigned rate,unsigned octaves);
 bool repeat_config(unsigned direction,unsigned octaves,unsigned gate);
 bool stutter(bool on) {if(!send({CommandType::SetStutter,0,uint8_t(2u<<live_repeat_rate_),uint16_t(on)}))return false;stutter_active_=on;dirty_=true;return true;}
 bool morph(float position) ;
 bool set_performance_fx(PerformanceFx mode,float amount,bool latch=false);
 bool performance_fx_tail()const{return performance_fx_tail_;}unsigned performance_fx_quantize()const{return performance_fx_quantize_;}unsigned live_repeat_rate()const{return live_repeat_rate_;}
 PerformanceFx performance_fx_mode()const{return performance_fx_mode_;}float performance_fx_amount()const{return performance_fx_amount_;}bool performance_fx_latched()const{return performance_fx_latched_;}
 bool release_strip();
 bool set_strip_mode(StripMode mode){release_strip();axes_.strip_mode=mode;dirty_=true;return true;}
 bool strip_input(int position,int maximum);

 bool capture_retro(){return send({CommandType::CaptureRetro,0,0,uint16_t(capture_quantized_?1:0)});}
 bool capture_toggle_quantize(){capture_quantized_=!capture_quantized_;dirty_=true;return true;}
 bool capture_quantized()const{return capture_quantized_;}
 bool capture_quantized_=true;
 bool morph_select_set(unsigned n){if(n>3 || n==project_.morph_set)return false;if(!send({CommandType::SetMorphSet,0,0,uint16_t(n)}))return false;morph_swap_sets(project_,n);dirty_=true;return true;}
 bool store_morph(unsigned side) ;
 bool undo();
 bool redo();
 bool euclidean(unsigned pulses,unsigned rotation=0) ;
 void release_all();
 void project_loaded() ;
 // Install a controller snapshot without firing commands for already-held buttons.
 bool seed_control(unsigned byte,uint8_t value) {if(byte<1 || byte>10)return false;controls_[byte-1]=value;dirty_=true;return true;}
 bool control(unsigned byte,uint8_t value) ;
 bool set_instrument(Instrument instrument) ;
 bool assign_overdub(unsigned slot,WavView view,const SampleMetadata& metadata,WavView prior,unsigned target,unsigned pattern);
 bool assign_sample(unsigned slot,WavView view,const SampleMetadata* metadata=nullptr,unsigned destination=255,unsigned pattern=255,unsigned kit_pad=255) ;
 // Stage 5 S5-3: freeze / unfreeze a track by swapping only its
 // frozen_asset field through the pattern-apply path (undoable, phase
 // preserved). The original instrument and steps stay in the pattern,
 // so unfreezing is lossless. Bindings/assets are supplied when a new
 // freeze render was registered; unfreeze passes neither.
 bool set_frozen_asset(unsigned track,unsigned pattern,unsigned slot,const WavView* bindings=nullptr,const SampleAssetState* assets=nullptr);
 // SCR-EDT-01 SOURCE SELECT (SPEC 2.8, S-04): the engine list is the
 // primary instrument-selection path; cycle_instrument() stays as the
 // auxiliary path. The cursor walks the descriptor registry and the
 // FAMILY knob steps between the first engines of each family in
 // display order (family groups in the renderer use the same order).
 static bool source_family_is(const char* a,const char* b){unsigned n=0;while(a[n] && a[n]==b[n])++n;return a[n]==b[n];}
 static int source_family_index_of(unsigned engine);
 static unsigned source_family_first(int family){
  for(unsigned e=0;e<instrument_count();++e)if(source_family_index_of(e)==family)return e;
  return 0;
 }
 bool toggle_source_select();
 bool source_confirm();
 bool source_knob(unsigned index,int change);
 bool cycle_instrument() ;
 bool pad(uint8_t hardware,uint8_t tag,uint16_t pressure) ;
 bool pad_input(uint8_t hardware,uint8_t tag,uint16_t pressure) ;
 const char* parameter_name(uint16_t id)const;
 FxChoice fx_knob_control(unsigned index)const;
 const char* knob_label(unsigned index) const ;
 float editing_parameter(uint16_t id)const;
 float knob_value(unsigned index) const ;
 void generate_range(unsigned index,int& lo,int& hi)const;
 float knob_normalized(unsigned index)const ;
 template<class T> T snap_center(T previous,T next,T center=T{})const{return center_snap_ && previous!=center && (previous-center)*(next-center)<=0?center:next;}
 bool kit_knob(unsigned index,int change);
 bool midi_import_confirmation()const{return modal_.owner==ModalOwner::MidiImport;}
 bool begin_midi_import();
 bool take_midi_import_request(MidiImportRequest& out);
 void midi_import_started(uint32_t ticket){midi_import_ticket_=ticket;}
 uint32_t take_midi_import_cancel(){uint32_t ticket=midi_import_cancel_ticket_;midi_import_cancel_ticket_=0;return ticket;}
 bool midi_import_finished(uint32_t ticket);
 bool modal_active()const{return browser_editing() || modal_.owner!=ModalOwner::None;}
 bool modal_accept();
 bool modal_cancel();
 bool modal_progress()const{return (modal_.owner==ModalOwner::Twin && twin_modal_==2) || modal_.owner==ModalOwner::Freeze || (modal_.owner==ModalOwner::MidiImport && modal_.value>=2) || (modal_.owner==ModalOwner::Stretch && stretch_modal()==2);}
 const char* modal_label(unsigned index)const{if(composition_modal_)return index==0?"APPLY":index==1?"CANCEL":"";if(twin_modal_==1)return index==0?(twin_config_.valid()?"PREPARE":""):index==1?"CANCEL":"";return modal_button(index,modal_progress()).label;}
 bool modal_key(unsigned index){switch(modal_button(index,modal_progress()).action){case ModalAction::Accept:return modal_accept();case ModalAction::Cancel:return modal_cancel();default:return false;}}
 ParameterTarget knob_reset_target(unsigned index)const;
 bool knob_reset_available(unsigned index)const;
 bool reset_touched_knobs();
 bool knob(unsigned index,int16_t delta) ;
 bool send(const Command& c) ;
 ui::LedState recording_led_state()const;
 ui::LedState recording_track_led(unsigned track)const;
 ui::PadRole pad_role()const;
 void clear_pad_feedback(){for(auto& context:pad_press_context_)context=0;for(auto& seen:trigger_seen_)seen=false;}
 uint64_t pad_feedback_context()const;
 const char* pad_hint()const;
 ui::LedState keyboard_pad_state(unsigned n)const;
 ui::PadGround pad_color_object(unsigned n)const;
 ui::LedState pad_feedback(unsigned n)const;
 void led_frame(ui::LedFrame& out)const;
 void pad_leds(uint8_t out[16])const{ui::LedFrame frame;led_frame(frame);for(unsigned i=0;i<16;++i)out[i]=frame.pads[i];}
 void button_leds(uint8_t out[62])const{ui::LedFrame frame;led_frame(frame);for(unsigned i=0;i<62;++i)out[i]=frame.buttons[i];}
 void base_button_leds(uint8_t out[62]) const ;
 const uint8_t* steps() const { return step_view_; }
 unsigned editing_pattern()const{return project_.active_pattern;}
 unsigned selected_track() const { return selected_; }
 ui::EngineKind selected_engine_kind()const{return library_catalog_controls() && browser_.previewing?ui::engine_kind(browser_.preview_instrument):ui::track_engine_kind(project_,selected_);}
 bool editing() const { return mode_==UiMode::Step; }
 bool dirty() const { return dirty_; }
 bool stop_audition(){if(!send({CommandType::StopPreview,0,0,0}))return false;browser_.previewing=false;browser_.audition_path[0]=0;return true;}
 const TransferView& transfer_view()const{return transfer_view_;}
 void transfer_view(const TransferView& view){if(__builtin_memcmp(&transfer_view_,&view,sizeof view)){transfer_view_=view;dirty_=true;}}
 bool take_transfer_request(){bool requested=transfer_request_;transfer_request_=false;return requested;}
 bool take_transfer_start(){bool start=transfer_start_;transfer_start_=false;return start;}
 bool take_transfer_browse(){bool browse=transfer_browse_;transfer_browse_=false;return browse;}
 bool take_transfer_close(){bool close=transfer_close_;transfer_close_=false;return close;}
 void mark_dirty() { dirty_=true; }
 void set_dirty(bool value) { dirty_=value; }
 unsigned dropped() const { return dropped_; }
 uint16_t held_pad_mask()const{uint16_t mask=0;for(unsigned i=0;i<16;++i)if(held_[i])mask|=uint16_t(1u<<i);return mask;}
  ~Performance(){clear_history();delete[] history_;delete[] rotation_revisions_;delete phrase_clipboard_;delete pattern_clipboard_;delete lock_point_;delete reload_point_;}
 enum class ProjectAction:uint8_t { None,New,Load };
 void request_project(ProjectAction action){if(recording_file_busy_ || file_action_pending() || modal_active()){notify(UiNotice::Failed);return;}if(action==ProjectAction::New && composition_file_service_){request_file(FileAction::NewProjectMenu);return;}set_mode(UiMode::Project);pending_project_=action;}
 ProjectAction pending_project()const{return pending_project_;}
 ProjectAction take_project_action(){auto result=project_action_;project_action_=ProjectAction::None;return result;}
 bool confirm_project(){if(pending_project_==ProjectAction::None)return false;project_action_=pending_project_;pending_project_=ProjectAction::None;return true;}
 void cancel_project(){pending_project_=ProjectAction::None;}
 unsigned group_cursor()const{return group_cursor_;}unsigned group_fx_slot()const{return group_fx_slot_;}bool group_send_page()const{return group_send_page_;}bool routing_fx_page()const{return routing_fx_page_;}
 unsigned nrpn_slot()const{return nrpn_slot_;}unsigned modulation_page()const{return modulation_page_;}unsigned automation_lane()const{return automation_lane_;}unsigned automation_cell()const{return automation_cell_;}
 unsigned macro_index()const{return macro_index_;}unsigned macro_route()const{return macro_route_;}unsigned performance_page()const{return performance_page_;}unsigned performance_fx_bank()const{return performance_fx_bank_;}unsigned sound_slot()const{return sound_slot_;}unsigned scene_slot()const{return scene_slot_;}bool launch_quantized()const{return launch_quantized_;}
 bool automation(unsigned lane,AutomationLane state);
 bool macro_value(unsigned index,float value,bool active=true);
 bool macro_route(unsigned index,unsigned slot,MacroRoute route);
 bool swap_group_inserts();
 uint16_t chain_key_parameter()const{return chain_node_==3?92:chain_node_>=1?uint16_t(94+(chain_node_-1)*4+3):0;}
 bool chain_toggle_bypass(){if(chain_node_<1 || chain_node_>2)return false;uint16_t id=uint16_t(102+chain_node_-1);return set_parameter(id,editing_parameter(id)>0.5f?0.0f:1.0f);}
 bool chain_edit();
 bool swap_track_inserts();
 bool reset_live();
 bool store_scene(unsigned slot);
 bool capture_snapshot(ProjectState*& slot){if(!slot){slot=new ProjectState();if(!slot)return false;}*slot=project_;dirty_=true;return true;}
 bool install_snapshot(const ProjectState* point){if(!point)return false;if(!engine_.stage_project(*point)){notify(UiNotice::Failed);return false;}project_=*point;project_loaded();dirty_=true;return true;}
 // Temporary Reload (Stage 4): the restore point is captured
 // automatically on entering Perform; Shift+LOCK returns to it.
 bool temporary_reload(){return install_snapshot(reload_point_);}
 bool reload_available()const{return reload_point_!=nullptr;}
 bool lock_available()const{return lock_point_!=nullptr;}
 // LOCK (all pages, SPEC 2.7): first press saves a full snapshot,
 // the next restores it and releases the point. Shift+LOCK is the
 // Temporary Reload restore instead.
 bool lock_press();
 ProjectState* lock_point_=nullptr;ProjectState* reload_point_=nullptr;
 bool apply_sound(const SoundState& sound);
 bool store_sound(unsigned slot);
 bool recall_sound(unsigned slot){return slot<kPresetSlots && apply_sound(project_.sounds[slot]);}
 bool loop_take();
 bool swap_kit_pad_sounds(unsigned first,unsigned second);
 bool map_kit_slices();
 bool chop_to_steps();
 SequenceCommitResult publish_midi_import(const MidiImport& m);
 SequenceCommitResult publish_track_sequence(unsigned pattern,unsigned track_index,ConstTrackView imported,unsigned automation_mask=0,unsigned nrpn_mask=0);
 bool replace_track_sequence(unsigned pattern,unsigned track_index,ConstTrackView imported,unsigned automation_mask=0,unsigned nrpn_mask=0){return publish_track_sequence(pattern,track_index,imported,automation_mask,nrpn_mask)==SequenceCommitResult::Applied;}

 bool replace_pattern_contents(const PatternState& value);
 // A library kit is one track, not a project replacement. Publish its pattern
 // and all prepared sample views together, preserving transport and other tracks.
 bool memory_refused(){copy_text(browser_.status,sizeof browser_.status,"Not enough memory / sound kept");notify(UiNotice::MemoryFull);return false;}
 void seed_preset_guids(const uint8_t (&seed)[16]){preset_guids_.seed(seed);}
 bool prepare_preset_references();
 bool register_saved_preset(unsigned,ConstTrackView,const PresetRegistration&);
 bool apply_preset_track(ConstTrackView source,const Command* sample_binding=nullptr,const WavView* bindings=nullptr,const SampleAssetState* assets=nullptr,const PresetFxSnapshot* fx=nullptr,const PresetRegistration* preset=nullptr);

 bool extend_track();
 bool copy_step();
 bool paste_step();
 bool copy_pattern();
 bool commit_pattern(PatternState* after);
 bool paste_pattern();
 bool commit_composition_phrase(unsigned,const PhraseState&);
 bool commit_composition_pattern(PatternState*);
 bool duplicate_variation();bool begin_variation();bool composition_apply();bool composition_knob(unsigned,int);
 bool harmony_generate();bool apply_groove_all();bool song_skeleton();bool scene_empty(unsigned)const;bool begin_scene_duplicate();bool duplicate_scene(unsigned,unsigned);bool composition_notice(const char*,bool error=false);
 unsigned composition_dialog()const{return composition_modal_;}
 unsigned scene_duplicate_target()const{return composition_scene_target_;}unsigned scene_duplicate_source()const{return composition_scene_source_;}
 void composition_file_service(bool available){composition_file_service_=available;}
 void request_empty_project(){set_mode(UiMode::Project);pending_project_=ProjectAction::New;}
 const char* composition_message()const{return composition_message_;}
 const VariationOptions& variation_options()const{return variation_;}VariationOptions& variation_options(){return variation_;}
 unsigned harmony_source()const{return harmony_source_;}HarmonyMode harmony_mode()const{return harmony_mode_;}
 bool copy_track_only()const{return copy_track_only_;}
 bool pattern_is_empty(unsigned p)const{for(const auto& t:project_.patterns[p].tracks)if(!sequence_empty(t))return false;return true;}
 bool duplicate_pattern();
 bool duplicate_step();
 bool double_pattern();
 bool erase_parameter_data(uint16_t id);
 bool toggle_step_chord(unsigned note);
 bool set_click_setting(unsigned bit);
 bool set_step_note(unsigned step,unsigned note);
 bool step_locked(uint16_t id) const ;
 // SPEC 2.6 locked state: the selected step of the selected
 // track carries an enabled lock for this parameter. DrumKit
 // steps use their own lock fields and report false here.
 uint16_t knob_parameter(unsigned i)const;
 float knob_display_value(unsigned i)const;
 float knob_display_normalized(unsigned i)const;
 float effective_parameter(uint16_t id)const;

 bool take_update_request(){bool value=update_requested_;update_requested_=false;return value;}
 bool take_save_request() ;
 bool extra_knob(unsigned index,int change,bool& handled);

private:
 SynthControlContext synth_controls()const ;
 bool synth_key(unsigned index) ;

 bool trigger_held_pad(uint8_t hardware,unsigned target,unsigned note,uint16_t pressure,uint8_t kind,int slice=-1);
 bool trigger_kit_pad(uint8_t hardware,unsigned pad,uint16_t pressure);
 // Disconnect can coincide with a full audio queue. Retain releases until the
 // audio consumer makes room; clearing the physical held state is not enough.
 void retry_releases() ;
 TrackView& track(unsigned index) { return track_views_.at(project_.audio[index].kind==TrackKind::Instrument?project_.active_pattern:0,index); }
 const TrackView& track_const(unsigned index) const { return track_views_.at(project_.audio[index].kind==TrackKind::Instrument?project_.active_pattern:0,index); }
 bool set_step(unsigned step,bool enabled) ;
 bool edit(Command after,Command before) ;
 void dispose(History& h);
 void clear_history(){for(unsigned i=0;i<64;++i)dispose(history_[i]);history_size_=history_cursor_=0;}
 void note_assignment_changes(unsigned pattern,const PatternState& before,const PatternState& after,uint16_t mask);
 void note_assignment_changes(unsigned pattern,const PatternState& after,uint16_t mask);
 void push_history(Command before,Command after,PatternState* pbefore=nullptr,PatternState* pafter=nullptr,TrackHistory* tbefore=nullptr,TrackHistory* tafter=nullptr);
 void install_single_track(unsigned pattern,unsigned t,const TrackState& state,bool rebound);
 bool apply_history(History& h,bool redo);

 struct RecordIdentity {uint32_t id=0;uint8_t pattern=0,track=0,step=0;};
 bool has_record_ids()const{for(const auto& r:record_ids_)if(r.id)return true;return false;}
 void clear_record_ids(){for(auto& r:record_ids_)r.id=0;}
 void clear_record_ids(unsigned pattern,uint16_t tracks){for(auto& r:record_ids_)if(r.id && r.pattern==pattern && (tracks&(1u<<r.track)))r.id=0;}
 void remember_rotation(unsigned pattern,unsigned track,uint32_t from,uint32_t to,unsigned length,int direction);
 bool rebase_record(EngineEvent& event)const;
 void forget_record(unsigned pattern,unsigned track,unsigned step){for(auto& r:record_ids_)if(r.id && r.pattern==pattern && r.track==track && r.step==step)r.id=0;}
 bool matches_record(unsigned pattern,unsigned track,unsigned step,uint32_t id)const{for(const auto& r:record_ids_)if(r.id==id && r.pattern==pattern && r.track==track && r.step==step)return true;return false;}
 void remember_record(unsigned pattern,unsigned track,unsigned step,uint32_t id);
 void mirror(const Command& c) ;
 void refresh_steps() ;
 unsigned preset_save_track_=kTracks;uint32_t preset_save_generation_=0;uint64_t preset_save_revision_=0;
 uint64_t assignment_revision_[kTracks]{};
 bool routing_fx_page_=false,group_send_page_=false;uint8_t group_fx_slot_=0;BrowserState browser_{};uint8_t midi_capture_state_=0;char midi_capture_path_[24]{};unsigned diagnostic_offset_=0;HardwareDiagnostics diagnostics_{};bool browser_refresh_pending_=false;FileAction file_action_=FileAction::None;uint32_t erase_generation_=0;uint8_t erase_track_=0,erase_pattern_=0;uint8_t system_space_unit_=0,project_list_cursor_=0,sampler_page_=0,sample_pad_=0,slice_cursor_=0,group_cursor_=0,diagnostic_test_=0,routing_page_=0;bool zone_vel_=false;
 bool strip_active_=false,strip_note_active_=false,strip_release_pending_=false;uint8_t strip_track_=0,strip_pattern_=0,strip_note_=0;
 bool project_unnamed_=false;
 char project_path_[160]="PROJECTS/Untitled.mgxs";
 uint64_t pad_press_context_[16]{},pad_press_ms_[16]{};
 DeviceSettings device_{};uint32_t device_revision_=0;bool device_saved_=true,storage_valid_=false;uint64_t storage_free_=0,storage_total_=0,tuner_sample_=0;TunerReading tuner_{};mutable char device_text_[32]{};
 LiveVisualAnalysisSlot local_visual_analysis_{};LiveVisualAnalysisSlot* visual_analysis_worker_=&local_visual_analysis_;bool visual_analysis_inline_=true,analysis_valid_=false;VisualAnalysisFrame analysis_{};uint64_t analysis_received_ms_=0,analysis_due_ms_=0,analysis_requested_sample_=0;unsigned analysis_backoff_=0;
 char recovery_message_[64]{};UiNotice notice_=UiNotice::None;uint64_t notice_ms_=0;AudioVisualFrame visual_{};ui::VisualSettings settings_{};uint64_t knob_overlay_until_[8]{},knob_turn_until_[8]{};uint64_t now_ms_=0,launch_ms_=0,strip_interaction_until_ms_=0,visual_received_ms_=0,trigger_ms_[16]{};bool trigger_seen_[16]{},warning_=false,fill_active_=false,stutter_active_=false;uint8_t layer_cursor_=0,layer_edit_page_=0;uint16_t layer_pool_cursor_=0;uint8_t asset_cursor_=0,instrument_view_=0,settings_cursor_=0,settings_page_=0,source_cursor_=0,chain_node_=0,song_cursor_=0;bool source_select_=false;bool note_edit_=false,follow_display_=false;int8_t queued_scene_=-1;uint16_t queued_stops_=0;
 GrooveState groove_clipboard_{};int16_t groove_clip_timing_[16]{};bool groove_copied_=false,groove_clip_clipped_=false;
 History* history_=new History[64]{};RotationRevision* rotation_revisions_=new RotationRevision[128]{};unsigned rotation_cursor_=0;
 uint8_t sn_ref_track_=0,sn_ref_pattern_=0,sn_ref_channel_=0;uint64_t sn_ref_armed_ms_=0;
 AudioLane audio_undo_[16]{};bool audio_undo_valid_[16]{};uint32_t audio_revision_[16]{},queued_audio_revision_=0;uint8_t audio_clip_cursor_=0,queued_audio_clip_=0;
 uint8_t loop_track_=0,loop_pattern_=0,loop_slot_=0;uint32_t loop_generation_=0;bool tracker_view_=false,step_overview_=false,counting_in_=false,note_recording_=false,note_finish_pending_=false,gesture_recording_=false,gesture_playing_=false,live_recording_=false,record_quantized_=false;uint8_t live_record_source_=255;uint8_t creative_lane_=0,creative_cell_=0,tuning_note_=0,note_record_track_=0,count_beat_=0;
 bool resampling_=false,resample_cancel_pending_=false,resample_cancel_request_=false,recording_file_busy_=false;uint8_t resample_bars_=1;ResampleRequest queued_resample_{},active_resample_{};LiveRecordRequest queued_record_{},queued_take_{};RecordingTarget active_record_target_{};RecordingOrigin active_record_origin_=RecordingOrigin::None;bool live_record_quantized_=false,record_stop_pending_=false,record_stop_request_=false;
 bool clip_follow_page_=false;
 bool arrangement_ui_active_=false;uint16_t arrangement_ui_mute_=0,arrangement_ui_solo_=0;
 bool scene_take_requested_=false;char scene_take_message_[96]{};
 uint16_t song_position_=0;bool seek_ready_=false;
 bool performance_fx_tail_=true;uint8_t fx_page_=0,fx_focus_slot_=0,performance_fx_quantize_=0,live_repeat_rate_=1,performance_fx_bank_=0;uint8_t repeat_mode_=0,repeat_rate_=2;bool repeat_release_pending_=false,repeat_latch_=false,repeat_ignore_release_=false;
 uint64_t edit_serial_=0;
 class EditGroup {
  Performance& ui_;uint32_t previous_;uint64_t serial_;bool finished_=false;
 public:
  explicit EditGroup(Performance& ui):ui_(ui),previous_(ui.active_group_),serial_(ui.edit_serial_){ui_.active_group_=++ui_.next_group_;}
  bool finish(bool ok){if(!finished_){finished_=true;ui_.active_group_=previous_;if(!ok && ui_.edit_serial_!=serial_){ui_.refresh_steps();ui_.notify(UiNotice::PartialApplied);}}return ok;}
  ~EditGroup(){finish(false);}
 };
 RecordIdentity record_ids_[64]{};uint8_t record_cursor_=0;uint32_t next_group_=0,active_group_=0,event_dropped_seen_=0;PatternState* pattern_clipboard_=nullptr;StepState step_clipboard_{};KitStepState kit_step_clipboard_{};bool copy_track_only_=false,clipboard_track_only_=false;uint8_t clipboard_track_=0,harmony_source_=0;HarmonyMode harmony_mode_=HarmonyMode::Bass;VariationOptions variation_{};bool composition_file_service_=false;uint8_t composition_scene_source_=0,composition_scene_target_=1;PhraseState* phrase_clipboard_=nullptr;char composition_message_[96]{};bool pattern_copied_=false,step_copied_=false,step_clipboard_is_kit_=false;uint8_t extend_pattern_=0,extend_track_=0;uint16_t extend_length_=0;uint64_t extend_time_=0;ModalField<ProjectAction> pending_project_{modal_,ModalOwner::Project};ProjectAction project_action_=ProjectAction::None;uint8_t modulation_page_=0,automation_lane_=0,automation_cell_=0,macro_index_=0,macro_route_=0,performance_page_=0,sound_slot_=0,scene_slot_=0,midi_cc_cursor_=0,midi_input_cursor_=0,nrpn_slot_=0;bool audio_available_=true,launch_quantized_=true,fixed_velocity_=false;bool encoder_edit_=false;uint8_t main_encoder_mode_=0;uint64_t last_tap_ms_=0;
 Engine& engine_; ProjectState& project_;TrackViews track_views_; bool notes_held_[16]{},pending_release_[16]{}; uint8_t held_tracks_[16]{},held_notes_[16]{},held_kind_[16]{},step_view_[16]{},positions_[16]{},wave_ref_[16]{}; int8_t pending_[16]{}; uint16_t pressures_[16]{},raw_pressures_[16]{}; bool held_[16]{},release_stutter_=false,release_fill_=false,release_control_all_=false,release_performance_fx_=false,performance_fx_latched_=false;
 uint8_t held_step_hardware_=255; uint8_t controls_[10]{};
 uint8_t selected_=0,selected_kit_pad_=0,page_=0,selected_step_=0,gen_page_=0,gen_lane_=0,gen_cell_=0,history_cursor_=0,history_size_=0,held_stutter_pad_=255,held_fill_pad_=255,held_performance_pad_=255;PerformanceFx performance_fx_mode_=PerformanceFx::Off;float performance_fx_amount_=0.6f; int accum_[8]{}; uint32_t generation_=0,favorite_revision_=0; unsigned dropped_=0,pulses_=5,rotation_=0; UiMode mode_=UiMode::Pad; UiAxes axes_;
 static constexpr uint64_t kPageNavGraceMs=60, kPageHomeMs=400;
 using ReturnTicket=NavigationLocation;
 ReturnTicket ticket_{}; bool ticket_valid_=false,page_left_down_=false,page_right_down_=false,page_left_pending_=false,page_right_pending_=false,page_home_fired_=false; uint64_t page_left_since_=0,page_right_since_=0,page_both_since_=0;bool step_down_=false,step_long_fired_=false;bool center_snap_=false;uint64_t knob_motion_ms_[8]{};uint8_t track_bank_=0,track_peek_slot_=255,track_peek_page_=0;bool track_peek_active_=false;uint64_t track_peek_since_=0;NavigationLocation track_peek_location_;bool step_reentry_=false;uint64_t step_since_=0; uint16_t control_all_parameter_=3;float control_all_position_=0.5f; bool playing_=false,dirty_=true,recovery_warning_=false,save_requested_=false,update_requested_=false,following_clip_=true,control_all_=false;
};
}
