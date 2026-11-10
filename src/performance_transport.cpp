// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "performance.h"
namespace groove {
auto Performance::confirm_erase(UiErase action) -> bool{if(modal_active())return false;erase_track_=selected_;erase_pattern_=project_.active_pattern;erase_generation_=generation_;modal_.owner=ModalOwner::UiCommand;modal_.value=uint8_t(action);modal_.sync();dirty_=true;return true;}
auto Performance::confirmation_heading()const -> const char*{switch(modal_.owner){case ModalOwner::UiCommand:return erase_title();case ModalOwner::Update:return "Firmware update?";case ModalOwner::SongImport:return "Import chain into Song?";case ModalOwner::SongClear:return "Clear Song?";case ModalOwner::SourceDiscard:return "Replace unknown source?";case ModalOwner::Extend:return "Extend pattern?";case ModalOwner::Loop:return "Replace loop take?";case ModalOwner::Reference:return "Write reference?";default:return "Confirm operation?";}}
auto Performance::erase_title()const -> const char*{static const char* names[]={"","Delete song section?","Delete audio clip?","Clear audio track?","Replace all 16 tracks?","Replace track steps?","Replace slice markers?","Remove layer?","Clear gesture?","Clear zones?","Import WAV loop?"};return erase_confirmation() && modal_.value<11?names[modal_.value]:"";}
auto Performance::accept_erase() -> bool{if(!erase_confirmation())return false;auto action=UiErase(modal_.value);modal_.owner=ModalOwner::None;modal_.value=0;modal_.sync();if(erase_track_!=selected_ || erase_pattern_!=project_.active_pattern || erase_generation_!=generation_)return false;
 switch(action){case UiErase::SongDelete:return song_delete();case UiErase::AudioDelete:return erase_audio(false);case UiErase::AudioClear:return erase_audio(true);case UiErase::MapPads:return request_file(FileAction::MapPads);case UiErase::ChopSteps:return chop_to_steps();case UiErase::AutoChop:return request_file(FileAction::AutoSlice);case UiErase::FileLoop:return request_file(FileAction::ImportWavLoop);case UiErase::LayerRemove:return layer_use(true);case UiErase::ZonesClear:{auto e=track_const(selected_).instrument==Instrument::DrumKit?track_const(selected_).kit.pads[selected_kit_pad_].edit:track_const(selected_).sample_edit;e.zone_count=0;return sample_edit(e);}
 case UiErase::GestureClear:if(!send({CommandType::GestureClear,0,0,0}))return false;project_.gesture.count=0;return true;}return false;}
auto Performance::dispatch(const ControllerInput& input)  -> bool{
  if(transfer_view_.active && input.type!=ControllerInput::Type::Control && input.type!=ControllerInput::Type::SoftKey)return false;
  if(modal_active())switch(input.type){
   case ControllerInput::Type::Control:case ControllerInput::Type::SoftKey:case ControllerInput::Type::Pad:
   case ControllerInput::Type::KnobTouch:case ControllerInput::Type::Knob:case ControllerInput::Type::Encoder:
   case ControllerInput::Type::MuteModifier:case ControllerInput::Type::SoloModifier:
   case ControllerInput::Type::Shift:case ControllerInput::Type::Stop:case ControllerInput::Type::Panic:case ControllerInput::Type::Strip:break;
   case ControllerInput::Type::Fill:case ControllerInput::Type::Stutter:case ControllerInput::Type::ControlAll:if(!input.value)break;return false;
   case ControllerInput::Type::Play:if(browser_editing())break;return false;
   default:return false;
  }
  switch(input.type) {
   case ControllerInput::Type::Pad:return pad(input.index,uint8_t(input.value),input.pressure);
   case ControllerInput::Type::KnobTouch:if(input.index>=8)return false;knob_overlay_until_[input.index]=now_ms_+1200;controls_[9]=uint8_t((controls_[9]&~(0x80>>input.index))|(input.value?(0x80>>input.index):0));dirty_=true;return true;
   case ControllerInput::Type::Knob:return knob(input.index,int16_t(input.value));
   case ControllerInput::Type::Mode:if(input.value<0 || input.value>int(UiMode::Library))return false;set_mode(UiMode(input.value));return true;
   case ControllerInput::Type::Strip:return strip_input(input.value,16383);
   case ControllerInput::Type::Shift:set_shift(input.value!=0);return true;
   case ControllerInput::Type::Play:return play(); case ControllerInput::Type::Stop:return stop();
   case ControllerInput::Type::Track:return select_track(input.index); case ControllerInput::Type::Pattern:return select_pattern(input.index);
   case ControllerInput::Type::Euclidean:return euclidean(unsigned(input.value),input.index);
   case ControllerInput::Type::StoreMorph:return store_morph(input.index);
   case ControllerInput::Type::Fill:return set_fill(input.value!=0);
   case ControllerInput::Type::Stutter:return stutter(input.value!=0);
   case ControllerInput::Type::Instrument:return cycle_instrument();
   case ControllerInput::Type::StepPage:step_page(input.index);return true;
   case ControllerInput::Type::SoftKey:return soft_key(input.index);
   case ControllerInput::Type::MuteModifier:return modifier(0x20,input.value!=0);
   case ControllerInput::Type::SoloModifier:return modifier(0x10,input.value!=0);
   case ControllerInput::Type::ControlAll:return set_control_all(input.value!=0);
   case ControllerInput::Type::WaveProfile:return set_wave_profile(input.value!=0);
   case ControllerInput::Type::Panic:return panic();
   case ControllerInput::Type::Control:if(input.value<0 || input.value>255)return false;return control(input.index,uint8_t(input.value));
   case ControllerInput::Type::Encoder:return encoder(bounded(input.value,-16,16));
   case ControllerInput::Type::Undo:return undo(); case ControllerInput::Type::Redo:return redo();
  } return false;
 }
auto Performance::set_multitimbral(bool on) -> bool{if(live_recording_ || resampling_ || recording_file_busy_)return false;if(!send({CommandType::SetMultitimbral,0,0,uint16_t(on)}))return false;playing_=counting_in_=note_recording_=false;seek_ready_=false;clear_pad_feedback();clear_launches();notify(on?UiNotice::ModuleEntered:UiNotice::ModuleExited);dirty_=true;return true;}
auto Performance::play()  -> bool{if(module_transport_blocked())return false; if((controls_[0]&0x40) && mode_==UiMode::Scene && project_.chain.length){if(!send({CommandType::SetChainEnabled,0,0,1}))return false;project_.chain.enabled=true;} bool count=note_recording_ && project_.ui.count_in && !project_.midi_clock_slave && !playing_ && !seek_ready_;CommandType action=count?CommandType::StartCountIn:seek_ready_?CommandType::Continue:CommandType::Start;if(!send({action,0,uint8_t(seek_ready_?2:0),0})) return false;seek_ready_=false;counting_in_=count;count_beat_=count?1:0;playing_=true; return true; }
auto Performance::restart_transport(bool quantized) -> bool{if(module_transport_blocked())return false;if(!send({CommandType::Start,0,0,uint16_t(quantized && playing_)}))return false;if(quantized && playing_)notify(UiNotice::RestartQueued);counting_in_=false;count_beat_=0;seek_ready_=false;playing_=true;dirty_=true;return true;}
auto Performance::stop()  -> bool{ if(twin_modal_==2)request_file(FileAction::CancelTwin);if(!send({CommandType::Stop,0,0,0})) return false;strip_active_=strip_note_active_=strip_release_pending_=false; performance_fx_mode_=PerformanceFx::Off;performance_fx_latched_=false;held_performance_pad_=255;release_performance_fx_=false; playing_=false;counting_in_=false;seek_ready_=false;count_beat_=0;clear_pad_feedback();clear_launches();if(following_clip_){project_.active_pattern=project_.clips[selected_].pattern;refresh_steps();}return true; }
auto Performance::panic() -> bool{strip_active_=strip_note_active_=strip_release_pending_=false;note_finish_pending_=note_recording_ || has_record_ids();engine_.request_panic();queued_scene_=-1;queued_stops_=0;clear_pad_feedback();playing_=control_all_=fill_active_=stutter_active_=note_recording_=counting_in_=false;repeat_mode_=0;seek_ready_=false;repeat_latch_=repeat_ignore_release_=false;repeat_release_pending_=false;count_beat_=0;control_all_position_=0.5f;held_stutter_pad_=held_fill_pad_=255;performance_fx_mode_=PerformanceFx::Off;performance_fx_latched_=false;held_performance_pad_=255;release_performance_fx_=false;for(unsigned i=0;i<16;++i){held_[i]=notes_held_[i]=pending_release_[i]=false;pressures_[i]=0;pending_[i]=-1;}dirty_=true;return true;}
auto Performance::home_or_return()  -> bool{home_navigation_pending_=page_left_down_ && page_right_down_;
  if(modal_active())return false;
  if(pending_project_!=ProjectAction::None) return false;
  if(update_confirm_) return false;
  if(mode_!=UiMode::Pad) {
   ticket_=navigation(); ticket_valid_=true;
   apply_mode(UiMode::Pad); return true;
  }
  if(!ticket_valid_) return false;
  ReturnTicket ticket=ticket_; ticket_valid_=false;
  restore_navigation(ticket);
  dirty_=true; return true;
 }
auto Performance::resampling(bool on,const ResampleRequest* request) -> void{resampling_=on;if(on){active_resample_=request?*request:queued_resample_;active_record_origin_=RecordingOrigin::Resample;}else resample_cancel_pending_=resample_cancel_request_=false;dirty_=true;}
auto Performance::set_audio_lane(unsigned t,const AudioLane& a,bool history,unsigned slot,WavView view,SampleMetadata metadata) -> bool{
  if(t>=16 || !valid_audio_lane(a))return false;
  unsigned count=0;for(unsigned i=0;i<16;++i)if(i!=t && project_.audio[i].kind!=TrackKind::Instrument)++count;
  if(a.kind!=TrackKind::Instrument && count>=kAudioLanes){copy_text(browser_.status,sizeof browser_.status,"Maximum 4 audio / looper tracks");dirty_=true;return false;}
  Command c{CommandType::SetAudioLane,uint8_t(t),0,uint16_t(slot)};c.audio_lane=a;c.sample=view;c.metadata=metadata;
  if(!send(c))return false;
  if(history){audio_undo_[t]=project_.audio[t];audio_undo_valid_[t]=true;}
  project_.audio[t]=a;++audio_revision_[t];dirty_=true;return true;
 }
auto Performance::common_top(unsigned index)const -> ui::ControlDescriptor<CommonTopAction>{
  if(modal_active() || repeat_mode_ || audio_page() || browser_editing() || (index==7 && (bake_control() || (mode_==UiMode::Synth && shifted()))))return {};
  if(shifted() && index==5 && mode_==UiMode::Synth)return {"TRACK NAME",CommonTopAction::TrackName,true};
  bool synth=mode_==UiMode::Synth;
  bool slots=!source_select_ && !kit_edit_page() && (mode_==UiMode::Pad || mode_==UiMode::Keyboard || mode_==UiMode::Chord || mode_==UiMode::Step || mode_==UiMode::Synth || mode_==UiMode::Modulation || mode_==UiMode::Generate || (mode_==UiMode::Effects && fx_page_>=3));
  bool store=slots,recall=slots && project_.sounds[sound_slot_].instrument!=Instrument::Empty;
  if(mode_==UiMode::Step && shifted())store=recall=false;
  if(mode_==UiMode::Effects && ((fx_page_==5) || (track_const(selected_).instrument==Instrument::DrumKit && !shifted())))store=false;
  if(mode_==UiMode::Perform && !shifted())store=false;
  bool valid=track_const(selected_).instrument!=Instrument::Empty && track_const(selected_).instrument!=Instrument::Unknown;
  bool available=project_.sounds[sound_slot_].instrument!=Instrument::Empty && project_.sounds[sound_slot_].instrument!=Instrument::Unknown;
  auto d=common_top_control(index,synth,store,recall,valid,available,mode_==UiMode::Perform);if(d.action==CommonTopAction::Source)d.selected=source_select_;return d;
 }
auto Performance::top_slot(unsigned index)const -> unsigned{
  if(index>=8)return 8;
  if(modal_active() || browser_editing() || repeat_mode_)return index;
  if(index==7 && bake_control())return 7;
  if(audio_page()){static const uint8_t normal[]={0,1,8,8,2,3,4,7},shift[]={0,8,8,8,2,6,5,1};return (shifted()?shift:normal)[index];}
  if(mode_==UiMode::Clip){static const uint8_t slots[]={4,5,8,8,0,1,2,3};return slots[index];}
  if(mode_==UiMode::Scene){static const uint8_t slots[]={1,2,4,8,0,3,5,8};return slots[index];}
  if(mode_==UiMode::Song){if(index<4)return 8;static const uint8_t slots[]={4,5,0,1};return shifted() && index>=6?index-4:slots[index-4];}
  if(mode_==UiMode::Mixer){static const uint8_t normal[]={8,8,8,8,0,1,6,7},shift[]={8,8,8,8,5,1,6,7};return (shifted()?shift:normal)[index];}
  if(mode_==UiMode::Synth && !source_select_ && track_const(selected_).instrument==Instrument::Layer){const uint8_t slots[]={0,1,uint8_t(layer_page()?2:8),uint8_t(layer_page()?3:8),4,5,6,7};return slots[index];}
  if(mode_==UiMode::Synth && wave_profile()){static const uint8_t slots[]={0,1,8,8,4,5,2,3};return slots[index];}
  if(mode_==UiMode::Effects && fx_page_==3 && shifted()){static const uint8_t slots[]={0,1,2,8,5,4,6,7};return slots[index];}
  if(mode_==UiMode::Effects && fx_page_!=5){static const uint8_t slots[]={0,1,2,5,3,4,6,7};return slots[index];}
  if(mode_==UiMode::Routing && routing_fx_page_){static const uint8_t normal[]={0,4,2,3,8,5,1,8};if(!shifted() && !live_recording_)return normal[index];const uint8_t busy[]={0,uint8_t(group_send_page_ || !shifted()?4:8),2,8,3,uint8_t(shifted() && !group_send_page_?4:5),1,8};return busy[index];}
  if(mode_==UiMode::Routing && !routing_fx_page_){static const uint8_t normal[]={0,8,2,4,3,5,1,8},shift[]={0,8,2,5,3,4,1,8};return (shifted()?shift:normal)[index];}
  return index;
 }
auto Performance::input_top(unsigned i)const -> bool{return !audio_page() && !kit_edit_page() && !source_select_ && !repeat_mode_ && !browser_editing() && !modal_active() && ((mode_==UiMode::Keyboard || mode_==UiMode::Chord) || ((mode_==UiMode::Step || mode_==UiMode::Pad) && i>=4 && (!shifted() || (mode_==UiMode::Pad && (i==4 || i==5)))));}
auto Performance::input_scale(int delta) -> bool{const auto& t=track_const(selected_);unsigned at=scale_index(t.scale_mask);if(at>=kScalePresets)at=0;unsigned next=input_cycle(int(at)+delta,kScalePresets);Command after{CommandType::SetScale,selected_,t.scale_root,scale_presets()[next].mask},before=after;before.value=t.scale_mask;return edit(after,before);}
auto Performance::input_root(int note) -> bool{const auto& t=track_const(selected_);note=bounded(note,0,127);if(engine_.command_space()<2)return false;EditGroup edit_group(*this);Command pitch{CommandType::SetNote,selected_,0,uint16_t(note)},old_pitch=pitch;old_pitch.value=t.root_note;Command scale{CommandType::SetScale,selected_,uint8_t(note%12),t.scale_mask},old_scale=scale;old_scale.step=t.scale_root;bool ok=edit(pitch,old_pitch) && edit(scale,old_scale);return edit_group.finish(ok);}
auto Performance::input_octave(int delta) -> bool{int next=bounded(int(project_.ui.keyboard_octave[selected_])+delta,-4,4);if(next==project_.ui.keyboard_octave[selected_])return false;project_.ui.keyboard_octave[selected_]=int8_t(next);release_input_pads();dirty_=true;return true;}
auto Performance::input_shape(int delta,bool inversion) -> bool{Command after{CommandType::SetChordConfig,0,0,project_.ui.chord_shape},before=after;after.parameter=before.parameter=project_.ui.chord_inversion;if(inversion)after.parameter=uint16_t(input_cycle(int(after.parameter)+delta,4));else after.value=uint16_t(input_cycle(int(after.value)+delta,kChordShapes));return edit(after,before);}
auto Performance::soft_presentation(unsigned i)const -> ui::ControlPresentation{if(transfer_view_.active){using A=SettingsAction;if(i<2)return ui::ControlDescriptor<A>{transfer_view_.usb?"":i?"STATION":"AP",i?A::TransferStation:A::TransferAP,!transfer_view_.started && !transfer_view_.failed};if(i==4)return ui::ControlDescriptor<A>{transfer_view_.usb?"":"START",A::StartTransfer,!transfer_view_.started && !transfer_view_.failed};if(i==5)return ui::ControlDescriptor<A>{"BROWSER",A::BrowseTransfer,true};return ui::ControlDescriptor<A>{i==7?"CLOSE":"",A::CloseTransfer,i==7};}if(library_catalog_controls())return library_catalog_button(i);if(input_top(i))return input_button(i,mode_==UiMode::Chord,shifted(),history_cursor_>0,history_cursor_<history_size_,track_const(selected_).instrument==Instrument::DrumKit);if(source_select_ && !modal_active()){if(i==5 && shifted())return ui::ControlDescriptor<CommonTopAction>{"TRACK NAME",CommonTopAction::TrackName,true};if(i==4)return ui::ControlDescriptor<CommonTopAction>{"LOAD ENGINE",CommonTopAction::MountSource,true};if(i==7)return ui::ControlDescriptor<CommonTopAction>{"BACK",CommonTopAction::CloseSource,true};return ui::ControlDescriptor<CommonTopAction>{};}unsigned slot=top_slot(i);if(slot>=8 || ((source_select_ || kit_edit_page()) && i>=6))return ui::ControlDescriptor<CommonTopAction>{};auto shown=page_top_presentation(slot);if(wave_profile() && shifted() && i==7){shown.label="EXIT PROFILE";shown.work=ui::TopWork::Exit;}return shown;}
auto Performance::soft_enabled(unsigned i)const -> bool{if(transfer_view_.active)return i==7 || i==5 || ((i<2 || i==4) && !transfer_view_.started && !transfer_view_.failed);if(library_catalog_controls())return library_catalog_button(i).enabled;if(input_top(i))return input_button(i,mode_==UiMode::Chord,shifted(),history_cursor_>0,history_cursor_<history_size_,track_const(selected_).instrument==Instrument::DrumKit).enabled;if(source_select_ && !modal_active())return i==4 || i==7 || (i==5 && shifted());unsigned slot=top_slot(i);return slot<8 && !((source_select_ || kit_edit_page()) && i>=6) && page_top_enabled(slot);}
auto Performance::status_control(unsigned index)const -> StatusControl{return header_status(index,{playing_,live_recording_ || gesture_recording() || (note_recording_ && playing_),note_recording_,follow_display_,lock_path(),project_.ui.metronome,project_.midi_clock_slave,axes_.strip_mode});}
auto Performance::sn_ref_confirmation()const -> bool{return sn_ref_armed_ && mode_==UiMode::Synth && wave_clock() && sn_ref_track_==selected_ && sn_ref_pattern_==project_.active_pattern && sn_ref_channel_==track_const(selected_).midi_channel && now_ms_>=sn_ref_armed_ms_ && now_ms_-sn_ref_armed_ms_<5000;}
auto Performance::mute_or_solo(unsigned t,bool solo,bool pattern_layer) -> bool{if(t>=16)return false;bool current=arrangement_ui_active_?(solo?bool(arrangement_ui_solo_&(1u<<t)):bool(arrangement_ui_mute_&(1u<<t))):solo?track_solo(project_,t):(pattern_layer?track_muted_pattern(project_,t):track_muted_global(project_,t));Command after{solo?CommandType::SetSolo:CommandType::SetMute,uint8_t(t),0,uint16_t(!current)},before=after;before.value=uint16_t(current);after.pattern=before.pattern=project_.clips[t].pattern;after.parameter=before.parameter=uint16_t(pattern_layer && !solo?1:0);return edit(after,before);}
auto Performance::creative(const CreativeState& c) -> bool{if(!valid_creative(c))return false;Command after{CommandType::SetCreative,selected_,0,0},before=after;after.creative=c;before.creative=track_const(selected_).creative;return edit(after,before);}
auto Performance::dice() -> bool{unsigned pattern=project_.active_pattern;uint16_t mask=uint16_t(1u<<selected_);auto* before=new PatternState(project_,pattern);auto* after=new PatternState(*before);auto& t=after->tracks[selected_];uint32_t seed=t.creative.seed+0x9e3779b9;for(unsigned i=0;i<t.length;++i){seed=next_random(seed);if(t.instrument==Instrument::DrumKit){auto& hit=t.kit.steps[selected_kit_pad_][i];if(seed%4==0)hit.enabled=!hit.enabled;hit.velocity=uint8_t(bounded(int(hit.velocity)+int((seed>>8)%25)-12,1,127));}else{if(seed%4==0)t.steps[i].enabled=!t.steps[i].enabled;t.steps[i].velocity=uint8_t(bounded(int(t.steps[i].velocity)+int((seed>>8)%25)-12,1,127));}}t.creative.seed=seed;if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();return true;}
auto Performance::rotate_track(int direction) -> bool{
  if(direction!=1 && direction!=-1){return false;}unsigned pattern=project_.active_pattern;uint16_t mask=uint16_t(1u<<selected_);auto* before=new PatternState(project_,pattern);auto* after=new PatternState(*before);const auto& source=before->tracks[selected_];auto& target=after->tracks[selected_];unsigned length=source.length;
  rotate_sequence_steps(target,source,length,direction);
  uint32_t previous=engine_.pattern_revision(pattern,selected_);if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;return false;}remember_rotation(pattern,selected_,previous,engine_.pattern_revision(pattern,selected_),length,direction);Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;c.pattern_revision=kRotationHistory;c.scalar=float(direction);c.scalar2=float(length);push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();return true;
 }
auto Performance::accept_event(const EngineEvent& input)  -> void{
  EngineEvent adjusted=input;const bool chromatic=input.type==EngineEventType::RecordedNote && (input.parameter&128);const bool native_drum=input.type==EngineEventType::RecordedNote && (input.parameter&256);if(input.type==EngineEventType::RecordedNote)adjusted.parameter&=127;const EngineEvent& event=adjusted;
  if(event.generation!=generation_)return;
  if((event.type==EngineEventType::RecordedNote || event.type==EngineEventType::RecordedKitHit || event.type==EngineEventType::RecordedGate || event.type==EngineEventType::RecordErase) && event.track<kTracks && event.flags<kPatterns && !rebase_record(adjusted)){notify(UiNotice::RecordLost);return;}
  if(event.type==EngineEventType::RecordErase && event.track<16 && event.flags<kPatterns){auto& step=track_views_.at(event.flags,event.track).steps[event.value];if(!same_step(step,event.prior_step)){notify(UiNotice::RecordLost);return;}Command after{CommandType::SetRecordedStep,event.track,event.value,0},before=after;after.pattern=before.pattern=event.flags;after.recorded_step=step;after.recorded_step.enabled=false;before.recorded_step=step;if(!edit(after,before)){notify(UiNotice::RecordLost);return;}if(event.flags==project_.active_pattern && event.track==selected_)refresh_steps();}
  if(event.type==EngineEventType::RecordedKitHit && event.track<16 && event.flags<kPatterns && event.parameter<16){auto track=track_views_.at(event.flags,event.track);if(track.instrument!=Instrument::DrumKit){notify(UiNotice::RecordLost);return;}auto& step=track.kit.steps[event.parameter][event.value];auto next=step;next.enabled=1;next.velocity=uint8_t(bounded(int(event.scalar),1,127));int micro=event.tick?int(event.tick)-193:0;next.microtiming=int8_t(bounded((micro+(micro>=0?2:-2))/4,-48,48));Command after{CommandType::SetKitStep,event.track,event.value,0},before=after;after.pattern=before.pattern=event.flags;after.parameter=before.parameter=event.parameter;after.kit_step=next;before.kit_step=step;if(!edit(after,before)){notify(UiNotice::RecordLost);return;}if(event.flags==project_.active_pattern && event.track==selected_)refresh_steps();}
  if(event.type==EngineEventType::RecordedNote && event.track<16 && event.flags<kPatterns && event.parameter<128){auto& step=track_views_.at(event.flags,event.track).steps[event.value];StepState next=step;next.chromatic=chromatic || next.chromatic;next.native_drum=native_drum || next.native_drum;if(!next.enabled){next.enabled=true;next.note=uint8_t(event.parameter);for(auto& note:next.chord)note=255;}else if(next.note!=event.parameter){bool found=false;for(uint8_t note:next.chord)found|=note==event.parameter;if(!found){unsigned slot=0;while(slot<kChordTones && next.chord[slot]!=255)++slot;if(slot==kChordTones){notify(UiNotice::ChordFull);return;}next.chord[slot]=uint8_t(event.parameter);}}next.velocity=uint8_t(bounded(int(event.scalar),1,127));if(event.tick)next.microtiming=int16_t(int(event.tick)-193);Command after{CommandType::SetRecordedStep,event.track,event.value,0},before=after;after.pattern=before.pattern=event.flags;after.recorded_step=next;before.recorded_step=step;if(!edit(after,before)){notify(UiNotice::RecordLost);return;}if(event.recording_id)remember_record(event.flags,event.track,event.value,event.recording_id);if(event.flags==project_.active_pattern && event.track==selected_)refresh_steps();}
  if(event.type==EngineEventType::RecordCapacity)notify(UiNotice::RecordLost);
  if(event.type==EngineEventType::RecordStopped)note_finish_pending_=false;
  if(event.type==EngineEventType::CountInState){counting_in_=event.value!=0;count_beat_=event.value;if(event.flags)playing_=false;}
  if(event.type==EngineEventType::RecordedGate && event.track<16 && event.flags<kPatterns && event.parameter>=1 && event.parameter<=384 && event.recording_id && matches_record(event.flags,event.track,event.value,event.recording_id)){auto& step=track_views_.at(event.flags,event.track).steps[event.value];if(step.enabled && step.gate!=event.parameter){Command after{CommandType::SetGate,event.track,event.value,event.parameter},before=after;after.pattern=before.pattern=event.flags;before.value=step.gate;if(!edit(after,before))notify(UiNotice::RecordLost);}forget_record(event.flags,event.track,event.value);}
  if(event.type==EngineEventType::GestureState){gesture_recording_=event.value==1;gesture_playing_=event.value==2;if(event.value==1){project_.gesture.count=0;project_.gesture.ticks=event.parameter;}if(event.value==3)project_.gesture.count=0;if(event.value==4)notify(UiNotice::GestureFull);}
  if(event.type==EngineEventType::GesturePoint){GesturePoint p{event.tick,event.parameter,event.scalar,event.track,event.value,GestureTarget(event.flags)};auto& g=project_.gesture;if(g.count && g.points[g.count-1].tick==p.tick && g.points[g.count-1].track==p.track && g.points[g.count-1].pattern==p.pattern && g.points[g.count-1].target==p.target && g.points[g.count-1].parameter==p.parameter)g.points[g.count-1]=p;else if(g.count<128)g.points[g.count++]=p;}
  if(event.type==EngineEventType::TempoChanged)project_.tempo=uint16_t(event.scalar);
  if(event.type==EngineEventType::Transport){if(!event.value)arrangement_ui_active_=false;playing_=event.value!=0;counting_in_=false;seek_ready_=false;song_position_=event.parameter;}
  if(event.type==EngineEventType::SongPosition){if(event.flags){seek_ready_=false;notify(UiNotice::Failed);}else{song_position_=event.parameter;seek_ready_=true;}}
  if(event.type==EngineEventType::Step){positions_[event.track]=event.value;if(follow_display_ && playing_ && mode_==UiMode::Step && event.track==selected_){bool touched=false;for(unsigned i=0;i<8;++i)touched=touched||knob_touched(i);unsigned pg=event.value/16;if(!touched && tracker_view_ && project_.clips[selected_].pattern==project_.active_pattern){selected_step_=uint8_t(event.value);dirty_=true;}if(!touched && pg!=page_ && pg<16){page_=uint8_t(pg);refresh_steps();dirty_=true;}}}
  if(event.type==EngineEventType::TwinRejected){twin_switch_pending_[event.track]=false;twin_end_ui("TWIN SWITCH REFUSED / SOUND KEPT");notify(UiNotice::TwinFailed);dirty_=true;}
  if(event.type==EngineEventType::ClipLaunchRejected){pending_[event.track]=-1;queued_stops_&=uint16_t(~(1u<<event.track));notify(UiNotice::Failed);}
  if(event.type==EngineEventType::SoundAttached && event.attached_sound){auto previous=track(event.track).instrument;PresetFxSnapshot strip;if(event.flags)strip.capture(track(event.track));mount_sound(track(event.track),*event.attached_sound);track(event.track).preset_slot=event.parameter;if(event.flags){strip.apply(track(event.track));project_.twins[event.track].active=event.value!=0;twin_switch_pending_[event.track]=false;copy_text(twin_message_,sizeof twin_message_,twin_status_name(event.value?TwinStatus::Active:TwinStatus::Ready));notify(UiNotice::TwinChanged);}else if(previous!=track(event.track).instrument)reset_track_morph(project_,event.track);++assignment_revision_[event.track];dirty_=true;}
  if(event.type==EngineEventType::ClipLaunched) {queued_stops_&=uint16_t(~(1u<<event.track)); project_.clips[event.track].pattern=event.value; project_.clips[event.track].playing=event.flags!=0; if(pending_[event.track]==event.value) pending_[event.track]=-1;if(following_clip_ && event.track==selected_){if(project_.active_pattern!=event.value)sn_ref_armed_=false;project_.active_pattern=event.value;refresh_steps();} }
  if(event.type==EngineEventType::ProgramChanged && event.track<16 && event.value<kPatterns && event.parameter<128)track_views_.at(event.value,event.track).midi_program=uint8_t(event.parameter);
  if(event.type==EngineEventType::EngineParameterChanged && event.value<kPatterns){set_engine_parameter(track_views_.at(event.value,event.track),event.parameter,event.scalar);dirty_=true;}
  if(event.type==EngineEventType::ParameterChanged && event.value<kPatterns){auto t=track_views_.at(event.value,event.track);{set_track_parameter(t,event.parameter,event.scalar);if(project_.morph_enabled && event.value==project_.clips[event.track].pattern && target_descriptor(event.parameter) && morph_index(event.parameter)<kCommonMorphParameters)project_.morph_a[event.track].values[morph_index(event.parameter)]=project_.morph_b[event.track].values[morph_index(event.parameter)]=event.scalar;}}
  if(event.type==EngineEventType::SceneLaunched) {if(queued_scene_==event.value)queued_scene_=-1; project_.active_scene=event.value;arrangement_ui_active_=event.flags!=0;arrangement_ui_mute_=event.parameter;arrangement_ui_solo_=0; }
  if(event.type==EngineEventType::Note && event.track<16){trigger_ms_[event.track]=now_ms_;trigger_seen_[event.track]=true;}
  if(event.type==EngineEventType::ClipLaunched){launch_ms_=now_ms_;}
  dirty_=true;
 }
auto Performance::midi_capture_path(const char* path) -> void{const char* name=path;for(const char* p=path;*p;++p)if(*p=='/')name=p+1;unsigned i=0;while(name[i] && i+1<sizeof midi_capture_path_){midi_capture_path_[i]=name[i];++i;}midi_capture_path_[i]=0;dirty_=true;}
auto Performance::freeze_format() -> void{copy_text(freeze_text_,sizeof freeze_text_,"Capturing 00% - CANCEL stops");unsigned v=freeze_progress_;if(v>99)v=99;freeze_text_[10]=char('0'+(v/10)%10);freeze_text_[11]=char('0'+v%10);}
auto Performance::set_device_settings(const DeviceSettings& next) -> bool{if(!valid_device(next) || engine_.command_space()<2)return false;if(next.local!=device_.local && !send({CommandType::ReleaseLocal,0,0,0}))return false;if(!send({CommandType::SetInputGain,0,next.tuner_source,uint16_t(int(next.input_db)+24)}))return false;device_=next;settings_.theme=device_.theme;++device_revision_;device_saved_=false;dirty_=true;return true;}
auto Performance::setting_raw_value(unsigned id)const -> float{const float values[]={float(settings_.motion),float(settings_.theme),float(settings_.pixel_order),float(settings_.strip),float(settings_.preview565),float(device_.screen),float(device_.led),float(device_.velocity),float(device_.input_db),float(device_.tuner_source),float(device_.local),float(multitimbral()),float(system_space_unit_)};return id<13?values[id]:0;}
auto Performance::setting_row_normalized(unsigned row)const -> float{unsigned id=setting_control(row);float lo=id>=5 && id<=10?kDeviceControls[id-5].minimum:0,hi=id>=5 && id<=10?kDeviceControls[id-5].maximum:id==1?float(ui::Theme::kCount-1):id==0 || id==3?2:1;return ui::clamp01((setting_raw_value(id)-lo)/(hi-lo));}
auto Performance::setting_control(unsigned index)const -> unsigned{static const unsigned ids[7][4]={{0,255,255,255},{5,1,2,4},{6,3,255,255},{7,255,255,255},{8,9,255,255},{10,11,255,255},{12,255,255,255}};return index<settings_count()?ids[settings_page_][index]:255;}
auto Performance::setting_name(unsigned index)const -> const char*{static const char* names[]={"MOTION","THEME","PIXEL ORDER","STRIP PROFILE","16-BIT VIEW","SCREEN LEVEL","LED LEVEL","VELOCITY","INPUT GAIN","TUNER INPUT","LOCAL CONTROL","MULTITIMBRAL","SPACE UNIT"};unsigned id=setting_control(index);return id<13?names[id]:"";}
auto Performance::setting_text(unsigned index)const -> const char*{switch(setting_control(index)){case 0:return settings_.motion==ui::Motion::Full?"Full":settings_.motion==ui::Motion::Reduced?"Reduced":"Off";case 1:return ui::Theme::name(settings_.theme);case 2:return settings_.pixel_order?"BGR565":"RGB565";case 3:return settings_.strip==ui::StripProfile::Uncalibrated?"Off":settings_.strip==ui::StripProfile::PaletteHypothesis?"Palette":"Mono";case 4:return settings_.preview565?"On":"Off";case 5:format_device_number(device_.screen," %");return device_text_;case 6:return device_.led==3?"High":device_.led==2?"Medium":"Low";case 7:return device_.velocity==0?"Linear":device_.velocity==1?"Soft":"Hard";case 8:format_device_number(device_.input_db," dB");return device_text_;case 9:return device_.tuner_source==0?"Input L":device_.tuner_source==1?"Input R":"Input L+R";case 10:return device_.local?"On":"Off";case 11:return multitimbral()?"On":"Off";case 12:return system_space_unit_?"MiB":"GiB";default:return "";}}
auto Performance::format_device_number(int n,const char* unit)const -> void{unsigned at=0;if(n<0){device_text_[at++]='-';n=-n;}if(n>=100)device_text_[at++]=char('0'+n/100);if(n>=10)device_text_[at++]=char('0'+n/10%10);device_text_[at++]=char('0'+n%10);while(*unit)device_text_[at++]=*unit++;device_text_[at]=0;}
auto Performance::service_help() -> void{bool was=help_shown_;if(!modal_active() && help_button_down()){if(!help_down_ || help_mode_!=mode_){help_down_=true;help_mode_=mode_;help_since_=now_ms_;}}else help_down_=false;help_shown_=help_visible();if(was!=help_shown_)dirty_=true;unsigned bit=1u<<unsigned(mode_);if(!(help_seen_&bit) && !modal_active() && notice()==UiNotice::None){help_seen_|=bit;notify(UiNotice::HintAvailable);}}
auto Performance::display_bar_phase()const -> float{float phase=visual_.bar_phase;if(playing_ && visual_.playing && visual_.rate)phase+=float(visual_elapsed_samples())*project_.tempo/(240.f*visual_.rate);return phase-floorf(phase);}
auto Performance::animate(uint64_t ms) -> void{bool overlay=knob_overlay();unsigned turning=knob_turning_mask();auto old_notice=notice();if(ms>now_ms_)now_ms_=ms;if(sn_ref_armed_ && !sn_ref_confirmation()){sn_ref_armed_=false;dirty_=true;}if(extend_pending_ && now_ms_-extend_time_>5000){extend_pending_=false;dirty_=true;}if(overlay!=knob_overlay() || turning!=knob_turning_mask() || old_notice!=notice())dirty_=true;if(modal_active()){page_left_pending_=page_right_pending_=page_left_down_=page_right_down_=false;step_down_=step_reentry_=false;}else{service_page_buttons();service_step_button();}service_track_shortcut();service_help();file_notice_.observe(browser_.status,now_ms_);if(mode_==UiMode::Settings && settings_page_==4 && visual_.sample!=tuner_sample_){tuner_=tune_input(visual_.tuner_samples,float(visual_.rate)/4);tuner_sample_=visual_.sample;}}
auto Performance::sync_ui_settings() -> void{project_.ui.theme=0;project_.ui.motion=uint8_t(settings_.motion);project_.ui.order=settings_.pixel_order;project_.ui.strip=uint8_t(settings_.strip);project_.ui.preview565=settings_.preview565;}
auto Performance::notify_recovery(RecoverySource source) -> void{if(source.kind==RecoveryKind::None || source.kind==RecoveryKind::Main){notify(UiNotice::Loaded);return;}copy_text(recovery_message_,sizeof recovery_message_,"Recovered .tmp");recovery_warning_=true;notify(UiNotice::Recovered);}
auto Performance::notice_text()const -> const char*{switch(notice()){case UiNotice::CompositionChanged:case UiNotice::CompositionFailed:return composition_message_;case UiNotice::TwinChanged:case UiNotice::TwinFailed:return twin_message_;case UiNotice::ModuleEntered:return "MODULE ON / SEQUENCER STOPPED / MIDI IN PLAYS TRACKS";case UiNotice::ModuleExited:return "MODULE OFF / PLAY TO RESUME";case UiNotice::ModuleTransport:return "SETTINGS > MIDI > MULTITIMBRAL: OFF / THEN PLAY";case UiNotice::LayerRefused:return layer_reason(notice_layer_reason_);case UiNotice::ControllerReconnect:return "CONTROLLER RECONNECT / PLAYBACK STOPPED";case UiNotice::PartialApplied:return "PARTIALLY APPLIED / UNDO TO REVERT";case UiNotice::HintAvailable:return "HOLD PAGE / ENTRY BUTTON FOR HELP";case UiNotice::RestartQueued:return "RESTART AT NEXT BAR";case UiNotice::MemoryFull:return "Not enough memory / sound kept";case UiNotice::AssetReload:return "SF2 not resident / reload preset";case UiNotice::SavedLocationFailed:return "Saved / startup project preference failed";case UiNotice::StripUnavailable:return "Strip mode unavailable / use PERFORM";case UiNotice::SnRefConfirm:return "Confirm REF / SN voices will stop";case UiNotice::SnRefQueued:return "REF command queued / no readback";case UiNotice::Recovered:return recovery_message_;case UiNotice::ProjectUnsynced:return "State lost / reload before saving";case UiNotice::ProjectPending:return "Changes pending / retry save";case UiNotice::SaveQueued:return "Changes pending / save queued";case UiNotice::Saving:return "Saving snapshot";case UiNotice::SnapshotSaved:return "Snapshot saved";case UiNotice::LoopPrecision:return "WAV loop boundary cannot be represented";case UiNotice::LoopUnsupported:return "No forward loop / clear slices and reverse";case UiNotice::LoopImported:return "WAV loop applied / Undo available";case UiNotice::ScaleRequired:return "Scale needs at least one note";case UiNotice::GrooveClipped:return "Groove limited / reduce timing or velocity";case UiNotice::ChordLiveMono:return "Mono instrument / lowest note only";case UiNotice::ChordRepeat:return "Disable note repeat for chords";case UiNotice::RecordLost:return "Recording incomplete / check edits";case UiNotice::ChordFull:return "Step chord full / maximum eight notes";case UiNotice::ChordVoiceLimit:return "Chord stored / engine voice limit";case UiNotice::ChordMono:return "Chord stored / selected engine is mono";case UiNotice::OverdubDone:return "Overdub ready / Undo restores source";case UiNotice::OverdubFailed:return "Overdub failed / source retained";case UiNotice::LoopConfirm:return "Shift + LOOP TAKE again / replaces track steps";case UiNotice::LoopReady:return "Loop prepared / Undo restores steps";case UiNotice::LoopInvalid:return "Select a 1-4 bar sample at current tempo";case UiNotice::GestureClear:return "Shift + CLEAR to erase gesture";case UiNotice::GestureFull:return "Gesture full / recording stopped";case UiNotice::Saved:return "Project saved";case UiNotice::SampleRecorded:return "Input take assigned / existing steps play it";case UiNotice::MidiSaved:return "MIDI file saved";case UiNotice::MidiImported:return "MIDI events imported";case UiNotice::SaveFailed:return "Save failed / check storage";case UiNotice::Loaded:return playing_?"Project loaded / playing":"Project loaded / press Play";case UiNotice::KitLoaded:return playing_?"Kit loaded / playing":"Kit loaded / ready";case UiNotice::NewProject:return "Empty project ready";case UiNotice::Failed:return "Operation failed / state retained";case UiNotice::Copied:return "Pattern copied";case UiNotice::StepCopied:return "Step copied";case UiNotice::StepPasted:return "Step pasted / Undo available";case UiNotice::StepRhythmPasted:return "Rhythm pasted / other settings kept";case UiNotice::ChopPhrase:return "Chop phrase ready / Undo available";case UiNotice::KitPadSwapped:return "Sounds swapped / step lanes unchanged";case UiNotice::ExtendConfirm:return "Press DOUBLE TRACK again / replaces later steps";case UiNotice::Extended:return "Track doubled / Undo available";case UiNotice::SceneStored:return "Scene stored / pads launch scenes";case UiNotice::SceneWait:return "Wait for queued launch before storing";case UiNotice::LocksFull:return "Four locks used / clear a lock";case UiNotice::FilesBusy:return "File transfer busy / finish the current file job";case UiNotice::HostFileTools:return "File transfer: use instrument Wi-Fi";case UiNotice::HostUpdateTools:return "Updates: use host update tools";default:return "";}}
auto Performance::notice_error()const -> bool{return notice()==UiNotice::CompositionFailed || notice()==UiNotice::TwinFailed || notice()==UiNotice::LayerRefused || notice()==UiNotice::ModuleTransport || notice()==UiNotice::PartialApplied || notice()==UiNotice::MemoryFull || notice()==UiNotice::AssetReload || notice()==UiNotice::SnRefConfirm || notice()==UiNotice::ProjectUnsynced || notice()==UiNotice::ProjectPending || notice()==UiNotice::SaveFailed || notice()==UiNotice::Failed || notice()==UiNotice::LocksFull || notice()==UiNotice::LoopInvalid || notice()==UiNotice::OverdubFailed || notice()==UiNotice::SceneWait || notice()==UiNotice::ChordFull || notice()==UiNotice::RecordLost || notice()==UiNotice::LoopUnsupported || notice()==UiNotice::LoopPrecision;}
auto Performance::warning()const -> bool{return recovery_warning_ || warning_ || notice_error() || dropped_ || engine_.event_dropped() || engine_.repeat_dropped() || visual_.invalid_audio || visual_.limiter_main>0.01f || visual_.limiter_cue>0.01f;}
auto Performance::visual_source()const -> unsigned{return mode_==UiMode::Browser?2:mode_==UiMode::Routing && routing_fx_page_?53+group_cursor_:audio_page()?(live_recording_?audio_lane().source:255):mode_==UiMode::Sampler?recording_source():255;}
auto Performance::analysis()const -> const VisualAnalysisFrame*{return analysis_valid_ && analysis_.generation==generation_ && analysis_.track==selected_ && analysis_.source==visual_source() && now_ms_-analysis_received_ms_<=250?&analysis_:nullptr;}
auto Performance::service_visual_analysis() -> void{
  engine_.visual_focus(selected_,visual_source());auto* slot=visual_analysis_worker_;if(!slot)return;
  if(visual_analysis_inline_)slot->process_one();
  VisualAnalysisFrame result;if(slot->collect(result) && result.generation==generation_ && result.track==selected_ && result.source==visual_source()){analysis_=result;analysis_received_ms_=now_ms_;analysis_valid_=true;dirty_=true;}
  bool needed=(mode_==UiMode::Synth && !source_select_) || mode_==UiMode::Sampler || mode_==UiMode::Browser || mode_==UiMode::Effects || (mode_==UiMode::Routing && routing_fx_page_) || audio_page();if(!needed)return;
  if(now_ms_<analysis_due_ms_ || !visual_.taps.sample || visual_.taps.sample==analysis_requested_sample_ || visual_.taps.generation!=generation_ || visual_.taps.track!=selected_ || visual_.taps.source!=visual_source())return;
  unsigned interval=analysis_backoff_<2?50:analysis_backoff_<4?100:200;analysis_due_ms_=now_ms_+interval;
  if(slot->submit(visual_.taps)){analysis_requested_sample_=visual_.taps.sample;analysis_backoff_=0;}else if(analysis_backoff_<4)++analysis_backoff_;
 }
auto Performance::layer_warning(bool runtime)const -> LayerReason{
 const auto t=track_const(selected_);if(t.instrument!=Instrument::Layer)return LayerReason::Ready;
 auto reason=layer_status(t.layer);if(reason!=LayerReason::Ready)return reason;
 // Declared overlap is available immediately on mount, before a note fires.
 for(unsigned k=0;k<t.layer.count;++k)for(unsigned v=0;v<t.layer.count;++v){
  unsigned slots[unsigned(VoiceResource::Count)]{},cost[unsigned(VoiceResource::Count)]{};uint8_t selected=0;
  layer_note_cost(t.layer,t.layer.parts[k].key_low,t.layer.parts[v].velocity_low,&selected);
  for(unsigned i=0;i<t.layer.count;++i)if(selected&(1u<<i)){
   const auto& child=t.layer.parts[i].sound;auto resource=engine_voice_resource(unsigned(child.instrument));
   if(resource!=VoiceResource::None){unsigned r=unsigned(resource);++slots[r];cost[r]+=engine_voice_cost(child.instrument,child.model);const auto* d=engine_descriptor(unsigned(child.instrument));if(d && (slots[r]>d->polyphony.shared_slots || cost[r]>d->polyphony.shared_budget))return LayerReason::Voices;}
   if((engine_asset_mode(child.instrument)==EngineAssetMode::Sample || engine_asset_mode(child.instrument)==EngineAssetMode::SoundFont) && (child.sample_slot>=kSampleAssets || !project_.assets[child.sample_slot].path[0] || project_.assets[child.sample_slot].metadata.missing))return LayerReason::Assets;
  }
 }
 if(runtime && visual_.attached_instrument[selected_]==unsigned(Instrument::Layer) && visual_.layer_failure[selected_]>0 && visual_.layer_failure[selected_]<unsigned(LayerReason::Assets)+1)return LayerReason(visual_.layer_failure[selected_]);
 return LayerReason::Ready;
}
auto Performance::poll_visual() -> void{AudioVisualFrame frame;while(engine_.pop_visual(frame))if(frame.generation==generation_){if(frame.performance_fx_pending!=visual_.performance_fx_pending || frame.performance_fx_mode!=visual_.performance_fx_mode)dirty_=true;if((frame.attached_instrument[selected_]==unsigned(Instrument::Texture) || frame.attached_instrument[selected_]==unsigned(Instrument::Resonator)) && frame.layer_failure[selected_]==unsigned(LayerReason::Cost) && frame.layer_failure[selected_]!=visual_.layer_failure[selected_]){copy_text(browser_.status,sizeof browser_.status,"DSP budget / request refused");notify(UiNotice::Failed);}if(frame.attached_instrument[selected_]==unsigned(Instrument::SoundFont) && frame.layer_failure[selected_]==unsigned(LayerReason::Assets) && frame.layer_failure[selected_]!=visual_.layer_failure[selected_])notify(UiNotice::AssetReload);if(frame.multitimbral!=visual_.multitimbral)dirty_=true;if(frame.attached_instrument[selected_]==unsigned(Instrument::Layer) && frame.layer_failure[selected_]!=visual_.layer_failure[selected_]){dirty_=true;if(frame.layer_failure[selected_]>0 && frame.layer_failure[selected_]<unsigned(LayerReason::Assets)+1){notice_layer_reason_=LayerReason(frame.layer_failure[selected_]);notify(UiNotice::LayerRefused);}}visual_=frame;visual_received_ms_=now_ms_;}}
auto Performance::poll()  -> void{for(unsigned i=0;i<8;++i)engine_.visual_control(i,selected_,knob_parameter(i));bool recording_pending=note_recording_ || note_finish_pending_ || has_record_ids();retry_releases(); EngineEvent event; while(engine_.pop_event(event))accept_event(event);uint32_t dropped=engine_.event_dropped();if((recording_pending || note_recording_ || note_finish_pending_ || has_record_ids()) && dropped!=event_dropped_seen_){note_finish_pending_=false;notify(UiNotice::RecordLost);}event_dropped_seen_=dropped;service_scene_take();poll_visual();service_visual_analysis(); }
auto Performance::effects_controls()const -> EffectsContext{return {shifted(),fx_page_,track_const(selected_).instrument==Instrument::DrumKit,track_const(selected_).send_prefader,chain_node_,track_const(selected_).insert_disabled,history_cursor_>0,history_cursor_<history_size_};}
auto Performance::perform_controls()const -> PerformContext{return {shifted(),performance_page_,performance_fx_quantize_,performance_fx_tail_,performance_fx_bank_!=0,track_const(selected_).mutation.frozen,gesture_recording_,gesture_playing_,control_all_,history_cursor_>0,history_cursor_<history_size_};}
auto Performance::layer_use(bool remove) -> bool{if(track_const(selected_).instrument!=Instrument::Layer)return false;Command after{CommandType::ApplySound,selected_,0,0},before=after;snapshot_sound(before.sound(),track_const(selected_));after.sound()=before.sound();auto& layer=after.sound().layer;unsigned at=layer_cursor_<layer.count?layer_cursor_:layer.count;
  if(remove){if(at>=layer.count)return false;for(unsigned i=at+1;i<layer.count;++i)layer.parts[i-1]=layer.parts[i];if(layer.count)--layer.count;}
  else{const auto& source=project_.sounds[layer_pool_cursor_];if(at>=kLayerParts || !layer_leaf_supported(source.instrument) || !engine_.preset_assets_ready(source)){copy_text(browser_.status,sizeof browser_.status,layer_leaf_exclusion(source.instrument)?layer_leaf_exclusion(source.instrument):"Child assets are not ready");notify(UiNotice::Failed);return false;}auto& part=layer.parts[at];part.sound=static_cast<const SoundLeaf&>(source);part.source=layer_pool_cursor_;part.fx=source.fx;part.raw_size=0;if(at==layer.count)++layer.count;unsigned cost=0;if(layer_status(layer,&cost)!=LayerReason::Ready){copy_text(browser_.status,sizeof browser_.status,layer_reason(layer_status(layer)));notify(UiNotice::Failed);return false;}}
  bool ok=edit(after,before);if(ok){instrument_view_=0;layer_cursor_=uint8_t(at<layer.count?at:layer.count<4?layer.count:3);}return ok;
 }
auto Performance::advanced(AdvancedGroup group,unsigned key,int value,unsigned cell) -> bool{
  auto& t=track(selected_);int old=advanced_value(t,group,key,cell);
  if(!set_advanced(t,group,key,value,cell))return false;
  set_advanced(t,group,key,old,cell);
  Command after{CommandType::SetAdvanced,selected_,uint8_t(cell),uint16_t(key)},before=after;after.parameter=before.parameter=uint16_t(group);after.scalar=float(value);before.scalar=float(old);return edit(after,before);
 }
auto Performance::follow(FollowMode mode,unsigned probability) -> bool{if(unsigned(mode)>2 || probability>100)return false;Command after{CommandType::SetFollow,0,uint8_t(probability),uint16_t(mode)},before{CommandType::SetFollow,0,project_.chain.probability,uint16_t(project_.chain.mode)};return edit(after,before);}
auto Performance::lock_target(unsigned step,uint16_t id)const -> ParameterTarget{if(id>=300 && id<=302)return id;const auto& t=track_const(selected_);Instrument engine=t.instrument;if(step<kSteps && t.steps[step].sound<kPresetSlots)engine=project_.sounds[t.steps[step].sound].instrument;return bind_parameter_target(engine,id);}
auto Performance::set_lock(unsigned step,uint16_t id,float value,unsigned slot)  -> bool{
  id=lock_target(step,id);value=snap_parameter(id,value);
  if(slot==kLocks){for(unsigned i=0;i<kLocks;++i)if(track_const(selected_).steps[step<kSteps?step:0].locks[i].enabled && track_const(selected_).steps[step<kSteps?step:0].locks[i].id==id){slot=i;break;}if(slot==kLocks)for(unsigned i=0;i<kLocks;++i)if(!track_const(selected_).steps[step<kSteps?step:0].locks[i].enabled){slot=i;break;}if(slot==kLocks){notify(UiNotice::LocksFull);return false;}}
  if(step>=kSteps || slot>=kLocks || !(valid_target(id,value) || valid_step_sample_lock(id,value) || (id>=128 && id<256 && value>=0 && value<=127))) return false;
  const auto& existing=track_const(selected_).steps[step];
  if(target_descriptor(id) && track_const(selected_).instrument!=Instrument::DrumKit && !existing.enabled && !existing.lock_only){Command after{CommandType::SetRecordedStep,selected_,uint8_t(step),0},before=after;before.recorded_step=existing;after.recorded_step=existing;after.recorded_step.lock_only=true;after.recorded_step.locks[slot]={id,value,true};return edit(after,before);}
  Command after{CommandType::SetStepLock,selected_,uint8_t(step),uint16_t(slot)}; after.parameter=id; after.scalar=value;
  const auto& lock=track(selected_).steps[step].locks[slot]; Command before=after;
  if(lock.enabled) { before.parameter=lock.id; before.scalar=lock.value; } else before.type=CommandType::ClearStepLock;
  return edit(after,before);
 }
auto Performance::set_performance_fx(PerformanceFx mode,float amount,bool latch) -> bool{Command c{CommandType::SetPerformanceFx,0,uint8_t(performance_fx_tail_?0:1),uint16_t(mode)};c.scalar=ui::clamp01(amount);c.parameter=performance_fx_quantize_;if(!send(c))return false;performance_fx_mode_=mode;performance_fx_amount_=c.scalar;performance_fx_latched_=mode!=PerformanceFx::Off && latch;release_performance_fx_=false;dirty_=true;return true;}
auto Performance::release_strip() -> bool{
  if(!strip_active_)return true;
  Command c{strip_note_active_?CommandType::MidiInput:CommandType::StripExpression,strip_track_,strip_note_,0};
  c.pattern=strip_pattern_;if(strip_note_active_){c.value=0x80;c.parameter=0x800;}
  if(!send(c)){strip_release_pending_=true;return false;}
  strip_active_=strip_note_active_=strip_release_pending_=false;dirty_=true;return true;
 }
auto Performance::strip_input(int position,int maximum) -> bool{
  if(transfer_view_.active)return false;
  if(position<=0){strip_interaction_until_ms_=now_ms_;return release_strip();}
  if(modal_active() || position>maximum || maximum<2)return false;
  if(strip_release_pending_ && !release_strip())return false;
  if(axes_.strip_mode==StripMode::Perform){if(mode_!=UiMode::Perform)return true;if(performance_fx_mode_!=PerformanceFx::Off){if(!set_performance_fx(performance_fx_mode_,float(position)/maximum,performance_fx_latched_))return false;}else if(!morph(float(position)/maximum))return false;}
  else {
   unsigned target=strip_active_?strip_track_:selected_;unsigned pattern=strip_active_?strip_pattern_:project_.active_pattern;
   int scaled=1+int(int64_t(position-1)*16382/(maximum-1));
   if(axes_.strip_mode==StripMode::Notes){
    unsigned note=unsigned(bounded(int(track_const(target).root_note)+int(int64_t(position-1)*24/(maximum-1)),0,127));
    if(strip_note_active_ && strip_note_==note)return true;
    if(strip_active_ && !release_strip())return false;
    Command c{CommandType::MidiInput,uint8_t(target),uint8_t(note),0x90};c.parameter=0x800;c.scalar=100;c.pattern=uint8_t(pattern);if(!send(c))return false;
    strip_note_=uint8_t(note);strip_note_active_=true;
   }else{Command c{CommandType::StripExpression,uint8_t(target),uint8_t(axes_.strip_mode==StripMode::Pitch?1:2),uint16_t(scaled)};c.pattern=uint8_t(pattern);if(!send(c))return false;}
   strip_track_=uint8_t(target);strip_pattern_=uint8_t(pattern);strip_active_=true;
  }
  strip_interaction_until_ms_=now_ms_+250;dirty_=true;return true;
 }
auto Performance::release_all() -> void{step_down_=false;page_left_pending_=page_right_pending_=page_left_down_=page_right_down_=false;clear_pad_feedback();release_strip();release_performance_fx_=true;held_performance_pad_=255;repeat_release_pending_=repeat_release_pending_ || repeat_mode_!=0;repeat_mode_=0;release_stutter_=release_fill_=true;release_control_all_=release_control_all_ || control_all_;control_all_=false;control_all_position_=0.5f;held_stutter_pad_=held_fill_pad_=255;for(auto& c:controls_)c=0;for(unsigned i=0;i<16;++i){if(notes_held_[i])pending_release_[i]=true;held_[i]=false;pressures_[i]=0;}retry_releases();dirty_=true;}
auto Performance::control(unsigned byte,uint8_t value)  -> bool{NavigationScope navigation_scope(*this);struct HelpScope{Performance& ui;~HelpScope(){ui.service_help();}} help_scope{*this};
  if(byte<1 || byte>10) return false;
  uint8_t old=controls_[byte-1]; controls_[byte-1]=value; uint8_t rising=value & uint8_t(~old);
  if(transfer_view_.active){if(byte==1 && (rising&0x80))return soft_key(7);if(byte==9)for(unsigned i=0;i<7;++i)if(rising&(1u<<i))return soft_key(i);return true;}
  if(old!=value){dirty_=true;if(!help_down_ && help_button_down()){help_down_=true;help_mode_=mode_;help_since_=now_ms_;}}
  if(byte==10 && old!=value)for(unsigned i=0;i<8;++i)if((old^value)&(0x80>>i))knob_overlay_until_[i]=now_ms_+1200;
  if(modal_active() && byte==1 && (rising&0x20))return modal_cancel();
  if(!browser_editing() && modal_active()){
   if(byte==1 && (rising&1))return modal_accept();
   if(byte==9){for(unsigned i=0;i<7;++i)if(rising&(1u<<i))return modal_key(i);return true;}
   if(byte==6 && (rising&0x80))return shifted()?panic():stop();
   if(byte==3 && !(value&0x10) && (old&0x10))return note_repeat(0,repeat_rate_);
   return true;
  }
  if(browser_editing()){if(byte==7 && ((old^value)&0x04))page_button_edge(true,(value&0x04)!=0);if(byte==8 && ((old^value)&0x08))page_button_edge(false,(value&0x08)!=0);if(byte==9){for(unsigned i=0;i<7;++i)if(rising&(1u<<i))return browser_edit_key(i);return true;}if(byte==1 && (rising&0x80))return browser_edit_key(7);if(byte==1)return encoder_direction(rising);if(byte==6 && (rising&0x80))return (controls_[0]&0x40)?panic():stop();if(byte==6 && (rising&0x20))return play();return true;}
  if(byte==7 && ((old^value)&0x04)) page_button_edge(true,(value&0x04)!=0);
  if(byte==8 && ((old^value)&0x08)) page_button_edge(false,(value&0x08)!=0);
  if(byte==9){bool ok=true;for(unsigned i=0;i<7;++i)if(rising&(1u<<i))ok=soft_key(i)&&ok;return ok;}
  if(byte==1 && (rising&0x80))return soft_key(7);
  if(byte==2){bool ok=true;for(unsigned i=0;i<8;++i)if((old^value)&(1u<<i))ok=track_shortcut(i,(value&(1u<<i))!=0)&&ok;return ok;}
  if(byte==7 && (rising&0x02) && !(controls_[0]&0x40)){set_mode(UiMode::Settings);return true;}
  if(byte==8 && (rising&0x04)){set_mode(UiMode::Browser);return request_file(FileAction::Refresh);}
  if(byte==8 && (rising&0x20)){set_mode(UiMode::Modulation);return true;}
  if(byte==8 && (rising&0x10)){if(shifted())request_save();else set_mode(UiMode::Project);return true;}
  if(byte==7 && (rising&0x02) && (controls_[0]&0x40)){if(playing_){notify(UiNotice::Failed);return false;}set_mode(UiMode::Settings);update_confirm_=true;dirty_=true;return true;}
  // Mapping follows encdr's pinned JSON descriptor; its prose table differs.
  // Physical mappings remain provisional until captured on the actual MK3.
  if(byte==4 && (rising&0x10)){fixed_velocity_=!fixed_velocity_;return true;}
  if(byte==5 && (rising&0x04))return shifted()?double_pattern():(mode_==UiMode::Step?duplicate_step():duplicate_pattern());
  if(byte==4 && ((old^value)&0x08))step_button_edge((value&0x08)!=0);
  if(byte==4) { if(rising&0x01)set_mode(UiMode::Pad); if(rising&0x02)set_mode(UiMode::Keyboard); if(rising&0x04)set_mode(UiMode::Chord); if(rising&0x08){note_edit_=false;set_mode(UiMode::Step);} if(rising&0x20)set_mode(UiMode::Scene); if(rising&0x40)set_mode(UiMode::Clip); if(rising&0x80){if(pending_project_==ProjectAction::None && !update_confirm_ && !freeze_modal_ && !stretch_modal_){if(shifted())redo();else undo();}} }
  if(byte==5 && (rising&0x02))set_mode(UiMode::Generate);
  if(byte==6 && (rising&0x40)){if(mode_==UiMode::Sampler)return request_file(shifted()?FileAction::SampleRecordToggle:FileAction::RecordToggle);if(shifted())return start_count_in_record();return arm_note_record(!note_recording_);}
  if(byte==6 && (rising&0x02))return restart_transport(shifted());
  if(byte==6 && (rising&0x10)){follow_display_=!follow_display_;dirty_=true;return true;}
  if(byte==6 && (rising&0x08)){if(shifted())return set_click_setting(1);if(last_tap_ms_ && now_ms_-last_tap_ms_>=200 && now_ms_-last_tap_ms_<=3000){Command c{CommandType::SetTempo,0,0,uint16_t(60000/(now_ms_-last_tap_ms_))};if(!send(c))return false;project_.tempo=c.value;}last_tap_ms_=now_ms_;return true;}
  if(byte==5 && (rising&0x40)){set_strip_mode(StripMode::Pitch);dirty_=true;return true;}
  if(byte==5 && (rising&0x80)){set_strip_mode(StripMode::Mod);dirty_=true;return true;}
  if(byte==3 && (rising&0x01)){set_strip_mode(StripMode::Notes);dirty_=true;return true;}if(byte==3 && (rising&0x20)){return lock_press();}
  if(byte==3)main_encoder_mode_=(value&0x02)?1:(value&0x04)?2:(value&0x08)?3:0;
  if(byte==6 && (rising&0x04)){bool touched=false;for(unsigned i=0;i<8;++i)touched|=knob_touched(i);if(touched)return reset_touched_knobs();}
  if(byte==6) { if((rising&0x04) && shifted())return mode_==UiMode::Scene?clear_chain():clear_step_locks(); if(rising&0x20)return play(); if(rising&0x80)return (controls_[0]&0x40)?panic():stop(); if(rising&0x01){if(controls_[0]&0x40)return set_control_all(!control_all_);set_mode(UiMode::Perform);performance_page_=0;set_strip_mode(StripMode::Perform);} }
  if(byte==7) { if(rising&0x08)set_mode(UiMode::Sampler); if(rising&0x10)set_mode(UiMode::Mixer); if(rising&0x01){set_mode(UiMode::Perform);performance_page_=1;} if(rising&0x20) { if(controls_[0]&0x40)return cycle_instrument(); if(mode_==UiMode::Synth && !wave_profile())return toggle_source_select(); set_mode(UiMode::Synth); } }
  if(byte==8) { if(rising&0x01){if(mode_==UiMode::Routing)set_mode(UiMode::Effects);else if(mode_==UiMode::Effects)set_mode(UiMode::Routing);else {routing_fx_page_=false;group_send_page_=false;set_mode(UiMode::Routing);}} if(rising&0x02)set_mode(UiMode::Song); }
  if(byte==3 && ((old^value)&0x10)){bool pressed=value&0x10;if(pressed && repeat_release_pending_)return false;if(pressed && repeat_latch_ && repeat_mode_){bool ok=note_repeat(0,repeat_rate_);if(ok)repeat_ignore_release_=true;return ok;}if(!pressed && repeat_ignore_release_){repeat_ignore_release_=false;return true;}if(!pressed && repeat_latch_)return true;bool ok=note_repeat(pressed?(shifted()?2:1):0,repeat_rate_);if(!ok && !pressed)repeat_release_pending_=true;return ok;}
  if(byte==1)return encoder_direction(rising);
  return true;
 }
auto Performance::set_instrument(Instrument instrument)  -> bool{ if(!engine_.supports_engine(instrument))return false; Command after{CommandType::SetInstrument,selected_,0,uint16_t(instrument)},before=after;before.value=uint16_t(track(selected_).instrument);if(instrument==Instrument::TapeMicrosound && track_const(selected_).engine_parameters[0]==0.5f){if(engine_.command_space()<2)return false;EditGroup edit_group(*this);bool ok=edit(after,before);if(ok)ok=set_parameter(32,0.75f);return edit_group.finish(ok);}return edit(after,before); }
auto Performance::assign_overdub(unsigned slot,WavView view,const SampleMetadata& metadata,WavView prior,unsigned target,unsigned pattern) -> bool{if(slot>=kSampleAssets || target>=16 || pattern!=project_.active_pattern)return false;const auto t=track_views_.at(pattern,target);if(t.instrument!=Instrument::Sampler || (!prior.data && !prior.reader))return false;Command after{CommandType::SetSample,uint8_t(target),0,uint16_t(slot)},before{CommandType::SetSample,uint8_t(target),0,t.sample_slot};after.pattern=before.pattern=uint8_t(pattern);after.sample=view;after.parameter=1;after.metadata=metadata;before.sample=prior;before.parameter=1;before.metadata=project_.assets[t.sample_slot].metadata;return edit(after,before);}
auto Performance::source_family_index_of(unsigned engine) -> int{
  static const char* order[]={"DRUM","SYNTH","SAMPLER","MACRO","FM","CREATIVE","MIDI","KIT","PHYSICAL"};
  const auto* d=instrument_descriptor(engine);if(!d)return 0;
  for(unsigned i=0;i<9;++i)if(source_family_is(order[i],engine_family_name(Instrument(engine))))return int(i);
  return 0;
 }
auto Performance::source_confirm() -> bool{
  // Replacing an Unknown engine discards its preserved payload, so the
  // first confirm only arms the discard (SPEC 2.9 fallback rule 3).
  if(track_const(selected_).instrument==Instrument::Unknown && !source_arm_unknown_){source_arm_unknown_=true;dirty_=true;return true;}
  if(source_cursor_>=instrument_count())return false;
  Instrument target=Instrument(source_cursor_);
  if(target!=track_const(selected_).instrument && !set_instrument(target))return false;
  source_select_=false;source_arm_unknown_=false;browser_.library_list=false;browser_refresh_pending_=false;instrument_view_=0;dirty_=true;return true;
 }
auto Performance::cycle_instrument()  -> bool{
  unsigned next=(unsigned(track(selected_).instrument)+1)%instrument_count();
  for(unsigned n=0;n<instrument_count();++n,next=(next+1)%instrument_count())if(engine_.supports_engine(Instrument(next)))return set_instrument(Instrument(next));
  return false;
 }
auto Performance::pad(uint8_t hardware,uint8_t tag,uint16_t pressure)  -> bool{
  if(transfer_view_.active)return false;
  const bool was_held=hardware<16 && held_[hardware];
  bool ok=pad_input(hardware,tag,pressure);
  if(hardware<16){if(ok && !was_held && tag==0x10){pad_press_context_[hardware]=notes_held_[hardware]?pad_feedback_context():0;pad_press_ms_[hardware]=now_ms_;}}
  return ok;
 }
auto Performance::pad_input(uint8_t hardware,uint8_t tag,uint16_t pressure)  -> bool{
  if(hardware>=16 || pressure>4095) return false;
  if(modal_active() && tag!=0x20 && tag!=0x30)return true;
  bool release=tag==0x20 || tag==0x30;
  raw_pressures_[hardware]=pressure;
  bool duplicate=tag==0x10 && held_[hardware];
  if(release) { pressures_[hardware]=0; held_[hardware]=false; }
  else if(tag==0x10) { pressures_[hardware]=pressure; held_[hardware]=true; }
  else if((tag==0 || tag==0x40) && held_[hardware])pressures_[hardware]=pressure;
  if(duplicate)return true;
  if(release && hardware==held_step_hardware_)held_step_hardware_=255;
  dirty_=true; unsigned p=(3-hardware/4)*4+hardware%4;
  if(release && hardware==held_stutter_pad_){held_stutter_pad_=255;bool ok=stutter(false);if(!ok)release_stutter_=true;return ok;}
  if(release && hardware==held_fill_pad_){held_fill_pad_=255;bool ok=set_fill(false);if(!ok)release_fill_=true;return ok;}
  if(release && hardware==held_performance_pad_){held_performance_pad_=255;if(!performance_fx_latched_){release_performance_fx_=true;retry_releases();}return true;}
  if(audio_page() && tag==0x10 && !(controls_[4]&0x30))return select_track(p);
  if(mode_==UiMode::Song && tag==0x10){if(!project_.song.length || song_cursor_>=project_.song.length)return false;auto sec=project_.song.sections[song_cursor_];if(sec.ticks && shifted()){sec.playing_mask^=uint16_t(1u<<p);sec.restart_mask|=uint16_t(1u<<p);}else sec.mute_mask^=uint16_t(1u<<p);return song_section_send(song_cursor_,sec);}   if(mode_==UiMode::Perform && tag==0x10 && !(controls_[4]&0x30)){if(performance_page_==4 && p>=2 && p<=(performance_fx_bank_?3u:12u)){PerformanceFx effect=PerformanceFx(performance_fx_bank_?p+10:p-1);if(shifted() && performance_fx_mode_==effect && performance_fx_latched_){release_performance_fx_=true;retry_releases();return true;}if(!set_performance_fx(effect,performance_fx_amount_,shifted()))return false;held_performance_pad_=shifted()?255:hardware;return true;}if(p>=2 && (p<=11 || (performance_page_==4 && p==12)))return true;if(p>=12)return morph(performance_page_==4?float(p-13)/2:float(p-12)/3);if(p==0){if(release_stutter_ || !stutter(true))return false;held_stutter_pad_=hardware;return true;}if(p==1){if(release_fill_ || !set_fill(true))return false;held_fill_pad_=hardware;return true;}}
  if(release && notes_held_[hardware]){Command c{CommandType::NoteOff,held_tracks_[hardware],hardware,held_notes_[hardware]};c.parameter=held_kind_[hardware];bool ok=send(c);if(ok)notes_held_[hardware]=false;else pending_release_[hardware]=true;return ok;}
  if((tag==0x40 || tag==0) && held_[hardware] && notes_held_[hardware]){Command c{CommandType::MidiInput,held_tracks_[hardware],held_notes_[hardware],0xa0};c.scalar=float(pressure)*127/4095;c.parameter=held_kind_[hardware]==3?uint16_t(0x300|hardware):0x100;return send(c);}
  if(tag!=0x10) return true;
  if(controls_[4]&0x30){if(!audio_page() && audio_lane().kind==TrackKind::Instrument && track_const(selected_).instrument==Instrument::DrumKit){select_kit_pad(p);if(controls_[4]&0x10){Command after{CommandType::SetKitSolo,selected_,0,uint16_t(track_const(selected_).kit.solo_mask^(1u<<p))},before=after;before.value=track_const(selected_).kit.solo_mask;return edit(after,before);}auto pad=track_const(selected_).kit.pads[p];pad.muted=!pad.muted;return set_kit_pad(p,pad);}bool solo=(controls_[4]&0x10)!=0;return mute_or_solo(p,solo,!solo && shifted());}
  if(mode_==UiMode::Step && select_held() && track_const(selected_).instrument==Instrument::DrumKit)return select_kit_pad(p);
  if(mode_==UiMode::Sampler && select_held() && track_const(selected_).instrument==Instrument::DrumKit){if(shifted() && sampler_page_==2)return swap_kit_pad_sounds(selected_kit_pad_,p);return select_kit_pad(p);}
  switch(mode_) {
   case UiMode::Project:asset_cursor_=uint8_t((asset_cursor_/16)*16+p);return true;
   case UiMode::Settings:if(p>=7)return false;settings_page_=uint8_t(p);settings_cursor_=0;dirty_=true;return true;
   case UiMode::Chord: {int note=keyboard_note(p);if(note<0)return false;bool ok=trigger_held_pad(hardware,selected_,note,pressure,3);if(ok && !live_poly_capable(track(selected_).instrument))notify(UiNotice::ChordLiveMono);return ok;}
   case UiMode::Keyboard: {int note=keyboard_note(p);if(note<0)return false;if(step_chord_editor())return toggle_step_chord(unsigned(60+note-int(track(selected_).root_note)));return trigger_held_pad(hardware,selected_,unsigned(note),pressure,2);}
   case UiMode::Library:if(library_catalog_controls()){if(!browser_.previewing)return true;return browser_.preview_instrument==Instrument::DrumKit?trigger_kit_pad(hardware,p,pressure):trigger_held_pad(hardware,selected_,uint8_t(bounded(48+int(p),0,127)),pressure,2); }return false;
   case UiMode::Browser:browser_.cursor=(browser_.cursor/16)*16+p;if(browser_.cursor>=browser_.count)browser_.cursor=browser_.count?browser_.count-1:0;return true;
   case UiMode::Routing:if(routing_fx_page_ && shifted()){selected_step_=uint8_t(page_*16+p);return true;}return select_track(p);
   case UiMode::Effects:if(fx_page_==4 && track_const(selected_).instrument==Instrument::DrumKit)return trigger_kit_pad(hardware,p,pressure);if(controls_[4]&0x08)return select_track(p);return trigger_held_pad(hardware,p,track(p).root_note,pressure,2);
   case UiMode::Diagnostics:return true;
   case UiMode::Sampler:if(track_const(selected_).instrument==Instrument::DrumKit)return trigger_kit_pad(hardware,p,pressure);if(pending_release_[hardware]){held_[hardware]=false;return false;}if(!set_sample_region((track(selected_).sample_edit.pads[p]%(track(selected_).sample_edit.slice_count?track(selected_).sample_edit.slice_count:track(selected_).sample_slices)),(track(selected_).sample_edit.slice_count?track(selected_).sample_edit.slice_count:track(selected_).sample_slices),track(selected_).sample_loop,track(selected_).sample_reverse)){held_[hardware]=false;return false;}return trigger_held_pad(hardware,selected_,track(selected_).sample_edit.zone_count?track(selected_).sample_edit.zones[track(selected_).sample_edit.pads[p]%track(selected_).sample_edit.zone_count].root:track(selected_).root_note,pressure,2,int(track(selected_).sample_edit.pads[p]));
   case UiMode::Step:if(tracker_view_)follow_display_=false;if(tag==0x10)held_step_hardware_=hardware;selected_step_=uint8_t(page_*16+p);if(shifted() || (controls_[4]&0x08))return true;if(track_const(selected_).instrument==Instrument::DrumKit){auto step=track_const(selected_).kit.steps[selected_kit_pad_][selected_step_];step.enabled=(controls_[5]&0x04)?0:!step.enabled;return set_kit_step(selected_kit_pad_,selected_step_,step);}return set_step(selected_step_,(controls_[5]&0x04)?false:!track(selected_).steps[selected_step_].enabled);
   case UiMode::Clip:if(controls_[4]&0x08)return select_pattern(p);return shifted()?stop_clip(selected_,launch_quantized_):launch_clip(selected_,p,launch_quantized_);
   case UiMode::Scene:scene_slot_=uint8_t(p);if(controls_[4]&0x08)return store_scene(p);return shifted()?append_chain(p):launch_scene(p,launch_quantized_);
   case UiMode::Mixer:return mute_or_solo(p,false);
   case UiMode::Modulation:if(modulation_page_==2 && !shifted()){auto route=project_.macros[macro_index_].routes[macro_route_];uint16_t mask=uint16_t(route.tracks^(1u<<p));if(!mask){notify(UiNotice::Failed);return false;}route.tracks=mask;return macro_route(macro_index_,macro_route_,route);}if(modulation_page_==1){automation_cell_=uint8_t((automation_cell_/16)*16+p);dirty_=true;return true;}if(modulation_page_==3 && shifted()){if(p>=12)return false;const auto& t=track_const(selected_);uint16_t mask=uint16_t(t.scale_mask^(1u<<p));if(!mask){notify(UiNotice::ScaleRequired);return false;}Command after{CommandType::SetScale,selected_,t.scale_root,mask},before=after;before.value=t.scale_mask;return edit(after,before);}return select_track(p);
   case UiMode::Generate: if(gen_page_){gen_cell_=uint8_t(p);selected_step_=uint8_t(page_*16+p);return true;}return euclidean(p+1);
   default:if(controls_[4]&0x08)return select_track(p);if(mode_==UiMode::Pad && track_const(selected_).instrument==Instrument::DrumKit)return trigger_kit_pad(hardware,p,pressure);return trigger_held_pad(hardware,p,track(p).root_note,pressure,2);
  }
 }
auto Performance::begin_midi_import() -> bool{
  if(modal_active() || file_action_pending() || mode_!=UiMode::Browser || !midi_browser())return false;
  MidiImportRequest request;const auto& b=browser_;
  if(!join_path(request.path,sizeof request.path,b.directory,b.entries[b.cursor].name))return false;
  request.generation=engine_.staged_generation();request.pattern=project_.active_pattern;request.track=selected_;request.channel=track_const(selected_).midi_channel;
  midi_import_request_=request;modal_.owner=ModalOwner::MidiImport;modal_.value=1;modal_.sync();
  copy_text(browser_.status,sizeof browser_.status,"PUSH: IMPORT / steps, RPN or NRPN may send");dirty_=true;return true;
 }
auto Performance::take_midi_import_request(MidiImportRequest& out) -> bool{
  if(modal_.owner!=ModalOwner::MidiImport || modal_.value!=2)return false;
  out=midi_import_request_;midi_import_request_={};modal_.value=3;modal_.sync();dirty_=true;return true;
 }
auto Performance::midi_import_finished(uint32_t ticket) -> bool{if(modal_.owner!=ModalOwner::MidiImport || midi_import_ticket_!=ticket)return false;modal_.owner=ModalOwner::None;modal_.value=0;midi_import_ticket_=0;modal_.sync();dirty_=true;return true;}
auto Performance::modal_accept() -> bool{
  if(browser_editing())return browser_edit_key(0);
  switch(modal_.owner){
   case ModalOwner::Project:return confirm_project();
   case ModalOwner::MidiImport:if(modal_.value!=1 || file_action_!=FileAction::None)return false;modal_.value=2;file_action_=FileAction::MidiImport;dirty_=true;return true;
   case ModalOwner::UiCommand:return accept_erase();
   case ModalOwner::Update:update_confirm_=false;if(playing_)return false;update_requested_=true;return true;
   case ModalOwner::Stretch:return stretch_modal()==1?request_file(FileAction::BeginStretch):false;
   case ModalOwner::Composition:return composition_apply();
   case ModalOwner::Twin:if(twin_modal_!=1 || !twin_config_.valid())return false;twin_modal_=0;return request_file(FileAction::BeginTwin);
   case ModalOwner::Freeze:return false;
   case ModalOwner::Loop:return loop_take();case ModalOwner::Extend:return extend_track();
   case ModalOwner::SongImport:song_import_armed_=false;return song_materialize();
   case ModalOwner::SongClear:song_clear_armed_=false;return song_clear();
   case ModalOwner::Reference:return synth_key(3);
   case ModalOwner::SourceDiscard:return source_confirm();
   default:return false;
  }
 }
auto Performance::modal_cancel() -> bool{if(browser_editing()){cancel_browser_edit();return true;}switch(modal_.owner){case ModalOwner::Twin:if(twin_modal_==2)return request_file(FileAction::CancelTwin);twin_modal_=0;dirty_=true;return true;case ModalOwner::MidiImport:if(file_action_==FileAction::MidiImport)file_action_=FileAction::None;if(midi_import_ticket_)midi_import_cancel_ticket_=midi_import_ticket_;midi_import_ticket_=0;midi_import_request_={};modal_.owner=ModalOwner::None;modal_.value=0;modal_.sync();break;case ModalOwner::Project:cancel_project();break;case ModalOwner::Stretch:if(stretch_modal()==2)return request_file(FileAction::CancelStretch);stretch_modal_=0;break;case ModalOwner::Freeze:return request_file(FileAction::CancelFreeze);default:modal_.owner=ModalOwner::None;modal_.value=0;modal_.sync();break;}dirty_=true;return true;}
auto Performance::pad_role()const -> ui::PadRole{
  using R=ui::PadRole;
  if(browser_editing())return R::Disabled;
  const bool kit=track_const(selected_).instrument==Instrument::DrumKit;
  if(mode_==UiMode::Song)return R::SongMute; // Song owns pad input before modifiers.
  if(controls_[4]&0x30){bool kit_target=!audio_page() && audio_lane().kind==TrackKind::Instrument && kit;return controls_[4]&0x10?(kit_target?R::KitSolo:R::Solo):(kit_target?R::KitMute:R::Mute);}
  if(audio_page())return R::TrackSelect;
  switch(mode_){
   case UiMode::Step:return select_held() && kit?R::KitSelect:R::Steps;
   case UiMode::Clip:return R::Clips;case UiMode::Scene:return R::Scenes;
   case UiMode::Keyboard:return step_chord_editor()?R::StepChord:R::Notes;
   case UiMode::Chord:return R::Chords;
   case UiMode::Library:return browser_.preview_instrument==Instrument::DrumKit?R::Kit:R::Notes;case UiMode::Browser:return R::Files;case UiMode::Project:return R::Assets;
   case UiMode::Settings:return R::Settings;case UiMode::Diagnostics:return R::Disabled;
   case UiMode::Sampler:return kit?(select_held()?R::KitSelect:R::Kit):R::Slices;
   case UiMode::Mixer:return R::Mute;
   case UiMode::Routing:return routing_fx_page_ && shifted()?R::Steps:R::TrackSelect;
   case UiMode::Generate:return gen_page_?R::Cells:R::Generate;
   case UiMode::Perform:return R::LiveFx;
   case UiMode::Modulation:
    if(modulation_page_==1)return R::Cells;
    if(modulation_page_==2 && !shifted())return R::Routes;
    if(modulation_page_==3 && shifted())return R::Scale;
    return R::TrackSelect;
   case UiMode::Effects:if(fx_page_==4 && kit)return R::Kit;break;
   default:break;
  }
  if(select_held())return R::TrackSelect;
  return mode_==UiMode::Pad && kit?R::Kit:R::Tracks;
 }
auto Performance::pad_feedback_context()const -> uint64_t{
  const auto role=pad_role();uint64_t key=uint64_t(generation_)+1;
  key=key*37+unsigned(role);key=key*37+unsigned(mode_);
  if(role==ui::PadRole::Kit || role==ui::PadRole::Slices || role==ui::PadRole::Notes || role==ui::PadRole::Chords){key=key*37+selected_;key=key*131+track_const(selected_).root_note;key=key*37+project_.active_pattern;}
  return key;
 }
auto Performance::pad_color_object(unsigned n)const -> ui::PadGround{
  using R=ui::PadRole;
  if(library_catalog_controls() && browser_.previewing)return {ui::engine_kind(browser_.preview_instrument)};
  switch(pad_role()){
   case R::Tracks:case R::TrackSelect:case R::Mute:case R::Solo:case R::SongMute:case R::Routes:return {ui::track_engine_kind(project_,n)};
   case R::Steps:case R::Notes:case R::Chords:case R::StepChord:case R::Kit:case R::KitSelect:case R::KitMute:case R::KitSolo:case R::Slices:case R::Clips:case R::Scenes:case R::Scale:return {ui::track_engine_kind(project_,selected_)};
   default:return ui::PadPalette::neutral;
  }
 }
auto Performance::pad_feedback(unsigned n)const -> ui::LedState{
  using S=ui::LedState;using R=ui::PadRole;
  if(n>=16)return S::Off;
  const auto role=pad_role();const auto& t=track_const(selected_);S state=S::Idle;
  if(library_catalog_controls()){if(!browser_.previewing)return S::Off;if(browser_.preview_instrument==Instrument::DrumKit)return browser_.preview_kit_mask&(1u<<n)?S::Assigned:S::Off;return S::Assigned;}
  switch(role){
   case R::Disabled:return S::Off;
   case R::Steps:{unsigned step=page_*16+n;bool kit=t.instrument==Instrument::DrumKit;
    bool enabled=kit?t.kit.steps[selected_kit_pad_][step].enabled:(t.steps[step].enabled || t.steps[step].lock_only);
    state=enabled?S::Assigned:S::Idle;if(step==selected_step_)state=S::Editing;
    if(playing_ && project_.clips[selected_].playing && project_.active_pattern==project_.clips[selected_].pattern && step==positions_[selected_])state=S::Playing;
    break;}
   case R::Files:{unsigned entry=(browser_.cursor/16)*16+n;state=entry<browser_.count?S::Assigned:S::Off;if(entry<browser_.count && entry==browser_.cursor)state=S::Selected;break;}
   case R::Assets:{unsigned asset=(asset_cursor_/16)*16+n;state=project_.assets[asset].path[0]?S::Assigned:S::Idle;if(asset==asset_cursor_)state=S::Selected;break;}
   case R::Settings:state=n<7?(n==settings_page_?S::Selected:S::Assigned):S::Off;break;
   case R::Cells:{unsigned cell=mode_==UiMode::Generate?gen_cell_:automation_cell_%16;state=mode_==UiMode::Generate || (automation_cell_/16)*16+n<t.automation[automation_lane_].length?S::Assigned:S::Idle;if(n==cell)state=S::Selected;break;}
   case R::Generate:state=S::Assigned;break;
   case R::Routes:state=project_.macros[macro_index_].routes[macro_route_].tracks&(1u<<n)?S::Selected:S::Idle;break;
   case R::Scale:state=n<12 && (t.scale_mask&(1u<<n))?S::Selected:S::Idle;break;
   case R::StepChord:{int key=keyboard_note(n);if(key<0){state=S::Off;break;}unsigned note=60+key-t.root_note;const auto& step=t.steps[selected_step_];bool on=step.enabled && step.note==note;for(auto chord:step.chord)on|=step.enabled && chord==note;state=on?S::Editing:keyboard_pad_state(n);break;}
   case R::Notes:case R::Chords:{state=keyboard_pad_state(n);break;}
   case R::Kit:case R::KitSelect:state=t.kit.pads[n].sample[0]<kSampleAssets || t.kit.pads[n].sample[1]<kSampleAssets?S::Assigned:S::Idle;if(n==selected_kit_pad_)state=S::Selected;if(t.kit.pads[n].muted || (t.kit.solo_mask && !(t.kit.solo_mask&(1u<<n))))state=S::Muted;break;
   case R::Slices:{unsigned count=t.sample_edit.slice_count?t.sample_edit.slice_count:t.sample_slices;if(!count)count=1;state=project_.assets[t.sample_slot].path[0]?S::Assigned:S::Idle;if(t.sample_edit.pads[n]%count==t.sample_slice%count)state=S::Selected;break;}
   case R::Clips:state=S::Assigned;if(project_.active_pattern==n)state=S::Selected;if(project_.clips[selected_].pattern==n && playing_ && project_.clips[selected_].playing)state=S::Playing;if(pending_[selected_]==int(n) || (pending_stop(selected_) && project_.clips[selected_].pattern==n))state=S::Queued;break;
   case R::Scenes:state=n==scene_slot_?S::Selected:S::Assigned;if(project_.active_scene==n && scene_matches_live(n) && playing_)state=S::Playing;if(queued_scene_==int(n))state=S::Queued;break;
   case R::Mute:state=track_muted_global(project_,n) || track_muted_pattern(project_,n)?S::Muted:S::Assigned;break;
   case R::KitMute:state=t.kit.pads[n].muted?S::Muted:S::Assigned;break;
   case R::Solo:state=track_solo(project_,n)?S::Selected:S::Idle;break;
   case R::KitSolo:state=t.kit.solo_mask&(1u<<n)?S::Selected:S::Idle;break;
   case R::SongMute:if(song_cursor_<project_.song.length){const auto& section=project_.song.sections[song_cursor_];bool on=section.ticks && shifted()?section.playing_mask&(1u<<n):!(section.mute_mask&(1u<<n));state=on?S::Assigned:S::Muted;}break;
   case R::LiveFx:{bool effect=performance_page_==4 && n>=2 && n<=(performance_fx_bank_?3u:12u);bool morph=n>=(performance_page_==4?13u:12u);state=n<2 || effect || morph?S::Assigned:S::Off;
    if((n==0 && stutter_active_) || (n==1 && fill_active_))state=S::PerformanceActive;
    if(effect && visual_.performance_fx_mode==unsigned(performance_fx_bank_?n+10:n-1))state=visual_.performance_fx_pending?S::Queued:S::PerformanceActive;
    if(effect && performance_fx_mode_==PerformanceFx(performance_fx_bank_?n+10:n-1) && visual_.performance_fx_mode!=unsigned(performance_fx_mode_))state=S::Queued;
    if(morph){float target=performance_page_==4?float(n-13)/2:float(n-12)/3;if(fabsf(morph_position()-target)<0.01f)state=S::Selected;}break;}
   case R::Tracks:case R::TrackSelect:state=(project_.audio[n].kind==TrackKind::Instrument?track_const(n).instrument!=Instrument::Empty:true)?S::Assigned:S::Idle;if(n==selected_)state=S::Selected;if(audible_muted(n))state=S::Muted;break;
  }
  ui::LedIntent intent{state,ui::PadPalette::ready};
  const unsigned hardware=mk3::pad_logical(n);
  const bool live=role==R::Tracks || role==R::Kit || role==R::Notes || role==R::Chords || role==R::Slices;
  if(live && pad_press_context_[hardware]==pad_feedback_context() && ((held_[hardware] && notes_held_[hardware]) || (settings_.motion==ui::Motion::Full && now_ms_>=pad_press_ms_[hardware] && now_ms_-pad_press_ms_[hardware]<ui::PadPalette::trigger_ms)))ui::LedStateEngine::overlay(intent,S::Triggered);
  // Note events identify tracks, not individual Kit hits/grains. Never map
  // a track's activity onto an unrelated kit pad, file, slice or cell.
  if(role==R::Tracks && settings_.motion==ui::Motion::Full && trigger_seen_[n] && now_ms_>=trigger_ms_[n] && now_ms_-trigger_ms_[n]<ui::PadPalette::trigger_ms)ui::LedStateEngine::overlay(intent,S::Triggered);
  if(role==R::Tracks || role==R::TrackSelect)ui::LedStateEngine::overlay(intent,recording_track_led(n));
  if((role==R::Kit || role==R::KitSelect) && (live_recording_ || recording_request_pending())){const auto target=recording_target();if(target.track==selected_ && target.kit_pad==n)ui::LedStateEngine::overlay(intent,recording_led_state());}
  return intent.state;
 }
auto Performance::led_frame(ui::LedFrame& out)const -> void{
  for(auto& v:out.buttons){v=0;}for(auto& v:out.button_rgb){v=0;}for(auto& v:out.pad_rgb){v=0;}for(auto& v:out.strip_rgb){v=0;}
  if(transfer_view_.active){for(auto& v:out.pads)v=0;for(auto& v:out.pad_state)v=ui::LedState::Off;for(auto& v:out.strip)v=0;for(auto& v:out.strip_level)v=0;out.buttons[19]=uint8_t(unsigned(ui::LedStateEngine::mono(true))*device_.led/3);out.buttons[17]=out.buttons[19];if(!transfer_view_.usb)out.buttons[12+unsigned(transfer_view_.station)]=out.buttons[19];if(!transfer_view_.started && !transfer_view_.failed)out.buttons[16]=out.buttons[19];return;}
  for(auto& v:out.strip_level)v=0;
  const auto& selected=track_const(selected_);float beat=visual_.beat;
  for(unsigned hardware=0;hardware<16;++hardware){
   out.pad_state[hardware]=pad_feedback(mk3::pad_logical(hardware));
   const auto& theme=ui::Theme::by_index(settings_.theme);unsigned logical=mk3::pad_logical(hardware);out.pads[hardware]=ui::PadPalette::encode(out.pad_state[hardware],now_ms_,settings_.motion,theme,pad_color_object(logical));
  }
  base_button_leds(out.buttons);
  out.buttons[6]=ui::LedStateEngine::mono(axes_.history.can_back());out.buttons[7]=ui::LedStateEngine::mono(axes_.history.can_forward());
  if(playing_)out.buttons[41]=ui::LedStateEngine::mono(true,settings_.motion==ui::Motion::Full && beat>0.18f);
  out.buttons[27]=ui::LedStateEngine::mono(mode_==UiMode::Perform || stutter_active_ || control_all_);
  out.buttons[22]=ui::LedStateEngine::mono(repeat_mode_!=0);out.buttons[11]=ui::LedStateEngine::mono(mode_==UiMode::Perform && performance_page_==1);
  out.buttons[9]=ui::LedStateEngine::mono(mode_==UiMode::Settings);out.buttons[4]=ui::LedStateEngine::mono(mode_==UiMode::Browser);out.buttons[8]=ui::LedStateEngine::mono(mode_==UiMode::Project);out.buttons[2]=ui::LedStateEngine::mono(mode_==UiMode::Song);out.buttons[10]=ui::LedStateEngine::mono(mode_==UiMode::Modulation);
  for(unsigned g=0;g<8;++g){unsigned t=track_bank_*8+g;bool group=mode_==UiMode::Routing && routing_fx_page_;out.buttons[29+g]=ui::LedStateEngine::shortcut(ui::Theme::by_index(settings_.theme),group?false:track_bank_!=0,group?g==group_cursor_:t==selected_,!group && audible_muted(t),group?ui::LedState::Off:recording_track_led(t),beat,now_ms_,settings_.motion,device_.led);}
  out.buttons[42]=ui::LedStateEngine::mono_state(recording_led_state(),beat,now_ms_,settings_.motion);out.buttons[24]=ui::LedStateEngine::mono(lock_point_ || reload_point_);out.buttons[45]=ui::LedStateEngine::mono(fixed_velocity_);out.buttons[55]=ui::LedStateEngine::mono(controls_[4]&0x08);out.buttons[39]=ui::LedStateEngine::mono(project_.ui.metronome);out.buttons[40]=ui::LedStateEngine::mono(follow_display_);out.buttons[52]=ui::LedStateEngine::mono(!modal_active() && (shifted()?history_cursor_<history_size_:history_cursor_>0));out.buttons[25]=ui::LedStateEngine::mono(axes_.strip_mode==StripMode::Pitch);out.buttons[26]=ui::LedStateEngine::mono(axes_.strip_mode==StripMode::Mod);out.buttons[28]=ui::LedStateEngine::mono(axes_.strip_mode==StripMode::Notes);out.buttons[37]=ui::LedStateEngine::mono(true,!(controls_[5]&0x02));out.buttons[38]=ui::LedStateEngine::mono((controls_[5]&0x04)!=0);out.buttons[54]=ui::LedStateEngine::mono(true,!(controls_[4]&0x04));out.buttons[20]=ui::LedStateEngine::mono(main_encoder_mode_==1);out.buttons[21]=ui::LedStateEngine::mono(main_encoder_mode_==2);out.buttons[23]=ui::LedStateEngine::mono(main_encoder_mode_==3);
  out.buttons[58]=encoder_direction_available(0x04)?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).accent,ui::Theme::by_index(settings_.theme)),2):0;
  out.buttons[59]=encoder_direction_available(0x20)?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).accent,ui::Theme::by_index(settings_.theme)),2):0;
  out.buttons[60]=encoder_direction_available(0x08)?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).accent,ui::Theme::by_index(settings_.theme)),2):0;
  out.buttons[61]=encoder_direction_available(0x10)?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).accent,ui::Theme::by_index(settings_.theme)),2):0;
  if(warning())out.buttons[43]=ui::LedStateEngine::mono_state(ui::LedState::Warning,beat,now_ms_,settings_.motion);
  if(mode_==UiMode::Perform && now_ms_<strip_interaction_until_ms_){
   float amount=performance_fx_mode_!=PerformanceFx::Off?performance_fx_amount_:performance_page_==3?float(selected.mutation.amount)/100:control_all_?control_all_position_:morph_position();unsigned cursor=unsigned(ui::clamp01(amount)*24+0.5f);
   for(unsigned i=0;i<25;++i)out.strip_level[i]=uint8_t(i==cursor?3:i<cursor?1:0);
  }else{
   // The 25 physical LEDs are one continuous bar. Extrapolate the 20 Hz
   // audio snapshot to the current UI tick so fast tempos never skip LEDs.
   float phase=0;
   if(settings_.motion!=ui::Motion::Off && playing_ && visual_.playing){
    uint64_t elapsed=now_ms_>=visual_received_ms_?now_ms_-visual_received_ms_:0;
    if(elapsed>100)elapsed=100;
    if(settings_.motion==ui::Motion::Reduced)elapsed=0;
    phase=visual_.bar_phase+float(elapsed)*float(project_.tempo)/240000.0f;
    phase-=float(unsigned(phase));
   }
   if(settings_.motion==ui::Motion::Reduced)phase=float(unsigned(phase*4))/4;
   unsigned head=unsigned(phase*25.0f)%25;
   out.strip_level[head]=3;
   if(playing_ && settings_.motion==ui::Motion::Full){out.strip_level[(head+24)%25]=2;out.strip_level[(head+23)%25]=1;out.strip_level[(head+22)%25]=1;}
  }
  for(unsigned i=0;i<25;++i)out.strip[i]=settings_.strip==ui::StripProfile::Uncalibrated?0:!out.strip_level[i]?0:settings_.strip==ui::StripProfile::PaletteHypothesis?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).mod,ui::Theme::by_index(settings_.theme)),out.strip_level[i]):ui::LedStateEngine::mono(true,out.strip_level[i]<3);
  if(mode_==UiMode::Diagnostics && diagnostic_test_==1){for(unsigned i=0;i<16;++i)out.pads[i]=mk3::indexed(unsigned(((settings_.motion==ui::Motion::Off?0:now_ms_/500)+i)%16)+1,2);for(unsigned i=0;i<62;++i)out.buttons[i]=ui::LedStateEngine::mono((i+(settings_.motion==ui::Motion::Off?0:now_ms_/500))%2==0);}
  if(device_.led<3){for(unsigned i=0;i<16;++i){auto& code=out.pads[i];if(code){unsigned level=code&3;bool pulse=out.pad_state[i]==ui::LedState::Queued || out.pad_state[i]==ui::LedState::Armed || out.pad_state[i]==ui::LedState::Warning;code=device_.led==1 && level==1 && pulse?0:uint8_t((code&~3u)|(level<device_.led?level:device_.led));}}for(unsigned i=0;i<62;++i)if((i<29 || i>36) && out.buttons[i])out.buttons[i]=uint8_t(unsigned(out.buttons[i])*device_.led/3);for(unsigned i=0;i<25;++i){if(out.strip_level[i]>device_.led)out.strip_level[i]=device_.led;if(out.strip[i])out.strip[i]=settings_.strip==ui::StripProfile::PaletteHypothesis?mk3::indexed(ui::LedStateEngine::token_hue(ui::Theme::by_index(settings_.theme).mod,ui::Theme::by_index(settings_.theme)),out.strip_level[i]):uint8_t(unsigned(out.strip[i])*device_.led/3);}}
  const auto& tokens=ui::Theme::by_index(settings_.theme);
  for(unsigned i=0;i<16;++i)out.pad_rgb[i]=ui::LedStateEngine::preview(out.pads[i],tokens);
  for(unsigned i=0;i<62;++i)if(out.buttons[i]){uint32_t color=i==42?tokens.danger:i==41?tokens.motion:i==43 && warning()?tokens.warn:tokens.accent;if(i>=29 && i<=36){unsigned track=track_bank_*8+i-29;bool group=mode_==UiMode::Routing && routing_fx_page_;color=(!group && audible_muted(track))?tokens.dim:(group?i-29==group_cursor_:track==selected_)?tokens.accent:tokens.secondary;if((group?ui::LedState::Off:recording_track_led(track))!=ui::LedState::Off)color=tokens.danger;}out.button_rgb[i]=color;}
  for(unsigned i=0;i<25;++i)if(out.strip_level[i])out.strip_rgb[i]=tokens.mod;


 }
auto Performance::automation(unsigned lane,AutomationLane state) -> bool{state.parameter=bind_parameter_target(track_const(selected_).instrument,state.parameter);if(lane>=2 || !valid_automation(state))return false;Command after{CommandType::SetAutomation,selected_,uint8_t(lane),0},before=after;after.automation=state;before.automation=track_const(selected_).automation[lane];return edit(after,before);}
auto Performance::swap_group_inserts() -> bool{if(group_cursor_>=8 || engine_.command_space()<8)return false;float base[8],a[8],b[8];const auto& group=project_.routing.groups[group_cursor_];for(unsigned i=0;i<8;++i){uint16_t id=group_fx_id(group_cursor_,i);unsigned mi=morph_index(id);if(mi>=kCommonMorphParameters)return false;base[i]=group.insert_fx[i];a[i]=project_.morph_a[0].values[mi];b[i]=project_.morph_b[0].values[mi];}EditGroup edit_group(*this);for(unsigned i=0;i<8;++i){unsigned other=(i+4)%8;Command after{CommandType::SetParameter,selected_,1,0},before=after;after.parameter=before.parameter=group_fx_id(group_cursor_,i);after.scalar=base[other];after.scalar2=a[other];after.scalar3=b[other];before.scalar=base[i];before.scalar2=a[i];before.scalar3=b[i];if(!edit(after,before)){return false;}}return edit_group.finish(true);}
auto Performance::chain_edit() -> bool{if(chain_node_==0){set_mode(UiMode::Synth);instrument_view_=1;dirty_=true;return true;}if(chain_node_==3){fx_page_=2;dirty_=true;return true;}fx_focus_slot_=uint8_t(chain_node_-1);fx_page_=3;dirty_=true;return true;}
auto Performance::swap_track_inserts() -> bool{if(engine_.command_space()<8)return false;float base[8],a[8],b[8];const auto& track=track_const(selected_);for(unsigned i=0;i<8;++i){const unsigned mi=morph_index(uint16_t(94+i));if(mi>=kCommonMorphParameters)return false;base[i]=track.insert_fx[i];a[i]=project_.morph_a[selected_].values[mi];b[i]=project_.morph_b[selected_].values[mi];}EditGroup edit_group(*this);for(unsigned i=0;i<8;++i){unsigned other=(i+4)%8;Command after{CommandType::SetParameter,selected_,1,0},before=after;after.parameter=before.parameter=uint16_t(94+i);after.scalar=base[other];after.scalar2=a[other];after.scalar3=b[other];before.scalar=base[i];before.scalar2=a[i];before.scalar3=b[i];if(!edit(after,before)){return false;}}return edit_group.finish(true);}
auto Performance::reset_live() -> bool{if(!send({CommandType::ResetLive,0,0,0}))return false;project_.morph_enabled=false;for(auto& m:project_.macros)m.active=false;control_all_=stutter_active_=fill_active_=false;performance_fx_mode_=PerformanceFx::Off;performance_fx_latched_=false;held_performance_pad_=255;release_performance_fx_=false;control_all_position_=0.5f;held_stutter_pad_=held_fill_pad_=255;dirty_=true;return true;}
auto Performance::lock_press() -> bool{if(shifted())return temporary_reload();if(lock_point_){if(!install_snapshot(lock_point_))return false;delete lock_point_;lock_point_=nullptr;dirty_=true;return true;}return capture_snapshot(lock_point_);}
auto Performance::publish_midi_import(const MidiImport& m) -> SequenceCommitResult{
  if(!m.kit_import())return publish_track_sequence(m.pattern(),m.track(),m.result(),m.cc_lane_mask(),m.nrpn_slot_mask());
  if(engine_.project_event_loss())return SequenceCommitResult::SyncLost;
  if(!engine_.project_snapshot_safe() || !engine_.command_space())return SequenceCommitResult::Busy;
  auto* before=alloc_new<PatternState>(project_,m.pattern());auto* after=before?alloc_new<PatternState>(*before):nullptr;if(!after){delete before;return SequenceCommitResult::NoMemory;}
  before->include_settings=after->include_settings=false;before->name_mask=after->name_mask=uint16_t(1u<<m.track());copy_text(before->track_names[m.track()],96,project_.tracks[m.track()].track_name);copy_text(after->track_names[m.track()],96,m.result().track_name);auto& target=after->tracks[m.track()];auto imported=m.result();target.length=imported.length;for(unsigned pad=0;pad<16;++pad)for(unsigned cell=0;cell<kSteps;++cell)target.kit.steps[pad][cell]=imported.kit.steps[pad][cell];
  uint16_t mask=uint16_t(m.spill_mask()|(1u<<m.track()));for(unsigned t=0;t<16;++t)if(m.spill_mask()&(1u<<t)){auto spill=m.spill(t);auto& dst=after->tracks[t];dst.length=spill.length;for(unsigned cell=0;cell<kSteps;++cell)dst.steps[cell]=spill.steps[cell];}
  for(unsigned t=0;t<16;++t)if((mask&(1u<<t)) && !valid_track(after->tracks[t])){delete before;delete after;return SequenceCommitResult::Invalid;}
  if(!engine_.replace_pattern(m.pattern(),*after,mask,true)){delete before;delete after;return SequenceCommitResult::Busy;}
  clear_record_ids(m.pattern(),mask);Command c{CommandType::ApplyPattern,0,1,uint16_t(m.pattern())};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,m.pattern(),*after,mask);refresh_steps();dirty_=true;return SequenceCommitResult::Applied;
 }
auto Performance::publish_track_sequence(unsigned pattern,unsigned track_index,ConstTrackView imported,unsigned automation_mask,unsigned nrpn_mask) -> SequenceCommitResult{
  if(pattern>=kPatterns || track_index>=kTracks || automation_mask>3 || nrpn_mask>15 || project_.audio[track_index].kind!=TrackKind::Instrument)return SequenceCommitResult::Invalid;
  if(engine_.project_event_loss())return SequenceCommitResult::SyncLost;
  if(!engine_.project_snapshot_safe() || !engine_.command_space())return SequenceCommitResult::Busy;
  if(!allocation_admitted(sizeof(PatternState),2))return SequenceCommitResult::NoMemory;
  auto* before=alloc_new<PatternState>(project_,pattern);auto* after=before?alloc_new<PatternState>(*before):nullptr;
  if(!before || !after){delete before;delete after;return SequenceCommitResult::NoMemory;}
  before->include_settings=after->include_settings=nrpn_mask!=0;before->name_mask=after->name_mask=uint16_t(1u<<track_index);copy_text(before->track_names[track_index],96,project_.tracks[track_index].track_name);copy_text(after->track_names[track_index],96,imported.track_name);auto& target=after->tracks[track_index];
  for(unsigned i=0;i<kSteps;++i)target.steps[i]=imported.steps[i];
  target.length=imported.length;
  for(unsigned i=0;i<2;++i)if(automation_mask&(1u<<i))target.automation[i]=imported.automation[i];
  for(unsigned i=0;i<4;++i)if(nrpn_mask&(1u<<i))target.nrpn[i]=imported.nrpn[i];
  if(!valid_track(target) || after->swing>100){delete before;delete after;return SequenceCommitResult::Invalid;}
  uint16_t mask=uint16_t(1u<<track_index);
  if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;return SequenceCommitResult::Busy;}
  clear_record_ids(pattern,mask);Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();dirty_=true;return SequenceCommitResult::Applied;
 }
auto Performance::replace_pattern_contents(const PatternState& value) -> bool{if(!engine_.command_space())return false;if(!allocation_admitted(sizeof(PatternState),2))return memory_refused();auto* before=alloc_new<PatternState>(project_,project_.active_pattern);auto* after=before?alloc_new<PatternState>(value):nullptr;if(!before || !after){delete before;delete after;return memory_refused();}before->include_settings=after->include_settings;if(!engine_.replace_pattern(project_.active_pattern,*after)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,0,0,project_.active_pattern};push_history(c,c,before,after);install_pattern(project_,project_.active_pattern,*after);refresh_steps();return true;}
auto Performance::extend_track() -> bool{
  const unsigned pattern=project_.active_pattern, length=track_const(selected_).length;
  if(!length || length>kSteps/2){extend_pending_=false;notify(UiNotice::Failed);return false;}
  if(!extend_pending_ || extend_pattern_!=pattern || extend_track_!=selected_ || extend_length_!=length || now_ms_-extend_time_>5000){extend_pending_=true;extend_pattern_=uint8_t(pattern);extend_track_=selected_;extend_length_=uint16_t(length);extend_time_=now_ms_;notify(UiNotice::ExtendConfirm);return true;}
  extend_pending_=false;
  auto* before=new PatternState(project_,pattern);auto* after=new PatternState(*before);auto& target=after->tracks[selected_];
  repeat_sequence_steps(target,length);
  for(auto& lane:target.automation)if(lane.length==length){for(unsigned i=0;i<length;++i)lane.values[length+i]=lane.values[i];lane.length=uint16_t(length*2);}
  target.length=uint16_t(length*2);uint16_t mask=uint16_t(1u<<selected_);
  if(!engine_.replace_pattern(pattern,*after,mask,true)){delete before;delete after;notify(UiNotice::Failed);return false;}
  Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);refresh_steps();notify(UiNotice::Extended);return true;
 }
auto Performance::copy_pattern() -> bool{if(copy_track_only_){if(!phrase_clipboard_)phrase_clipboard_=alloc_new<PhraseState>();if(!phrase_clipboard_)return memory_refused();*phrase_clipboard_=project_.patterns[project_.active_pattern].tracks[selected_];}else{if(!pattern_clipboard_)pattern_clipboard_=alloc_new<PatternState>();if(!pattern_clipboard_)return memory_refused();snapshot_pattern(*pattern_clipboard_,project_,project_.active_pattern);}clipboard_track_only_=copy_track_only_;clipboard_track_=selected_;pattern_copied_=true;notify(UiNotice::Copied);return true;}
auto Performance::duplicate_pattern() -> bool{unsigned target=kPatterns;for(unsigned p=project_.active_pattern+1;p<kPatterns;++p)if(pattern_is_empty(p)){target=p;break;}if(target>=kPatterns){notify(UiNotice::Failed);return false;}bool scope=copy_track_only_;copy_track_only_=false;bool copied=copy_pattern();copy_track_only_=scope;if(!copied)return false;if(!select_pattern(target))return false;return paste_pattern();}
auto Performance::double_pattern() -> bool{const auto& cur=project_.patterns[project_.active_pattern];for(const auto& t:cur.tracks)if(t.length>kSteps/2){notify(UiNotice::Failed);return false;}auto* after=new PatternState(project_,project_.active_pattern);for(auto& t:after->tracks){unsigned len=t.length;repeat_sequence_steps(t,len);t.length=uint16_t(len*2);for(auto& lane:t.automation)if(lane.enabled && lane.length && lane.length<=kAutomationSteps/2){for(unsigned s=0;s<lane.length;++s)lane.values[lane.length+s]=lane.values[s];lane.length=uint16_t(lane.length*2);}}return commit_pattern(after);}
auto Performance::set_click_setting(unsigned bit) -> bool{unsigned old=(project_.ui.metronome?1:0)|(project_.ui.count_in?2:0)|(project_.ui.click_main?4:0);Command after{CommandType::SetClickSettings,0,0,uint16_t(old^bit)},before=after;before.value=old;return edit(after,before);}
auto Performance::synth_controls()const  -> SynthControlContext{
  return {track_const(selected_).instrument,shifted(),wave_profile(),wave_clock(),
          sn_ref_confirmation(),history_cursor_>0,history_cursor_<history_size_,project_.sounds[sound_slot_].instrument!=Instrument::Empty};
 }
auto Performance::trigger_held_pad(uint8_t hardware,unsigned target,unsigned note,uint16_t pressure,uint8_t kind,int slice) -> bool{if(pending_release_[hardware]){held_[hardware]=false;return false;}Command c{CommandType::Trigger,uint8_t(target),hardware,uint16_t(fixed_velocity_?127:pad_velocity(pressure,device_.velocity))};c.parameter=kind;c.scalar=float(note);c.scalar2=kind==3?float(project_.ui.chord_shape):0;c.scalar3=kind==3?float(project_.ui.chord_inversion):0;if(slice>=0){c.pattern=254;c.scalar3=float(slice);}if(!send(c)){held_[hardware]=false;return false;}held_notes_[hardware]=uint8_t(note);held_tracks_[hardware]=uint8_t(target);held_kind_[hardware]=kind;notes_held_[hardware]=true;return true;}
auto Performance::retry_releases()  -> void{
  if(strip_release_pending_ && !release_strip())return;
  if(repeat_release_pending_){if(!note_repeat(0,repeat_rate_))return;repeat_release_pending_=false;}
  if(release_stutter_){if(!stutter(false))return;release_stutter_=false;}
  if(release_fill_){if(!set_fill(false))return;release_fill_=false;}
  if(release_performance_fx_){if(!set_performance_fx(PerformanceFx::Off,performance_fx_amount_,false))return;release_performance_fx_=false;}
  if(release_control_all_){Command c{CommandType::SetControlAll,0,0,0};if(!send(c))return;release_control_all_=false;}
  for(unsigned i=0;i<16;++i)if(pending_release_[i]){Command c{CommandType::NoteOff,held_tracks_[i],uint8_t(i),held_notes_[i]};c.parameter=held_kind_[i];if(send(c)){pending_release_[i]=false;notes_held_[i]=false;}}
 }
auto Performance::edit(Command after,Command before)  -> bool{
  if(after.pattern>=kPatterns)after.pattern=project_.audio[after.track].kind==TrackKind::Instrument?project_.active_pattern:0;
  if(before.pattern>=kPatterns)before.pattern=after.pattern;
  Instrument next=track_const(after.track).instrument;
  if(after.type==CommandType::SetInstrument)next=Instrument(after.value);
  else if(after.type==CommandType::SetSample)next=after.step==unsigned(Instrument::Granular)?Instrument::Granular:Instrument::Sampler;
  else if(after.type==CommandType::RestoreSampleAssignment)next=Instrument(after.step);
  else if(after.type==CommandType::ApplySound)next=after.sound().instrument;
  TrackHistory* old_state=nullptr;TrackHistory* new_state=nullptr;
  if(next!=track_const(after.track).instrument){old_state=alloc_new<TrackHistory>(track_views_.at(after.pattern,after.track));new_state=old_state?alloc_new<TrackHistory>(*old_state):nullptr;if(!old_state || !new_state){delete old_state;delete new_state;return memory_refused();}capture_track_morph(old_state->morph,project_,after.track);}
  if(!send(after)){delete old_state;delete new_state;return false;}
  mirror(after);
  if(old_state){new_state->assign(track_views_.at(after.pattern,after.track));capture_track_morph(new_state->morph,project_,after.track);Command history{CommandType::ApplyPattern,after.track,0,after.pattern};history.parameter=uint16_t(1u<<after.track);history.frames=after.type==CommandType::SetSample?1:0;push_history(history,history,nullptr,nullptr,old_state,new_state);}else push_history(before,after);return true;
 }
auto Performance::dispose(History& h) -> void{delete h.pattern_before;delete h.pattern_after;delete h.track_before;delete h.track_after;h.track_before=h.track_after=nullptr;h.pattern_before=h.pattern_after=nullptr;h.before=Command{};h.after=Command{};}
auto Performance::install_single_track(unsigned pattern,unsigned t,const TrackState& state,bool rebound) -> void{auto target=track_views_.at(pattern,t);if(target.instrument!=state.instrument || target.sample_slot!=state.sample_slot || rebound)++assignment_revision_[t];bool changed=target.instrument!=state.instrument;target=state;if(changed)reset_track_morph(project_,t);clear_record_ids(pattern,uint16_t(1u<<t));}
auto Performance::remember_rotation(unsigned pattern,unsigned track,uint32_t from,uint32_t to,unsigned length,int direction) -> void{rotation_revisions_[rotation_cursor_]={from,to,uint16_t(length),uint8_t(pattern),uint8_t(track),int8_t(direction)};rotation_cursor_=(rotation_cursor_+1)%128;for(auto& r:record_ids_)if(r.id && r.pattern==pattern && r.track==track){if(r.step>=length){r.id=0;notify(UiNotice::RecordLost);}else r.step=uint8_t(direction>0?(r.step+1)%length:(r.step+length-1)%length);}}
auto Performance::mirror(const Command& c)  -> void{
  if(c.type==CommandType::SetStep || c.type==CommandType::SetStepNote || c.type==CommandType::SetStepChord || c.type==CommandType::SetGate || c.type==CommandType::SetRecordedStep)forget_record(c.pattern,c.track,c.step);
  auto t=track_views_.at(c.pattern,c.track);
  const Instrument old_instrument=t.instrument;const uint8_t old_slot=t.sample_slot;
  switch(c.type) {
   case CommandType::SetSample:if(c.parameter==1)project_.assets[c.value].metadata=c.metadata;t.sample_slot=uint8_t(c.value);{auto next=c.step==unsigned(Instrument::Granular)?Instrument::Granular:Instrument::Sampler;if(t.instrument!=next){t.definition_cookie=0;t.preset_name[0]=0;t.preset_identity={};t.preset_slot=kNoPreset;t.preset_fx={};t.output_trim=1;}t.instrument=next;}break;
   case CommandType::RestoreSampleAssignment:t.sample_slot=uint8_t(c.value);t.instrument=Instrument(c.step);if(c.track==selected_ && c.pattern==project_.active_pattern)instrument_view_=0;break;
   case CommandType::SetAutomation:t.automation[c.step]=c.automation;break;
   case CommandType::SetNrpnConfig:t.nrpn[c.step].number=c.parameter;t.nrpn[c.step].enabled=c.value!=0;t.nrpn[c.step].registered=c.scalar==1;break;
   case CommandType::SetMacroValue:project_.macros[c.step].value=c.scalar;project_.macros[c.step].active=c.value!=0;break;
   case CommandType::SetMacroRoute:project_.macros[c.step].routes[c.value]=c.route;break;
   case CommandType::SetChain:project_.chain=c.chain;break;case CommandType::SetScene:project_.scenes[c.value]=c.scene;break;    case CommandType::SetSong:project_.song=c.song_state;if(!c.song_state.enabled)arrangement_ui_active_=false;break;    case CommandType::SetSongSection:project_.song.sections[c.value]=c.song_section;break;
   case CommandType::SetSoundOverride:t.sound_override=c.value;break;
   case CommandType::SetSound:install_pool_definition(project_,c.value,c.sound());break;
   case CommandType::RegisterPreset:project_.sounds[c.value]=c.sound();t.preset_identity=c.sound().identity;t.preset_slot=c.value;t.preset_fx=c.sound().fx;t.output_trim=c.sound().output_trim;copy_track_field(t.preset_name,c.sound().preset_name);twin_registered(project_,c.track,c.value);break;
   case CommandType::ApplySound:mount_sound(t,c.sound());t.preset_slot=preset_slot_for(project_,c.sound().identity);instrument_view_=0;break;
   case CommandType::SetMidiProgram:t.midi_program=uint8_t(c.value);break;case CommandType::SetMidiInputRoute:project_.midi_input_route[c.step]=uint8_t(c.value);break;case CommandType::SetMidiChannel:t.midi_channel=uint8_t(c.value);break;case CommandType::SetTranspose:t.transpose=int8_t(c.scalar);break;case CommandType::SetVelocityScale:t.velocity_scale=c.scalar;break;case CommandType::SetScale:t.scale_mask=c.value;t.scale_root=c.step;break;case CommandType::SetExternalClock:project_.midi_clock_slave=c.value!=0;break;
   case CommandType::SetTempo:project_.tempo=c.value;break;case CommandType::SetSwing:project_.patterns[c.pattern].swing=uint8_t(c.value);break;
   case CommandType::SetNoteInputQuantize:project_.ui.note_quantized=c.value!=0;break;
   case CommandType::SetNoteReplace:project_.ui.note_replace=c.value!=0;break;
   case CommandType::SetClickSettings:project_.ui.metronome=c.value&1;project_.ui.count_in=c.value&2;project_.ui.click_main=c.value&4;break;
   case CommandType::SetStepNote:t.steps[c.step].note=uint8_t(c.value);break;
   case CommandType::SetRecordedStep:t.steps[c.step]=c.recorded_step;break;
   case CommandType::SetStepChord:t.steps[c.step].chord[c.parameter]=uint8_t(c.value);break;
   case CommandType::SetGrooveSnapshot:t.groove=c.groove;break;
   case CommandType::SetAdvanced:set_advanced(t,AdvancedGroup(c.parameter),c.value,int(c.scalar),c.step);break;
   case CommandType::SetClipFollow:t.clip_follow=c.clip_follow;break;
   case CommandType::SetFollow:project_.chain.mode=FollowMode(c.value);project_.chain.probability=c.step;break;
   case CommandType::SetSpeed:t.speed_num=uint8_t(c.value);t.speed_den=c.step;break;
   case CommandType::SetStepAttributes:t.steps[c.step].attributes=uint8_t(c.value);break;
   case CommandType::SetStep:t.steps[c.step].enabled=c.value!=0;break;
   case CommandType::SetKitPad:if(c.step<16){const auto prior=t.kit.pads[c.step];t.kit.pads[c.step]=c.kit_pad;if(prior.sample[0]!=c.kit_pad.sample[0] || prior.sample[1]!=c.kit_pad.sample[1])++assignment_revision_[c.track];if(c.parameter==1 && c.value<kSampleAssets)project_.assets[c.value].metadata=c.metadata;}break;
   case CommandType::SetKitStep:if(c.parameter<16)t.kit.steps[c.parameter][c.step]=c.kit_step;break;
   case CommandType::SetKitSolo:t.kit.solo_mask=c.value;break;
   case CommandType::SetSampleRegion:t.sample_slice=c.step;t.sample_slices=uint8_t(c.value);t.sample_loop=c.parameter&1;t.sample_reverse=c.parameter&2;if(t.sample_loop)t.sample_edit.mode=SampleMode::Loop;else if(t.sample_edit.mode==SampleMode::Loop)t.sample_edit.mode=SampleMode::OneShot;break;
   case CommandType::SetSoundLock:t.steps[c.step].sound=c.value;break;
   case CommandType::SetGate:t.steps[c.step].gate=c.value;break;
   case CommandType::SetCycle:t.steps[c.step].cycle_every=uint8_t(c.value);t.steps[c.step].cycle_phase=uint8_t(c.parameter);t.steps[c.step].condition=c.scalar>=1 && c.scalar<=6?TrigCondition(unsigned(c.scalar)-1):TrigCondition::Cycle;break;
   case CommandType::SetProbability:t.steps[c.step].probability=uint8_t(c.value);break;
   case CommandType::SetVelocity:t.steps[c.step].velocity=uint8_t(c.value);break;
   case CommandType::SetRatchet:t.steps[c.step].ratchet=uint8_t(c.value);break;
   case CommandType::SetMicrotiming:t.steps[c.step].microtiming=int16_t(c.scalar);break;
   case CommandType::SetCondition:t.steps[c.step].condition=TrigCondition(c.value);break;
   case CommandType::SetDirection:t.direction=PlaybackDirection(c.value);break;
   case CommandType::SetMute:if(arrangement_ui_active_ && (c.parameter!=1 || c.pattern==project_.clips[c.track].pattern))set_track_mixer_flag(arrangement_ui_mute_,c.track,c.value!=0);if(c.parameter==1){if(c.pattern<kPatterns)set_track_mixer_flag(project_.patterns[c.pattern].mute_mask,c.track,c.value!=0);}else set_track_mixer_flag(project_.muted_tracks,c.track,c.value!=0);break;
   case CommandType::SetSolo:if(arrangement_ui_active_)set_track_mixer_flag(arrangement_ui_solo_,c.track,c.value!=0);set_track_mixer_flag(project_.solo_tracks,c.track,c.value!=0);break;
   case CommandType::SetInstrument:if(t.instrument!=Instrument(c.value)){t.definition_cookie=0;t.preset_name[0]=0;t.preset_identity={};t.preset_slot=kNoPreset;t.preset_fx={};t.output_trim=1;}t.instrument=Instrument(c.value);if(c.track==selected_ && c.pattern==project_.active_pattern)instrument_view_=0;break;
   case CommandType::SetLength:t.length=c.value;break;
   case CommandType::SetNote:t.root_note=uint8_t(c.value);break;
   case CommandType::SetEngineParameter:edit_engine_parameter(project_,t,c.track,c.parameter,c.scalar,c.step,c.scalar2,c.scalar3);break;
   case CommandType::SetAssetPlayback:if(c.value<kSampleAssets){project_.assets[c.value].metadata.root=c.metadata.root;project_.assets[c.value].metadata.bpm=c.metadata.bpm;}break;
   case CommandType::SetAssetFavorite:if(c.value<kSampleAssets){if(project_.assets[c.value].metadata.favorite!=(c.step!=0))++favorite_revision_;project_.assets[c.value].metadata.favorite=c.step!=0;}break;
   case CommandType::SetSampleEdit:t.sample_edit=c.sample_edit;t.sample_loop=c.sample_edit.mode==SampleMode::Loop;if(c.parameter==1){t.sample_slice=c.step;t.sample_slices=uint8_t(c.value);}break;
   case CommandType::SetRouting:project_.routing=c.routing;break;
   case CommandType::SetTrackBus:t.output_bus=uint8_t(c.value);break;
   case CommandType::SetTrackSendMode:t.send_prefader=uint8_t(c.value);break;
   case CommandType::SetSampleSlot:t.sample_slot=uint8_t(c.value);break;
   case CommandType::SetCreative:t.creative=c.creative;break;
   case CommandType::SetMutation:t.mutation={c.frames,uint8_t(c.value),c.parameter!=0};break;
   case CommandType::SetTrackArp:t.arp_mode=uint8_t(c.value);t.arp_rate=c.step;t.arp_octaves=uint8_t(c.parameter);break;
   case CommandType::SetChordConfig:project_.ui.chord_shape=uint8_t(c.value);project_.ui.chord_inversion=uint8_t(c.parameter);break;
   case CommandType::SetRepeatConfig:project_.ui.arp_direction=c.step;project_.ui.arp_octaves=uint8_t(c.parameter);project_.ui.repeat_gate=uint8_t(c.value);break;
   case CommandType::SetTuning:project_.tuning[c.step]=c.scalar;break;

   case CommandType::SetParameter:set_parameter_base(project_,t,c.parameter,c.scalar);if(c.parameter>=128 && c.parameter<256 && c.value==2)t.midi_cc_used[(c.parameter-128)/8]&=uint8_t(~(1u<<((c.parameter-128)%8)));if(c.parameter>=260 && c.parameter<=261 && c.value==2)t.expression_used&=uint8_t(~(1u<<(c.parameter-260)));if(c.step==1 || (c.step==0 && project_.morph_enabled && (((c.parameter>=64 && c.parameter<=76) || (c.parameter>=384 && c.parameter<474)) || c.pattern==project_.clips[c.track].pattern))){unsigned mt=((c.parameter>=64 && c.parameter<=76) || (c.parameter>=384 && c.parameter<474))?0:c.track;set_morph_endpoint(project_.morph_a[mt],t,c.parameter,c.step?c.scalar2:c.scalar);set_morph_endpoint(project_.morph_b[mt],t,c.parameter,c.step?c.scalar3:c.scalar);}break;
   case CommandType::SetStepLock:t.steps[c.step].locks[c.value]={c.parameter,c.scalar,true};break;
   case CommandType::ClearStepLock:t.steps[c.step].locks[c.value].enabled=false;break;
   default:break;
  }
  if(t.instrument!=old_instrument)reset_track_morph(project_,c.track);
  if(c.restore_morph && c.morph_payload())restore_track_morph(project_,c.track,*c.morph_payload());
  // Rebinding the same slot is still an explicit assignment. A pending take
  // must not overwrite it just because its numerical slot stayed unchanged.
  if(c.type==CommandType::SetSample || c.type==CommandType::RestoreSampleAssignment || t.instrument!=old_instrument || t.sample_slot!=old_slot)++assignment_revision_[c.track];
 }
}
