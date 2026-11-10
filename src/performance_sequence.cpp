// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "performance.h"
namespace groove {
auto Performance::start_count_in_record() -> bool{if(module_transport_blocked())return false;if(!arm_note_record(true))return false;if(playing_)return true;if(project_.midi_clock_slave)return play();if(!send({CommandType::StartCountIn,0,0,0}))return false;counting_in_=true;count_beat_=1;playing_=true;dirty_=true;return true;}
auto Performance::seek_song(unsigned position) -> bool{if(position>16383 || playing_ || counting_in_ || (project_.song.enabled && song_has_snapshots(project_.song))){notify(UiNotice::Failed);return false;}if(!send({CommandType::SeekSong,0,0,uint16_t(position)}))return false;song_position_=uint16_t(position);seek_ready_=true;dirty_=true;return true;}
auto Performance::record_navigation() -> void{if(home_navigation_pending_){if(!page_left_down_ && !page_right_down_)home_navigation_pending_=false;return;}if(modal_active() || track_peek_active_)return;auto n=navigation();if(!axes_.history.empty() && axes_.history.current()==n)axes_.history.replace_current(n);else axes_.history.record(n);}
auto Performance::step_button_edge(bool down)  -> void{
  if(down) { step_reentry_=mode_==UiMode::Step;step_down_=true; step_since_=now_ms_; step_long_fired_=false; }
  else {step_down_=false;if(step_reentry_ && !step_long_fired_ && mode_==UiMode::Step)cycle_step_view();}
 }
auto Performance::cycle_step_view() -> void{if(tracker_view_){tracker_view_=false;step_overview_=false;}else if(step_overview_){step_overview_=false;tracker_view_=true;follow_display_=true;}else step_overview_=true;dirty_=true;}
auto Performance::service_step_button()  -> void{
  if(step_down_ && !step_long_fired_ && now_ms_>=step_since_ && now_ms_-step_since_>=kPageHomeMs) {
   step_long_fired_=true;
   if(mode_==UiMode::Step && !note_edit_) { note_edit_=true; dirty_=true; }
  }
 }
auto Performance::live_recording(bool on,uint8_t source,const RecordingTarget* target,bool quantized,RecordingOrigin origin) -> void{live_recording_=on;live_record_source_=on?source:255;live_record_quantized_=on && quantized;if(on){active_record_target_=target?*target:current_recording_target();active_record_origin_=origin==RecordingOrigin::None?(source==1?RecordingOrigin::Input:RecordingOrigin::Source):origin;}else{active_record_origin_=RecordingOrigin::None;record_stop_pending_=record_stop_request_=false;}dirty_=true;}
auto Performance::arm_note_record(bool on) -> bool{if(on)event_dropped_seen_=engine_.event_dropped();if(!send({CommandType::ArmNoteRecord,selected_,0,uint16_t(on)}))return false;note_recording_=on;note_finish_pending_=!on;if(on)note_record_track_=selected_;dirty_=true;return true;}
auto Performance::recording_origin()const -> RecordingOrigin{return live_recording_?active_record_origin_:recording_request_pending()?(file_action_==FileAction::Resample?RecordingOrigin::Resample:queued_record_.origin):RecordingOrigin::None;}
auto Performance::recording_quantized()const -> bool{return live_recording_?live_record_quantized_:recording_request_pending()?(file_action_==FileAction::Resample || (queued_record_.quantized && playing_)):(record_quantized_ && playing_);}
auto Performance::recording_target()const -> RecordingTarget{if(file_action_==FileAction::Capture || file_action_==FileAction::Overdub)return queued_take_.target;return live_recording_?active_record_target_:recording_request_pending()?(file_action_==FileAction::Resample?queued_resample_.target:queued_record_.target):current_recording_target();}
auto Performance::recording_source()const -> unsigned{if(file_action_==FileAction::Capture || file_action_==FileAction::Overdub)return queued_take_.source;return resampling_?active_resample_.source:live_recording_ && live_record_source_<=kCaptureMax?live_record_source_:file_action_==FileAction::Resample?queued_resample_.source:recording_request_pending()?queued_record_.source:capture_source();}
auto Performance::current_recording_target()const -> RecordingTarget{const auto& t=track_const(selected_);unsigned pad=t.instrument==Instrument::DrumKit?selected_kit_pad_:255;return {selected_,project_.active_pattern,engine_.staged_generation(),t.instrument,uint8_t(pad<16?t.kit.pads[pad].sample[0]:t.sample_slot),uint8_t(pad),assignment_revision(project_.active_pattern,selected_)};}
auto Performance::recording_controls()const -> RecordingContext{return {shifted(),playing_,audio_available_,scene_take_requested_,live_recording_,resampling_,resample_cancel_pending_ || resample_cancel_request_,file_action_pending(),recording_file_busy_,history_cursor_>0,history_cursor_<history_size_,live_recording_ || recording_request_pending()?recording_quantized():record_quantized_,resample_bars_,recording_source(),record_stop_pending_ || record_stop_request_,recording_origin()};}
auto Performance::record_destination(unsigned& track,unsigned& slot,int& kit_pad) const  -> void{track=selected_;const auto& t=track_const(selected_);if(t.instrument==Instrument::DrumKit){kit_pad=selected_kit_pad_;slot=t.kit.pads[selected_kit_pad_].sample[0];}else{kit_pad=-1;slot=t.sample_slot;}}
auto Performance::library_catalog_button(unsigned i)const -> ui::ControlDescriptor<LibraryAction>{return library_button(i,shifted(),browser_.library_kits,browser_.library_originals,browser_.previewing,browser_.count>0);}
auto Performance::library_catalog_key(unsigned i) -> bool{
 const auto control=library_catalog_button(i);if(!control.enabled)return false;
 using A=LibraryAction;switch(control.action){
 case A::Kind:return request_file(FileAction::LibraryKind);
 case A::Origin:return request_file(FileAction::LibraryOriginals);
 case A::Files:set_mode(UiMode::Browser);return request_file(FileAction::Refresh);
 case A::Preview:return request_file(FileAction::Preview);case A::StopPreview:return request_file(FileAction::StopPreview);
 case A::Load:return request_file(FileAction::Load);case A::Previous:return move_preset_library(-1);case A::Next:return move_preset_library(1);
 case A::StoreFile:return request_file(FileAction::SaveCurrentPreset);case A::SaveAs:return begin_preset_save();case A::Duplicate:return begin_preset_save(true);
 case A::Back:return page_back();default:return false;
 }
}

auto Performance::set_kit_pad_at(unsigned track_index,unsigned pattern,unsigned pad,const KitPadState& next,WavView view,const SampleMetadata* metadata) -> bool{
  if(track_index>=kTracks || pattern>=kPatterns || pad>=16 || track_views_.at(pattern,track_index).instrument!=Instrument::DrumKit)return false;
  Command after{CommandType::SetKitPad,uint8_t(track_index),uint8_t(pad),next.sample[0]},before=after;after.pattern=before.pattern=uint8_t(pattern);
  after.kit_pad=next;before.kit_pad=track_views_.at(pattern,track_index).kit.pads[pad];
  if(view.data || view.reader){after.parameter=1;after.sample=view;after.metadata=metadata?*metadata:project_.assets[next.sample[0]].metadata;after.metadata.rate=view.rate;after.metadata.frames=view.frames;after.metadata.bits=view.bits;after.metadata.channels=uint8_t(view.channels);}
  return edit(after,before);
 }
auto Performance::set_kit_step(unsigned pad,unsigned step,const KitStepState& next) -> bool{
  if(pad>=16 || step>=kSteps || track_const(selected_).instrument!=Instrument::DrumKit)return false;
  Command after{CommandType::SetKitStep,selected_,uint8_t(step),0},before=after;
  after.parameter=before.parameter=uint16_t(pad);after.kit_step=next;before.kit_step=track_const(selected_).kit.steps[pad][step];bool ok=edit(after,before);if(ok)refresh_steps();return ok;
 }
auto Performance::gesture_record(bool on) -> bool{if(on && !playing_ && !play())return false;Command c{CommandType::GestureRecord,0,0,uint16_t(on)};c.parameter=384;if(!send(c))return false;gesture_recording_=on;gesture_playing_=false;return true;}
auto Performance::gesture_play(bool on) -> bool{if(on && !project_.gesture.count)return false;if(on && !playing_ && !play())return false;if(!send({CommandType::GesturePlay,0,0,uint16_t(on)}))return false;gesture_playing_=on;gesture_recording_=false;return true;}
auto Performance::mutation_set(uint32_t seed,unsigned amount,bool history) -> bool{if(!seed || amount>100)return false;const auto& old=track_const(selected_).mutation;Command after{CommandType::SetMutation,selected_,0,uint16_t(amount)},before=after;after.frames=seed;after.parameter=old.frozen && old.seed==seed;before.frames=old.seed;before.value=old.amount;before.parameter=old.frozen;if(history)return edit(after,before);if(!send(after))return false;track(selected_).mutation={seed,uint8_t(amount),after.parameter!=0};dirty_=true;return true;}
auto Performance::mutation_freeze() -> bool{auto* before=new PatternState(project_,project_.active_pattern);auto* after=new PatternState(*before);auto& target=after->tracks[selected_];target.mutation.frozen=!target.mutation.frozen;if(target.mutation.frozen)for(unsigned i=0;i<kSteps;++i)target.mutation_source[i]=target.steps[i];unsigned pattern=project_.active_pattern;uint16_t mask=uint16_t(1u<<selected_);if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);dirty_=true;return true;}
auto Performance::scene_matches_live(unsigned scene)const -> bool{if(scene>=kScenes)return false;const auto& saved=project_.scenes[scene];for(unsigned t=0;t<kTracks;++t)if(project_.clips[t].pattern!=saved.pattern_for_track[t] || project_.clips[t].playing==bool(saved.mute_mask&(1u<<t)))return false;return true;}
auto Performance::display_song_tick()const -> float{float tick=float(visual_.song_tick);if(playing_ && visual_.playing && visual_.rate)tick+=float(visual_elapsed_samples())*project_.tempo*kSongTicksPerBeat/(60.f*visual_.rate);return fminf(tick,float(visual_.song_ticks));}
auto Performance::launch_clip(unsigned t,unsigned pattern,bool quantized)  -> bool{
  if(t>=16 || pattern>=kPatterns || !send({CommandType::LaunchClip,uint8_t(t),uint8_t(quantized),uint16_t(pattern)})) return false;
  if(queued_scene_>=0){clear_launches();}pending_[t]=int8_t(pattern);queued_stops_&=uint16_t(~(1u<<t));if(t==selected_){select_pattern(pattern);following_clip_=true;} dirty_=true; return true;
 }
auto Performance::stop_clip(unsigned t,bool quantized)  -> bool{ if(t>=16 || !send({CommandType::StopClip,uint8_t(t),uint8_t(quantized),0})) return false;if(queued_scene_>=0){clear_launches();}pending_[t]=-1;queued_stops_|=uint16_t(1u<<t); dirty_=true; return true; }
auto Performance::launch_scene(unsigned scene,bool quantized)  -> bool{ if(scene>=kScenes || !send({CommandType::LaunchScene,0,uint8_t(quantized),uint16_t(scene)})) return false; clear_launches();queued_scene_=int8_t(scene);project_.chain.enabled=false;for(unsigned t=0;t<16;++t) pending_[t]=int8_t(project_.scenes[scene].pattern_for_track[t]);select_pattern(project_.scenes[scene].pattern_for_track[selected_]);following_clip_=true; dirty_=true; return true; }
auto Performance::set_clip_follow(const ClipFollow& f) -> bool{if(!valid_clip_follow(f))return false;Command after{CommandType::SetClipFollow,selected_,0,0},before=after;after.clip_follow=f;before.clip_follow=track_const(selected_).clip_follow;return edit(after,before);}
auto Performance::scene_take_status()const -> const char*{auto state=engine_.scene_take_state();if(scene_take_requested_){if(state==SceneTake::Armed)return "ARMED / clip and scene snapshot capture";if(state==SceneTake::Recording)return "RECORDING / 960 ticks per beat / END at next bar";if(state==SceneTake::Finishing)return "FINISHING / waiting for bar boundary";return "Arrangement / waiting for audio owner";}return scene_take_message_;}
auto Performance::scene_take_toggle() -> bool{
  if(scene_take_requested_)return send({CommandType::SceneTakeControl,0,1,0});
  if(playing_ || counting_in_ || note_recording_ || live_recording_ || !audio_available_ || !engine_.project_snapshot_safe() || engine_.command_space()<2){notify(UiNotice::Failed);return false;}
  if(!send({CommandType::SceneTakeControl,0,0,0}) || !send({CommandType::Start,0,0,0}))return false;
  scene_take_requested_=true;scene_take_message_[0]=0;playing_=true;seek_ready_=false;launch_quantized_=true;dirty_=true;return true;
 }
auto Performance::service_scene_take() -> void{
  if(!scene_take_requested_)return;
  auto state=engine_.scene_take_state();
  if(state==SceneTake::Failed){const char* text=engine_.scene_take_error()==SceneTake::Capacity?"Take exceeded 32 sections / original Song kept":engine_.scene_take_error()==SceneTake::Unsupported?"Song definition changed / original Song kept":"Take interrupted or cancelled / original Song kept";copy_text(scene_take_message_,sizeof scene_take_message_,text);engine_.acknowledge_scene_take();scene_take_requested_=false;notify(UiNotice::RecordLost);dirty_=true;return;}
  if(state!=SceneTake::Ready)return;
  if(engine_.project_events_pending())return;
  if(engine_.project_event_loss()){copy_text(scene_take_message_,sizeof scene_take_message_,"Project sync lost / original Song kept");engine_.acknowledge_scene_take();scene_take_requested_=false;notify(UiNotice::RecordLost);return;}
  if(engine_.command_space()<2)return;
  SongState captured;if(!engine_.copy_scene_take(captured) || !valid_song(captured))return;
  Command after{CommandType::SetSong,0,0,0},before=after;after.song_state=captured;before.song_state=project_.song;
  if(!send({CommandType::Stop,0,0,0}) || !edit(after,before))return;
  engine_.acknowledge_scene_take();scene_take_requested_=false;playing_=false;song_cursor_=0;copy_text(scene_take_message_,sizeof scene_take_message_,"Arrangement stored / one Undo restores previous Song");notify(UiNotice::Saved);dirty_=true;
 }
auto Performance::step_controls()const -> StepContext{return {shifted(),track_const(selected_).instrument==Instrument::DrumKit,project_.ui.click_main,project_.ui.count_in,project_.ui.note_quantized,project_.ui.note_replace,step_copied_,tracker_view_,step_overview_,page_,history_cursor_>0,history_cursor_<history_size_};}
auto Performance::song_controls()const -> SongControlContext{
  const auto& s=project_.song;
  return {s.length,project_.chain.length,song_cursor_,song_cursor_<s.length && s.sections[song_cursor_].ticks!=0,scene_take_requested_,playing_,counting_in_,song_import_armed_,song_clear_armed_};
 }
auto Performance::song_send(const SongState& s) -> bool{if(scene_take_requested_ || !valid_song(s))return false;Command after{CommandType::SetSong,0,0,0},before=after;after.song_state=s;before.song_state=project_.song;return edit(after,before);}
auto Performance::song_section_send(unsigned i,const SongSection& s) -> bool{if(scene_take_requested_ || i>=32 || !valid_song_section(s))return false;Command after{CommandType::SetSongSection,0,0,uint16_t(i)},before=after;after.song_section=s;before.song_section=project_.song.sections[i];return edit(after,before);}
auto Performance::song_materialize() -> bool{if(project_.song.length)return true;const auto& c=project_.chain;if(!c.length)return false;auto s=project_.song;unsigned n=c.length<32?c.length:32;for(unsigned i=0;i<n;++i){auto& sec=s.sections[i];sec.scene=c.scenes[i];unsigned b=c.follow[c.scenes[i]].bars?c.follow[c.scenes[i]].bars:c.bars;sec.bars=uint16_t(b?b:1);sec.transpose=0;sec.mute_mask=0;}s.length=uint8_t(n);s.enabled=true;s.loop=true;return song_send(s);}
auto Performance::song_add() -> bool{if(!project_.song.length && project_.chain.length)return song_materialize();if(project_.song.length>=32)return false;auto s=project_.song;auto& sec=s.sections[s.length];sec=SongSection{};sec.scene=project_.active_scene;sec.bars=uint16_t(song_scene_bars(project_,project_.active_scene));sec.transpose=0;sec.mute_mask=0;++s.length;s.enabled=true;song_cursor_=uint8_t(s.length-1);return song_send(s);}
auto Performance::song_delete() -> bool{if(!project_.song.length || song_cursor_>=project_.song.length)return false;auto s=project_.song;for(unsigned i=song_cursor_;i+1<s.length;++i)s.sections[i]=s.sections[i+1];--s.length;if(song_cursor_>=s.length && song_cursor_)--song_cursor_;return song_send(s);}
auto Performance::clear_step_locks() -> bool{if(mode_==UiMode::Effects && fx_page_==4 && track_const(selected_).instrument==Instrument::DrumKit){auto step=track_const(selected_).kit.steps[selected_kit_pad_][selected_step_];for(auto& lock:step.fx_lock)lock=255;return set_kit_step(selected_kit_pad_,selected_step_,step);}const auto& old=track_const(selected_).steps[selected_step_];auto next=old;bool any=next.lock_only;next.lock_only=false;for(auto& lock:next.locks){any|=lock.enabled;lock.enabled=false;}if(!any)return true;Command after{CommandType::SetRecordedStep,selected_,selected_step_,0},before=after;after.recorded_step=next;before.recorded_step=old;return edit(after,before);}
auto Performance::note_repeat(unsigned mode,unsigned rate) -> bool{if(mode>2 || rate>=8)return false;Command c{CommandType::SetNoteRepeat,0,uint8_t(rate),uint16_t(mode)};if(!send(c))return false;repeat_mode_=uint8_t(mode);repeat_rate_=uint8_t(rate);project_.ui.repeat_rate=uint8_t(rate);dirty_=true;return true;}
auto Performance::set_repeat_latch(bool on) -> bool{bool stop=!on && repeat_mode_ && !(controls_[2]&0x10);if(engine_.command_space()<(stop?2u:1u))return false;if(!send({CommandType::SetRepeatLatch,0,0,uint16_t(on)}))return false;repeat_latch_=on;if(stop && !note_repeat(0,repeat_rate_))return false;dirty_=true;return true;}
auto Performance::track_arp(unsigned mode,unsigned rate,unsigned octaves) -> bool{if(mode>4 || rate>3 || octaves<1 || octaves>4)return false;const auto& t=track_const(selected_);Command after{CommandType::SetTrackArp,selected_,uint8_t(rate),uint16_t(mode)},before=after;after.parameter=uint16_t(octaves);before.step=t.arp_rate;before.value=t.arp_mode;before.parameter=t.arp_octaves;return edit(after,before);}
auto Performance::repeat_config(unsigned direction,unsigned octaves,unsigned gate) -> bool{if(direction>3 || octaves<1 || octaves>4 || gate<10 || gate>100)return false;Command after{CommandType::SetRepeatConfig,0,uint8_t(direction),uint16_t(gate)},before=after;after.parameter=uint16_t(octaves);before.step=project_.ui.arp_direction;before.parameter=project_.ui.arp_octaves;before.value=project_.ui.repeat_gate;return edit(after,before);}
auto Performance::euclidean(unsigned pulses,unsigned rotation)  -> bool{
  auto& t=track(selected_); const unsigned length=t.length; if(!length || length>kSteps || pulses>length) return false;
  if(length>64 || t.instrument==Instrument::DrumKit){
   if(!engine_.command_space())return false;
   if(!allocation_admitted(sizeof(TrackHistory),2))return memory_refused();
   auto* before=alloc_new<TrackHistory>(ConstTrackView(t));if(before)before->phrase_only=true;auto* after=before?alloc_new<TrackHistory>(*before):nullptr;
   if(!before || !after){delete before;delete after;return memory_refused();}
   for(unsigned i=0;i<length;++i){unsigned phase=(i+length-(rotation%length))%length;if(t.instrument==Instrument::DrumKit)after->kit.steps[selected_kit_pad_][i].enabled=(phase*pulses)%length<pulses;else after->steps[i].enabled=(phase*pulses)%length<pulses;}
   unsigned pattern=project_.active_pattern;
   if(!engine_.replace_track_phrase(pattern,selected_,*after)){delete before;delete after;return false;}
   Command c{CommandType::ApplyPattern,selected_,1,uint16_t(pattern)};c.parameter=uint16_t(1u<<selected_);
   push_history(c,c,nullptr,nullptr,before,after);install_track_phrase(track(selected_),*after);refresh_steps();dirty_=true;return true;
  }
  if(engine_.command_space()<length){return false;}EditGroup edit_group(*this);
  for(unsigned i=0;i<length;++i) { unsigned phase=(i+length-(rotation%length))%length; bool enabled=(phase*pulses)%length<pulses; if(t.steps[i].enabled!=enabled && !set_step(i,enabled)){return false;} }
  refresh_steps(); dirty_=true; return edit_group.finish(true);
 }
auto Performance::generate_range(unsigned index,int& lo,int& hi)const -> void{
 if(gen_page_==5){auto d=harmony_knob(index);lo=d.minimum;hi=d.maximum;return;}
  lo=0;hi=1;
  if(gen_page_==1){const int low[]={-24,-48,0,0,0,0,-32,-48},high[]={24,0,48,2,64,1,32,48};lo=low[index];hi=high[index];}
  else if(gen_page_==4){const int low[]={0,0,1,0,0,0,0,0},high[]={4,3,4,1,1,1,1,1};lo=low[index];hi=high[index];}
  else if(gen_page_==2){const int low[]={0,0,1,-96,-64,0,0,0},high[]={4,100,16,96,64,1,1,1};lo=low[index];hi=high[index];}
  else{const int low[]={0,1,1,0,1,0,1,1},high[]={1,3,16,2,16,127,16,8};lo=low[index];hi=high[index];if(index==5 && gen_lane_==2)lo=1;}
 }
auto Performance::recording_led_state()const -> ui::LedState{
  if(live_recording_ || resampling_){
   const unsigned state=record_state();
   if(state==2 || state==5)return ui::LedState::Recording;
   if(state==3)return ui::LedState::Finalizing;
   return ui::LedState::Armed;
  }
  if(recording_request_pending())return ui::LedState::Armed;
  if(recording_file_busy_)return ui::LedState::Finalizing;
  if(gesture_recording_ || midi_capture_state_ || (note_recording_ && playing_))return ui::LedState::Recording;
  return note_recording_ || counting_in_?ui::LedState::Armed:ui::LedState::Off;
 }
auto Performance::recording_track_led(unsigned track)const -> ui::LedState{
  if(resampling_ && resample_view().target.track==track)return recording_led_state();
  if((live_recording_ || recording_request_pending()) && recording_target().track==track)return recording_led_state();
  if(note_recording_ && note_record_track_==track)return playing_?ui::LedState::Recording:ui::LedState::Armed;
  return ui::LedState::Off;
 }
auto Performance::store_scene(unsigned slot) -> bool{if(slot>=16)return false;if(queued_scene_>=0 || queued_stops_){notify(UiNotice::SceneWait);return false;}for(int pending:pending_)if(pending>=0){notify(UiNotice::SceneWait);return false;}Command after{CommandType::SetScene,0,0,uint16_t(slot)},before=after;before.scene=project_.scenes[slot];for(unsigned t=0;t<16;++t){after.scene.pattern_for_track[t]=project_.clips[t].pattern;if(!project_.clips[t].playing)after.scene.mute_mask|=uint16_t(1u<<t);}scene_slot_=uint8_t(slot);if(!edit(after,before))return false;notify(UiNotice::SceneStored);return true;}
auto Performance::loop_take() -> bool{
  const auto& current=track_const(selected_);const auto& asset=project_.assets[current.sample_slot];const auto& meta=asset.metadata;
  if(current.instrument!=Instrument::Sampler || !asset.path[0] || !meta.rate || !meta.frames || meta.missing){loop_confirm_=false;notify(UiNotice::LoopInvalid);return false;}
  uint64_t actual=uint64_t(meta.frames)*project_.tempo,one=uint64_t(meta.rate)*240;unsigned bars=unsigned((actual+one/2)/one);
  uint64_t expected=one*bars,difference=actual>expected?actual-expected:expected-actual;
  if(bars<1 || bars>4 || difference>uint64_t(project_.tempo)*2){loop_confirm_=false;notify(UiNotice::LoopInvalid);return false;}
  if(!loop_confirm_ || loop_track_!=selected_ || loop_pattern_!=project_.active_pattern || loop_slot_!=current.sample_slot || loop_generation_!=engine_.staged_generation()){
   loop_confirm_=true;loop_track_=selected_;loop_pattern_=project_.active_pattern;loop_slot_=current.sample_slot;loop_generation_=engine_.staged_generation();notify(UiNotice::LoopConfirm);return true;
  }
  if(engine_.command_space()<2){notify(UiNotice::Failed);return false;}
  auto* before=new PatternState(project_,project_.active_pattern);before->include_settings=true;auto* after=new PatternState(*before);auto& t=after->tracks[selected_];
  t.clip_follow=ClipFollow{};t.length=uint8_t(bars*16);t.speed_num=t.speed_den=1;t.direction=PlaybackDirection::Forward;t.sample_edit=SampleEdit{};t.sample_edit.fade_ms=0;t.sample_loop=t.sample_reverse=false;t.sample_slice=0;t.sample_slices=1;t.root_note=meta.root;t.transpose=0;t.scale_mask=0x0fff;t.velocity_scale=1;t.accumulator=AccumulatorState{};t.groove=GrooveState{};t.lanes_enabled=false;for(auto& lane:t.automation)lane=AutomationLane{};StepState empty_step{};for(auto& step:t.steps)__builtin_memcpy(&step,&empty_step,sizeof step);t.steps[0].enabled=true;t.steps[0].note=60;t.steps[0].velocity=127;
  unsigned pattern=project_.active_pattern;uint16_t mask=uint16_t(1u<<selected_);if(!engine_.replace_pattern(pattern,*after,mask)){delete before;delete after;notify(UiNotice::Failed);return false;}
  Command c{CommandType::ApplyPattern,0,0,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);loop_confirm_=false;refresh_steps();
  if(!launch_clip(selected_,pattern,true)){notify(UiNotice::Failed);return false;}notify(UiNotice::LoopReady);return true;
 }
auto Performance::chop_to_steps() -> bool{const auto& source=track_const(selected_);if(source.instrument==Instrument::DrumKit){
  bool assigned=false;for(const auto& pad:source.kit.pads)assigned|=(pad.sample[0]<kSampleAssets && project_.assets[pad.sample[0]].path[0]) || (pad.sample[1]<kSampleAssets && project_.assets[pad.sample[1]].path[0]);if(!assigned){notify(UiNotice::Failed);return false;}
  unsigned pattern=project_.active_pattern;auto* before=new PatternState(project_,pattern);auto* after=new PatternState(*before);auto& target=after->tracks[selected_];if(target.length<16)target.length=16;for(unsigned pad=0;pad<16;++pad)for(unsigned step=0;step<16;++step){KitStepState hit{};hit.enabled=pad==step && ((target.kit.pads[pad].sample[0]<kSampleAssets && project_.assets[target.kit.pads[pad].sample[0]].path[0]) || (target.kit.pads[pad].sample[1]<kSampleAssets && project_.assets[target.kit.pads[pad].sample[1]].path[0]));target.kit.steps[pad][step]=hit;}
  uint16_t mask=uint16_t(1u<<selected_);if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;notify(UiNotice::Failed);return false;}Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();copy_text(browser_.status,sizeof browser_.status,"Chop phrase / first 16 steps replaced");notify(UiNotice::ChopPhrase);return true;
  }unsigned count=source.sample_edit.slice_count?source.sample_edit.slice_count:source.sample_slices;if(source.instrument!=Instrument::Sampler || count<2 || !project_.assets[source.sample_slot].path[0]){notify(UiNotice::Failed);return false;}auto* chopped=new TrackState(source);chopped->length=16;for(unsigned i=0;i<16;++i){StepState step{};step.enabled=true;step.note=source.root_note;step.velocity=110;step.locks[0]={301,float(source.sample_edit.pads[i]%count),true};chopped->steps[i]=step;}bool ok=replace_track_sequence(project_.active_pattern,selected_,*chopped);delete chopped;if(!ok)notify(UiNotice::Failed);return ok;}
auto Performance::copy_step() -> bool{step_clipboard_is_kit_=track_const(selected_).instrument==Instrument::DrumKit;if(step_clipboard_is_kit_)kit_step_clipboard_=track_const(selected_).kit.steps[selected_kit_pad_][selected_step_];else step_clipboard_=track_const(selected_).steps[selected_step_];step_copied_=true;notify(UiNotice::StepCopied);return true;}
auto Performance::paste_step() -> bool{
  if(!step_copied_)return false;
  bool target_kit=track_const(selected_).instrument==Instrument::DrumKit;
  if(target_kit){auto next=step_clipboard_is_kit_?kit_step_clipboard_:track_const(selected_).kit.steps[selected_kit_pad_][selected_step_];if(!step_clipboard_is_kit_){next.enabled=step_clipboard_.enabled;next.velocity=step_clipboard_.velocity;next.probability=step_clipboard_.probability;next.ratchet=step_clipboard_.ratchet;int micro=step_clipboard_.microtiming;next.microtiming=int8_t(bounded((micro+(micro>=0?2:-2))/4,-48,48));}bool ok=set_kit_step(selected_kit_pad_,selected_step_,next);if(ok)notify(step_clipboard_is_kit_?UiNotice::StepPasted:UiNotice::StepRhythmPasted);return ok;}
  const auto& current=track_const(selected_).steps[selected_step_];auto next=step_clipboard_is_kit_?current:step_clipboard_;if(step_clipboard_is_kit_){next.enabled=kit_step_clipboard_.enabled;next.velocity=kit_step_clipboard_.velocity;next.probability=kit_step_clipboard_.probability;next.ratchet=kit_step_clipboard_.ratchet;next.microtiming=int16_t(kit_step_clipboard_.microtiming)*4;}
  if(same_step(current,next)){if(step_clipboard_is_kit_)notify(UiNotice::StepRhythmPasted);return true;}Command after{CommandType::SetRecordedStep,selected_,selected_step_,0},before=after;after.recorded_step=next;before.recorded_step=current;bool ok=edit(after,before);if(ok)notify(step_clipboard_is_kit_?UiNotice::StepRhythmPasted:UiNotice::StepPasted);return ok;
 }
auto Performance::duplicate_step() -> bool{if(!copy_step())return false;unsigned limit=track_const(selected_).length;if(limit>kSteps)limit=kSteps;if(unsigned(selected_step_)+1>=limit){notify(UiNotice::Failed);return false;}selected_step_++;refresh_steps();bool ok=paste_step();dirty_=true;return ok;}
auto Performance::toggle_step_chord(unsigned note) -> bool{
  if(note>127)return false;
  const auto& step=track_const(selected_).steps[selected_step_];StepState next=step;
  if(!next.enabled){next.enabled=true;next.note=uint8_t(note);for(auto& tone:next.chord)tone=255;}
  else if(next.note==note){if(next.chord[0]<128){next.note=next.chord[0];for(unsigned i=1;i<kChordTones;++i)next.chord[i-1]=next.chord[i];next.chord[kChordTones-1]=255;}else next.enabled=false;}
  else{unsigned slot=0;while(slot<kChordTones && next.chord[slot]!=note)++slot;if(slot==kChordTones){slot=0;while(slot<kChordTones && next.chord[slot]!=255)++slot;}if(slot==kChordTones){notify(UiNotice::ChordFull);return false;}if(next.chord[slot]==note){for(unsigned i=slot+1;i<kChordTones;++i)next.chord[i-1]=next.chord[i];next.chord[kChordTones-1]=255;}else next.chord[slot]=uint8_t(note);}
  Command after{CommandType::SetRecordedStep,selected_,selected_step_,0},before=after;after.recorded_step=next;before.recorded_step=step;bool ok=edit(after,before);if(ok){unsigned count=next.enabled?1:0;for(auto tone:next.chord)count+=tone<128;const auto& t=track_const(selected_);if(count>instrument_voice_limit(t.instrument,t.plaits_model))notify(instrument_voice_limit(t.instrument,t.plaits_model)==1?UiNotice::ChordMono:UiNotice::ChordVoiceLimit);refresh_steps();}return ok;
 }
auto Performance::set_step_note(unsigned step,unsigned note) -> bool{if(step>=kSteps || note>127)return false;Command after{CommandType::SetStepNote,selected_,uint8_t(step),uint16_t(note)},before=after;before.value=track_const(selected_).steps[step].note;return edit(after,before);}
auto Performance::step_locked(uint16_t id) const  -> bool{ if(!id) return false; const auto& t=track_const(selected_); if(t.instrument==Instrument::DrumKit) return false; const auto& st=t.steps[selected_step_]; for(unsigned li=0;li<kLocks;++li) if(st.locks[li].enabled && st.locks[li].id==id) return true; return false; }
auto Performance::trigger_kit_pad(uint8_t hardware,unsigned pad,uint16_t pressure) -> bool{
  if(pad>=16 || pending_release_[hardware])return false;
  selected_kit_pad_=uint8_t(pad);refresh_steps();dirty_=true;if(shifted())return true;
  Command c{CommandType::Trigger,selected_,uint8_t(pad),uint16_t(fixed_velocity_?127:pad_velocity(pressure,device_.velocity))};c.parameter=4;c.scalar=float(hardware);
  if(!send(c))return false;
  held_notes_[hardware]=uint8_t(pad);held_tracks_[hardware]=selected_;held_kind_[hardware]=4;notes_held_[hardware]=true;return true;
 }
auto Performance::set_step(unsigned step,bool enabled)  -> bool{ if(step>=kSteps)return false;if(track_const(selected_).instrument==Instrument::DrumKit){auto hit=track_const(selected_).kit.steps[selected_kit_pad_][step];hit.enabled=enabled;return set_kit_step(selected_kit_pad_,step,hit);}Command after{CommandType::SetStep,selected_,uint8_t(step),uint16_t(enabled)},before=after; before.value=track(selected_).steps[step].enabled; bool ok=edit(after,before); refresh_steps(); return ok; }
auto Performance::note_assignment_changes(unsigned pattern,const PatternState& before,const PatternState& after,uint16_t mask) -> void{if(pattern>=kPatterns)return;for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t)){const auto& a=before.tracks[t];const auto& b=after.tracks[t];if(a.instrument!=b.instrument || a.sample_slot!=b.sample_slot)++assignment_revision_[t];}}
auto Performance::note_assignment_changes(unsigned pattern,const PatternState& after,uint16_t mask) -> void{if(!after.include_settings)return;for(unsigned t=0;t<kTracks;++t)if(mask&(1u<<t)){const auto& a=project_.tracks[t];const auto& b=after.tracks[t];if(a.instrument!=b.instrument || a.sample_slot!=b.sample_slot)++assignment_revision_[t];}}
auto Performance::rebase_record(EngineEvent& event)const -> bool{uint32_t current=engine_.pattern_revision(event.flags,event.track),revision=event.pattern_revision;for(unsigned count=0;revision!=current && count<128;++count){const RotationRevision* found=nullptr;for(unsigned i=0;i<128;++i){const auto& candidate=rotation_revisions_[i];if(candidate.length && candidate.pattern==event.flags && candidate.track==event.track && candidate.from==revision){found=&candidate;break;}}if(!found || event.value>=found->length)return false;event.value=uint8_t(found->direction>0?(event.value+1)%found->length:(event.value+found->length-1)%found->length);revision=found->to;}return revision==current;}
auto Performance::remember_record(unsigned pattern,unsigned track,unsigned step,uint32_t id) -> void{forget_record(pattern,track,step);for(auto& r:record_ids_)if(!r.id){r={id,uint8_t(pattern),uint8_t(track),uint8_t(step)};return;}record_ids_[record_cursor_]={id,uint8_t(pattern),uint8_t(track),uint8_t(step)};record_cursor_=uint8_t((record_cursor_+1)%64);notify(UiNotice::RecordLost);}
auto Performance::refresh_steps()  -> void{const auto& t=track(selected_);for(unsigned i=0;i<16;++i)step_view_[i]=uint8_t(t.instrument==Instrument::DrumKit?t.kit.steps[selected_kit_pad_][page_*16+i].enabled:t.steps[page_*16+i].enabled);}
}
