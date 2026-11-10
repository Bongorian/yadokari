// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "performance.h"
namespace groove {
auto Performance::navigation_key()const -> uint32_t{
  const unsigned sub=mode_==UiMode::Perform?performance_page_:mode_==UiMode::Synth?instrument_view_:mode_==UiMode::Effects?fx_page_:mode_==UiMode::Generate?gen_page_:mode_==UiMode::Sampler?sampler_page_:mode_==UiMode::Diagnostics?0:mode_==UiMode::Modulation?modulation_page_:mode_==UiMode::Settings?settings_page_:mode_==UiMode::Mixer?page_:mode_==UiMode::Step?(tracker_view_?2:step_overview_?1:0)+(note_edit_?4:0):mode_==UiMode::Clip?(clip_follow_page_?1:0):mode_==UiMode::Routing?(routing_fx_page_?1:0)+(group_send_page_?2:0):0;
  return (unsigned(mode_)<<16)|sub|((mode_==UiMode::Synth && source_select_)?0x100:0);
 }
auto Performance::navigation()const -> NavigationLocation{
  NavigationLocation n;n.mode=uint8_t(mode_);n.focus=axes_.focus;n.key=navigation_key();
  const uint16_t values[]={selected_,selected_kit_pad_,selected_step_,page_,instrument_view_,fx_page_,performance_page_,gen_page_,sampler_page_,0,modulation_page_,settings_page_,settings_cursor_,group_cursor_,group_fx_slot_,uint16_t(group_send_page_),uint16_t(routing_fx_page_),gen_lane_,gen_cell_,slice_cursor_,audio_clip_cursor_,macro_index_,macro_route_,automation_lane_,automation_cell_,midi_cc_cursor_,nrpn_slot_,scene_slot_,song_cursor_,sound_slot_,uint16_t(tracker_view_),uint16_t(step_overview_),uint16_t(zone_vel_),uint16_t(clip_follow_page_),uint16_t(encoder_edit_),uint16_t(follow_display_),uint16_t(browser_.cursor),layer_cursor_,layer_pool_cursor_,midi_input_cursor_,project_.active_pattern,uint16_t(following_clip_),chain_node_,fx_focus_slot_,uint16_t(source_select_),source_cursor_,uint16_t(browser_.favorites_only),performance_fx_bank_,uint16_t(note_edit_),uint16_t(browser_.library_kits),uint16_t(browser_.library_instrument),uint16_t(browser_.library_originals),uint16_t(browser_.library_offset),browser_.library_return};
  for(unsigned i=0;i<54;++i)n.value[i]=values[i];
  copy_text(n.directory,sizeof n.directory,browser_.directory);return n;
 }
auto Performance::restore_navigation(const NavigationLocation& n) -> void{selected_=uint8_t(n.value[0]);apply_mode(UiMode(n.mode));if(mode_!=UiMode(n.mode))return;const auto* v=n.value;
  selected_=uint8_t(v[0]);selected_kit_pad_=uint8_t(v[1]);selected_step_=uint8_t(v[2]);page_=uint8_t(v[3]);instrument_view_=uint8_t(v[4]);fx_page_=uint8_t(v[5]);performance_page_=uint8_t(v[6]);gen_page_=uint8_t(v[7]);sampler_page_=uint8_t(v[8]);modulation_page_=uint8_t(v[10]);settings_page_=uint8_t(v[11]);settings_cursor_=uint8_t(v[12]);group_cursor_=uint8_t(v[13]);group_fx_slot_=uint8_t(v[14]);group_send_page_=v[15];routing_fx_page_=v[16];gen_lane_=uint8_t(v[17]);gen_cell_=uint8_t(v[18]);slice_cursor_=uint8_t(v[19]);audio_clip_cursor_=uint8_t(v[20]);macro_index_=uint8_t(v[21]);macro_route_=uint8_t(v[22]);automation_lane_=uint8_t(v[23]);automation_cell_=uint8_t(v[24]);midi_cc_cursor_=uint8_t(v[25]);nrpn_slot_=uint8_t(v[26]);scene_slot_=uint8_t(v[27]);song_cursor_=uint8_t(v[28]);sound_slot_=uint8_t(v[29]);tracker_view_=v[30];step_overview_=v[31];zone_vel_=v[32];clip_follow_page_=v[33];encoder_edit_=v[34];follow_display_=v[35];browser_.cursor=v[36];layer_cursor_=uint8_t(v[37]);layer_pool_cursor_=v[38];midi_input_cursor_=uint8_t(v[39]);axes_.focus=n.focus;following_clip_=v[41];project_.active_pattern=following_clip_?project_.clips[selected_].pattern:uint8_t(v[40]);chain_node_=uint8_t(v[42]);fx_focus_slot_=uint8_t(v[43]);source_select_=v[44];source_cursor_=uint8_t(v[45]<instrument_count()?v[45]:0);browser_.favorites_only=v[46];performance_fx_bank_=uint8_t(v[47]);note_edit_=v[48];if(mode_==UiMode::Library){browser_.library_kits=v[49];browser_.library_instrument=Instrument(v[50]);browser_.library_originals=v[51];browser_.library_offset=v[52];browser_.library_return=uint8_t(v[53]);browser_.library_list=true;browser_refresh_pending_=true;}else if(source_select_){browser_.library_instrument=Instrument(source_cursor_);browser_.library_kits=v[49];browser_.library_originals=v[51];browser_.library_offset=v[52];browser_.library_list=true;browser_.library_resume=true;browser_refresh_pending_=true;}if(__builtin_strcmp(browser_.directory,n.directory)){copy_text(browser_.directory,sizeof browser_.directory,n.directory);browser_.count=0;browser_refresh_pending_=true;}refresh_steps();dirty_=true;
 }
auto Performance::apply_mode(UiMode mode)  -> void{if(browser_editing() && mode!=mode_)return; if(mode!=mode_){if(mode_==UiMode::Library || (mode_==UiMode::Synth && source_select_)){if(file_action_==FileAction::LibraryOpen || file_action_==FileAction::LibraryOpenKits || file_action_==FileAction::LibraryMove || file_action_==FileAction::LibraryKind || file_action_==FileAction::LibraryOriginals || file_action_==FileAction::LibraryInspect || file_action_==FileAction::LibraryRestore || file_action_==FileAction::LibraryEngine)file_action_=FileAction::None;browser_.library_list=false;browser_.library_resume=false;browser_.count=0;browser_refresh_pending_=false;}step_chord_edit_=false;encoder_edit_=false;loop_confirm_=false;sn_ref_armed_=false;extend_pending_=false;source_select_=false;source_arm_unknown_=false;if(mode!=UiMode::Step)note_edit_=false;if(mode!=UiMode::Settings)update_confirm_=false;song_import_armed_=song_clear_armed_=false;stretch_modal_=0;freeze_modal_=0;if(mode_==UiMode::Perform && performance_fx_mode_!=PerformanceFx::Off){release_performance_fx_=true;retry_releases();}}if(mode==UiMode::Browser){browser_.library_list=false;if(!browser_.count)browser_refresh_pending_=true;}if(mode==UiMode::Library){browser_.library_list=true;browser_refresh_pending_=true;}if(mode==UiMode::Perform && mode_!=UiMode::Perform)capture_snapshot(reload_point_);if(mode!=UiMode::Browser)browser_.new_project_list=false;mode_=mode; switch(mode){case UiMode::Pad:axes_.pad_mode=PadMode::Play;break;case UiMode::Step:axes_.pad_mode=PadMode::Step;break;case UiMode::Clip:axes_.pad_mode=PadMode::Clip;break;case UiMode::Scene:axes_.pad_mode=PadMode::Scene;break;case UiMode::Keyboard:axes_.pad_mode=PadMode::Keyboard;break;case UiMode::Chord:axes_.pad_mode=PadMode::Chords;break;default:break;} dirty_=true; }
auto Performance::page_back() -> bool{if(modal_active())return false;save_navigation();NavigationLocation location;if(!axes_.history.back(location))return false;restore_navigation(location);axes_.history.replace_current(navigation());return true;}
auto Performance::page_forward() -> bool{if(modal_active())return false;save_navigation();NavigationLocation location;if(!axes_.history.forward(location))return false;restore_navigation(location);axes_.history.replace_current(navigation());return true;}
auto Performance::page_button_edge(bool right,bool down)  -> void{
  bool& self_down=right?page_right_down_:page_left_down_;
  bool& other_down=right?page_left_down_:page_right_down_;
  bool& self_pending=right?page_right_pending_:page_left_pending_;
  bool& other_pending=right?page_left_pending_:page_right_pending_;
  uint64_t& self_since=right?page_right_since_:page_left_since_;
  if(down) {
   self_down=true;
   if(other_down) { other_pending=false; self_pending=false; page_both_since_=now_ms_; }
   else { self_since=now_ms_; self_pending=true; }
  } else {
   self_down=false;
   if(self_pending) { self_pending=false; if(right)page_forward(); else page_back(); }
   if(!page_left_down_ && !page_right_down_) page_home_fired_=false;
  }
 }
auto Performance::service_page_buttons()  -> void{
  if(page_left_down_ && page_right_down_) {
   if(!page_home_fired_ && now_ms_-page_both_since_>=kPageHomeMs) { page_home_fired_=true; home_or_return(); }
   return;
  }
  if(page_left_pending_ && now_ms_-page_left_since_>=kPageNavGraceMs) { page_left_pending_=false; page_back(); }
  if(page_right_pending_ && now_ms_-page_right_since_>=kPageNavGraceMs) { page_right_pending_=false; page_forward(); }
 }
auto Performance::track_shortcut(unsigned slot,bool down) -> bool{
  if(slot>=8)return false;
  if(!down){if(track_peek_slot_==slot){if(track_peek_active_){restore_navigation(track_peek_location_);}track_peek_slot_=255;track_peek_active_=false;dirty_=true;}return true;}
  bool group=mode_==UiMode::Routing && routing_fx_page_;
  if(group){group_cursor_=uint8_t(slot);axes_.focus=Focus::Group;dirty_=true;return true;}
  if(shifted())track_bank_^=1;
  unsigned target=slot+8*track_bank_;
  if(mute_modifier())return mute_or_solo(target,false,shifted());
  if(solo_modifier())return mute_or_solo(target,true);
  if(controls_[4]&0x08){if(!select_track(target))return false;set_mode(UiMode::Clip);return true;}
  if(!select_track(target))return false;
  track_peek_location_=navigation();track_peek_slot_=uint8_t(slot);track_peek_since_=now_ms_;track_peek_active_=false;return true;
 }
auto Performance::service_track_shortcut() -> void{if(modal_active())return;if(track_peek_slot_<8 && !(controls_[1]&(1u<<track_peek_slot_))){track_shortcut(track_peek_slot_,false);return;}if(track_peek_slot_<8 && !track_peek_active_ && now_ms_-track_peek_since_>=kPageHomeMs){track_peek_active_=true;apply_mode(UiMode::Mixer);}}
auto Performance::held_model_target(unsigned index)const -> ParameterTarget{int local=engine_model_parameter(track_const(selected_).instrument);return index==1 && step_hold_edit() && !shifted() && local>=0?engine_target(track_const(selected_).instrument,unsigned(local)):0;}
auto Performance::audio_key(unsigned index) -> bool{const auto d=audio_button(index,audio_lane().kind,shifted(),live_recording_ || recording_file_busy_ || file_action_pending(),audio_undo_valid_[selected_],live_recording_);if(!d.enabled)return false;auto a=audio_lane();
  switch(d.action){
  case AudioAction::Instrument:a.kind=TrackKind::Instrument;break;
  case AudioAction::Looper:a.kind=TrackKind::Looper;break;
  case AudioAction::Audio:a.kind=TrackKind::Audio;break;
  case AudioAction::Song:set_mode(UiMode::Song);return true;
  case AudioAction::Record:return request_file(FileAction::AudioRecord);
  case AudioAction::Overdub:return request_file(FileAction::AudioOverdub);
  case AudioAction::Toggle:a.enabled=!a.enabled;break;
  case AudioAction::Delete:return confirm_erase(UiErase::AudioDelete);
  case AudioAction::Clear:return confirm_erase(UiErase::AudioClear);
  case AudioAction::Duplicate:{const auto source=a.clips[audio_clip_cursor_];if(source.asset>=kSampleAssets)return false;unsigned n=0;while(n<kAudioClips && a.clips[n].asset<kSampleAssets)++n;if(n==kAudioClips)return false;a.clips[n]=source;unsigned rate=project_.assets[source.asset].metadata.rate;if(!rate)return false;uint64_t ticks=(uint64_t(source.frames)*project_.tempo*960+uint64_t(rate)*60-1)/(uint64_t(rate)*60);a.clips[n].tick+=uint32_t(((ticks+3839)/3840)*3840);audio_clip_cursor_=uint8_t(n);break;}
  case AudioAction::Undo:return undo_audio();
  case AudioAction::Back:performance_page_=0;dirty_=true;return true;
  default:return false;
  }return set_audio_lane(selected_,a);
 }
auto Performance::common_top_key(unsigned index) -> bool{const auto d=common_top(index);if(!d.enabled)return false;switch(d.action){case CommonTopAction::TrackName:return begin_track_rename();case CommonTopAction::Source:return toggle_source_select();case CommonTopAction::Modulation:set_mode(UiMode::Modulation);return true;case CommonTopAction::Store:return store_sound(sound_slot_);case CommonTopAction::Recall:return recall_sound(sound_slot_);case CommonTopAction::Audio:set_mode(UiMode::Perform);performance_page_=5;return true;default:return false;}}
auto Performance::page_title()const -> const char*{if(mode_==UiMode::Browser && browser_.new_project_list)return "NEW PROJECT";if(mode_==UiMode::Library)return browser_.library_kits?"KITS":"PRESETS";if(source_select_)return "ENGINE";if(kit_edit_page())return "KIT PADS";if(mode_==UiMode::Perform){const char* names[]={"A/B MORPH","MACROS","GESTURE","MUTATION","LIVE FX","AUDIO"};return names[performance_page_];}return mode_name(mode_);}
auto Performance::input_key(unsigned i) -> bool{auto d=input_button(i,mode_==UiMode::Chord,shifted(),history_cursor_>0,history_cursor_<history_size_,track_const(selected_).instrument==Instrument::DrumKit);if(!d.enabled)return false;switch(d.action){
 case InputAction::ScalePrev:return input_scale(-1);case InputAction::ScaleNext:return input_scale(1);case InputAction::OctaveDown:return input_octave(-1);case InputAction::OctaveUp:return input_octave(1);
 case InputAction::ShapePrev:return input_shape(-1,false);case InputAction::ShapeNext:return input_shape(1,false);case InputAction::InvertPrev:return input_shape(-1,true);case InputAction::InvertNext:return input_shape(1,true);
 case InputAction::ChordEdit:if(step_chord_edit_){step_chord_edit_=false;set_mode(step_chord_origin_);}else{step_chord_origin_=mode_;set_mode(UiMode::Keyboard);step_chord_edit_=true;}dirty_=true;return true;
 case InputAction::Sounds:return open_preset_library();case InputAction::PreviousSound:return request_file(FileAction::LibraryPrev);case InputAction::NextSound:return request_file(FileAction::LibraryNext);
 case InputAction::Undo:return undo();case InputAction::Redo:return redo();default:return false;}}
auto Performance::soft_selected(unsigned i)const -> bool{if(transfer_view_.active)return !transfer_view_.usb && i<2 && (i==1)==transfer_view_.station;if(library_catalog_controls())return library_catalog_button(i).selected;if(input_top(i))return i==5 && step_chord_editor() && !shifted();if(source_select_ && !modal_active())return false;unsigned slot=top_slot(i);return slot<8 && page_top_selected(slot);}
auto Performance::soft_key(unsigned i) -> bool{if(transfer_view_.active){if(i==7)transfer_close_=true;else if(i==5){transfer_browse_=true;transfer_close_=true;}else if(!transfer_view_.started && !transfer_view_.failed && i<2)transfer_view_.station=i==1;else if(!transfer_view_.started && !transfer_view_.failed && i==4)transfer_start_=true;else return false;dirty_=true;return true;}NavigationScope navigation(*this);if(library_catalog_controls())return library_catalog_key(i);if(input_top(i))return input_key(i);if(source_select_ && !modal_active()){if(i==5 && shifted())return begin_track_rename();if(i==4)return source_confirm();if(i==7)return toggle_source_select();return false;}unsigned slot=top_slot(i);return slot<8 && !((source_select_ || kit_edit_page()) && i>=6) && page_top_key(slot);}
auto Performance::page_top_presentation(unsigned index)const  -> ui::ControlPresentation{if(composition_modal_){if(index>=2)return ui::ControlDescriptor<ModalAction>{};return ui::ControlDescriptor<ModalAction>{modal_label(index),index==0?ModalAction::Accept:ModalAction::Cancel,true};}if(twin_modal_==1){if(index<2)return ui::ControlDescriptor<ModalAction>{modal_label(index),index?ModalAction::Cancel:ModalAction::Accept,index!=0 || twin_config_.valid()};return ui::ControlDescriptor<ModalAction>{};}if(common_top(index).action!=CommonTopAction::None)return common_top(index);if(!browser_editing() && modal_active())return modal_button(index,modal_progress());if(mode_==UiMode::Modulation && modulation_page_==3 && index==4)return modulation_button_control(index,modulation_controls());if(index==7 && bake_control())return ui::ControlDescriptor<SynthAction>{bake_next_label(),SynthAction::BakePreset,true};if(audio_page())return audio_button(index,audio_lane().kind,shifted(),live_recording_ || recording_file_busy_ || file_action_pending(),audio_undo_valid_[selected_],live_recording_);if(browser_editing())return browser_edit_button_control(index,browser_edit_controls());if(mode_==UiMode::Song)return song_button_control(index,song_controls());
  if(!repeat_mode_ && mode_==UiMode::Generate && gen_page_==0 && shifted() && (index==4 || index==5))return generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_);
  if(mode_==UiMode::Chord && !repeat_mode_)return input_button(index,true,shifted(),history_cursor_>0,history_cursor_<history_size_,track_const(selected_).instrument==Instrument::DrumKit);
  if(repeat_mode_)return repeat_button_control(index,repeat_rate_);
  if(mode_==UiMode::Synth)return synth_button_control(index,synth_controls());
  if(mode_==UiMode::Effects)return effects_button_control(index,effects_controls());
  if(mode_==UiMode::Browser)return browser_button_control(index,browser_controls());
  if(mode_==UiMode::Mixer)return mixer_button(index,track_const(selected_).frozen_asset<kSampleAssets,page_,history_cursor_,history_size_);
  if(mode_==UiMode::Perform)return perform_button_control(index,perform_controls());
  if(mode_==UiMode::Sampler)return sampler_button_control(index,sampler_controls());
  if(mode_==UiMode::Routing && !routing_fx_page_)return recording_button_control(index,recording_controls());
  if(mode_==UiMode::Routing)return routing_fx_button_control(index,routing_fx_controls());
  if(mode_==UiMode::Diagnostics)return diagnostics_button(index);
  if(mode_==UiMode::Step)return step_button_control(index,step_controls());
  if(mode_==UiMode::Modulation)return modulation_button_control(index,modulation_controls());
  if(mode_==UiMode::Clip)return clip_button_control(index,clip_controls());
  if(mode_==UiMode::Scene)return scene_button(index,launch_quantized_,following_clip_,project_.chain.enabled,history_cursor_,history_size_,shifted());
  if(mode_==UiMode::Project){return project_button(index,shifted(),pending_project_!=ProjectAction::None,midi_capture_state_!=0,history_cursor_,history_size_);}
if(mode_==UiMode::Settings){return settings_button(index,update_confirm_,settings_page_);}if(mode_==UiMode::Generate)return generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_);return default_button(index,page_,history_cursor_,history_size_);}
auto Performance::page_top_enabled(unsigned index)const -> bool{if(common_top(index).action!=CommonTopAction::None)return common_top(index).enabled;if(!browser_editing() && modal_active())return modal_label(index)[0]!=0;if(mode_==UiMode::Modulation && modulation_page_==3 && index==4)return modulation_button_control(index,modulation_controls()).enabled;if(index==7 && bake_control())return !recording_configuration_locked(recording_controls()) || shifted();if(audio_page())return audio_button(index,audio_lane().kind,shifted(),live_recording_ || recording_file_busy_ || file_action_pending(),audio_undo_valid_[selected_],live_recording_).enabled;if(browser_editing())return browser_edit_button_control(index,browser_edit_controls()).enabled;if(mode_==UiMode::Song)return song_button_control(index,song_controls()).enabled;if(repeat_mode_)return repeat_button_control(index,repeat_rate_).enabled;if(mode_==UiMode::Synth)return synth_button_control(index,synth_controls()).enabled;if(mode_==UiMode::Step)return step_button_control(index,step_controls()).enabled;if(mode_==UiMode::Chord)return input_button(index,true,shifted(),history_cursor_>0,history_cursor_<history_size_,track_const(selected_).instrument==Instrument::DrumKit).enabled;if(mode_==UiMode::Mixer)return mixer_button(index,track_const(selected_).frozen_asset<kSampleAssets,page_,history_cursor_,history_size_).enabled;if(mode_==UiMode::Effects)return effects_button_control(index,effects_controls()).enabled;if(mode_==UiMode::Browser)return browser_button_control(index,browser_controls()).enabled;if(mode_==UiMode::Diagnostics)return diagnostics_button(index).enabled;if(mode_==UiMode::Sampler)return sampler_button_control(index,sampler_controls()).enabled;if(mode_==UiMode::Routing){if(!routing_fx_page_)return recording_button_control(index,recording_controls()).enabled;return routing_fx_button_control(index,routing_fx_controls()).enabled;}if(mode_==UiMode::Project)return project_button(index,shifted(),pending_project_!=ProjectAction::None,midi_capture_state_!=0,history_cursor_,history_size_).enabled;if(mode_==UiMode::Perform)return perform_button_control(index,perform_controls()).enabled;if(mode_==UiMode::Modulation)return modulation_button_control(index,modulation_controls()).enabled;if(mode_==UiMode::Clip)return clip_button_control(index,clip_controls()).enabled;if(mode_==UiMode::Scene)return scene_button(index,launch_quantized_,following_clip_,project_.chain.enabled,history_cursor_,history_size_,shifted()).enabled;if(mode_==UiMode::Settings)return settings_button(index,update_confirm_,settings_page_).enabled;if(mode_==UiMode::Generate)return generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_).enabled;return default_button(index,page_,history_cursor_,history_size_).enabled;}
auto Performance::page_top_selected(unsigned index)const -> bool{if(mode_==UiMode::Mixer && index==1)return twin_status()==TwinStatus::Active;if(common_top(index).action!=CommonTopAction::None)return common_top(index).selected;if(!browser_editing() && modal_active())return false;if(mode_==UiMode::Modulation && modulation_page_==3 && index==4)return modulation_button_control(index,modulation_controls()).selected;if(audio_page())return index==unsigned(audio_lane().kind)-1;if(mode_==UiMode::Effects && fx_page_==5 && index>=3 && index<=6)return effects_button_control(index,effects_controls()).selected;if(mode_==UiMode::Clip && index==4)return clip_follow_page_;if(mode_==UiMode::Scene && index==4)return scene_button(index,launch_quantized_,following_clip_,project_.chain.enabled,history_cursor_,history_size_,shifted()).selected;if(mode_==UiMode::Synth && source_select_)return index==4;if(mode_==UiMode::Synth && index==7)return wave_profile();if(mode_==UiMode::Chord && !repeat_mode_)return false;if(repeat_mode_)return repeat_button_control(index,repeat_rate_).selected;if(mode_==UiMode::Effects)return effects_button_control(index,effects_controls()).selected;if(mode_==UiMode::Step)return step_button_control(index,step_controls()).selected;if(mode_==UiMode::Routing && !routing_fx_page_)return recording_button_selected(index,recording_controls(),project_.routing.monitor>0);if(mode_==UiMode::Routing)return routing_fx_button_control(index,routing_fx_controls()).selected;if(mode_==UiMode::Diagnostics)return diagnostics_button(index).selected;if(mode_==UiMode::Sampler)return sampler_button_control(index,sampler_controls()).selected;if(mode_==UiMode::Browser)return browser_button_control(index,browser_controls()).selected;if(mode_==UiMode::Modulation)return modulation_button_control(index,modulation_controls()).selected;if(mode_==UiMode::Perform)return perform_button_control(index,perform_controls()).selected;if(mode_==UiMode::Clip && index<4)return false;if(mode_==UiMode::Scene && index<4)return scene_button(index,launch_quantized_,following_clip_,project_.chain.enabled,history_cursor_,history_size_,shifted()).selected;if(mode_==UiMode::Project)return project_button(index,shifted(),pending_project_!=ProjectAction::None,midi_capture_state_!=0,history_cursor_,history_size_).selected;if(mode_==UiMode::Synth && !wave_profile())return index<4 && index==instrument_view_;if(mode_==UiMode::Settings)return settings_button(index,update_confirm_,settings_page_).selected;if(mode_==UiMode::Generate)return generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_).selected;if(wave_profile() && mode_==UiMode::Synth)return index<2 && index==wave_clock();if(mode_==UiMode::Mixer)return mixer_button(index,track_const(selected_).frozen_asset<kSampleAssets,page_,history_cursor_,history_size_).selected;return default_button(index,page_,history_cursor_,history_size_).selected;}
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("Os"),noinline))
#endif
auto Performance::page_top_key(unsigned index) -> bool{NavigationScope navigation_scope(*this);if(common_top(index).action!=CommonTopAction::None)return common_top_key(index);if(!browser_editing() && modal_active())return modal_key(index);if(index==7 && bake_control())return bake_next();if(audio_page())return audio_key(index);if(mode_==UiMode::Mixer && index>=2 && index<4)return false;if(browser_editing())return browser_edit_key(index);if(freeze_modal_){if(index==0 || index==1)return request_file(FileAction::CancelFreeze);return true;}if(stretch_modal_){if(stretch_modal_==1){if(index==0)return request_file(FileAction::BeginStretch);if(index==1){stretch_modal_=0;dirty_=true;return true;}return true;}if(index==0 || index==1)return request_file(FileAction::CancelStretch);return true;}if(!page_top_enabled(index))return false;if(mode_==UiMode::Modulation && modulation_button_control(index,modulation_controls()).action==ModulationAction::Multitimbral)return set_multitimbral(!multitimbral());encoder_edit_=false;if(mode_==UiMode::Song){switch(song_button_control(index,song_controls()).action){
 case SongAction::Record:return scene_take_toggle();
 case SongAction::Cancel:return scene_take_cancel();
 case SongAction::Add:return song_add();
 case SongAction::Delete:return confirm_erase(UiErase::SongDelete);
 case SongAction::Import:if(song_import_armed_){song_import_armed_=false;return song_materialize();}song_import_armed_=true;dirty_=true;return true;
 case SongAction::Skeleton:return song_skeleton();
 case SongAction::Clear:if(song_clear_armed_){song_clear_armed_=false;return song_clear();}song_clear_armed_=true;dirty_=true;return true;
 default:return false;
 }}if(repeat_mode_)return repeat_button_control(index,repeat_rate_).action==RepeatAction::Rate?note_repeat(repeat_mode_,index):false;if(mode_==UiMode::Synth)return synth_key(index);if(mode_==UiMode::Chord)return input_key(index);if(mode_==UiMode::Effects){switch(effects_button_control(index,effects_controls()).action){
   case EffectsAction::ChainInserts:fx_page_=3;dirty_=true;return true;
   case EffectsAction::SwapInserts:return swap_track_inserts();
   case EffectsAction::ChainBypass:return chain_toggle_bypass();
   case EffectsAction::ChainEdit:return chain_edit();
   case EffectsAction::PadInsert:fx_page_=4;dirty_=true;return true;
   case EffectsAction::Undo:return undo();
   case EffectsAction::Page:fx_page_=uint8_t(index);dirty_=true;return true;
   case EffectsAction::ChainToggle:fx_page_=fx_page_==3?5:3;dirty_=true;return true;
   case EffectsAction::SendDelay:case EffectsAction::SendReverb:{Command after{CommandType::SetTrackSendMode,selected_,0,uint16_t(track_const(selected_).send_prefader^(1u<<(index-3)))},before=after;before.value=track_const(selected_).send_prefader;return edit(after,before);}
   default:break;
  }}
  if(mode_==UiMode::Generate && gen_page_==5 && index==4)return harmony_generate();
  if(mode_==UiMode::Generate && gen_page_==2 && shifted() && index==5)return apply_groove_all();
  if(mode_==UiMode::Generate){auto ga=generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_).action;if(ga==GenerateAction::RotateLeft)return rotate_track(-1);if(ga==GenerateAction::RotateRight)return rotate_track(1);}
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::CopyStep)return copy_step();
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::PasteStep)return paste_step();
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::ClickMain)return set_click_setting(4);
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::CountIn)return set_click_setting(2);
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::NoteReplace){if(track_const(selected_).instrument==Instrument::DrumKit)return false;Command after{CommandType::SetNoteReplace,0,0,uint16_t(!project_.ui.note_replace)},before=after;before.value=project_.ui.note_replace;return edit(after,before);}
  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::NoteQuantize){Command after{CommandType::SetNoteInputQuantize,0,0,uint16_t(!project_.ui.note_quantized)},before=after;before.value=project_.ui.note_quantized;return edit(after,before);}
  if(mode_==UiMode::Browser){switch(browser_button_control(index,browser_controls()).action){
   case BrowserAction::Rename:return begin_browser_edit(BrowserOperation::Rename);
   case BrowserAction::Delete:return begin_browser_edit(BrowserOperation::Delete);
   case BrowserAction::NewFolder:return begin_browser_edit(BrowserOperation::NewFolder);
   case BrowserAction::Open:return request_file(FileAction::Open);
   case BrowserAction::Up:return request_file(FileAction::Up);
   case BrowserAction::Preview:return request_file(FileAction::Preview);
   case BrowserAction::StopPreview:return request_file(FileAction::StopPreview);
   case BrowserAction::Load:return request_file(FileAction::Load);
   case BrowserAction::MidiImport:return request_file(FileAction::MidiImport);
   case BrowserAction::Favorite:return request_file(FileAction::Favorite);
   case BrowserAction::Duplicate:return request_file(FileAction::Duplicate);
   case BrowserAction::ToggleFavorites:return request_file(FileAction::ToggleFavorites);
   case BrowserAction::SaveKit:return request_file(FileAction::SaveKit);
   case BrowserAction::Project:set_mode(UiMode::Project);return true;
   case BrowserAction::Factory:return request_file(FileAction::Factory);
   default:return false;
  }}
  if(mode_==UiMode::Sampler && shifted() && index>=4 && index<=6){switch(sampler_recording_control(index-4,recording_controls(),sample_auditioning()).action){
   case RecordingAction::RecordInput:return request_file(FileAction::SampleRecordToggle);
   case RecordingAction::RecordSource:case RecordingAction::Stop:return request_file(FileAction::RecordToggle);
   case RecordingAction::Cancel:return request_file(FileAction::CancelResample);
   case RecordingAction::Preview:return request_file(FileAction::AuditionSelectedSample);
   default:return false;
  }}
  if(mode_==UiMode::Sampler){switch(sampler_button_control(index,sampler_controls()).action){
   case SamplerAction::Page:sampler_page_=uint8_t(index==3?sampler_button_control(index,sampler_controls()).minimum:index);return true;
   case SamplerAction::Chop:if(sampler_page_==3 && !shifted()){stretch_modal_=1;stretch_format();dirty_=true;return true;}return confirm_erase(UiErase::AutoChop);
   case SamplerAction::MapPads:return confirm_erase(UiErase::MapPads);
   case SamplerAction::ChopSteps:return confirm_erase(UiErase::ChopSteps);
   case SamplerAction::ClearZones:return confirm_erase(UiErase::ZonesClear);
   case SamplerAction::FileLoop:return confirm_erase(UiErase::FileLoop);
   case SamplerAction::LoopTake:return loop_take();
   case SamplerAction::OpenBrowser:loop_confirm_=false;set_mode(UiMode::Browser);return true;
   default:break;
  }}
  if(mode_==UiMode::Diagnostics){switch(diagnostics_button(index).action){

   case DiagnosticsAction::Test:diagnostic_test_=uint8_t(index-3);return true;
   case DiagnosticsAction::Tone:diagnostic_test_=uint8_t(index-3);if(!diagnostics_.audio_active){notify(UiNotice::Failed);return false;}return send({CommandType::DiagnosticTone,0,0,1});
   case DiagnosticsAction::Exit:diagnostic_test_=0;set_mode(UiMode::Settings);return true;
   default:return false;
  }}
  if(mode_==UiMode::Routing && !routing_fx_page_){switch(recording_button_control(index,recording_controls()).action){
   case RecordingAction::GroupFx:routing_fx_page_=true;dirty_=true;return true;
   case RecordingAction::Mixer:set_mode(UiMode::Mixer);return true;
   case RecordingAction::Monitor:{Command after{CommandType::SetRouting,0,0,0},before=after;before.routing=project_.routing;after.routing=project_.routing;after.routing.monitor=after.routing.monitor>0?0:0.25f;return edit(after,before);}
   case RecordingAction::Capture:return request_file(FileAction::Capture);
   case RecordingAction::Record:case RecordingAction::Stop:return request_file(FileAction::RecordToggle);
   case RecordingAction::Resample:return request_file(FileAction::Resample);
   case RecordingAction::Cancel:return request_file(FileAction::CancelResample);
   case RecordingAction::Bars:{const auto d=recording_button_control(index,recording_controls());resample_bars_=uint8_t(resample_bars_>=d.maximum?d.minimum:d.adjusted(resample_bars_,1));dirty_=true;return true;}
   case RecordingAction::Quantize:record_quantized_=!record_quantized_;dirty_=true;return true;
   case RecordingAction::Overdub:return request_file(FileAction::Overdub);
   case RecordingAction::Undo:return undo();
   case RecordingAction::Redo:return redo();
   default:return false;
  }}
  if(mode_==UiMode::Routing && routing_fx_page_){switch(routing_fx_button_control(index,routing_fx_controls()).action){
   case RoutingFxAction::ExitFx:routing_fx_page_=false;dirty_=true;return true;
   case RoutingFxAction::ToggleSlot:group_fx_slot_^=1;dirty_=true;return true;
   case RoutingFxAction::SwapInserts:return swap_group_inserts();
   case RoutingFxAction::GroupPage:group_send_page_=!group_send_page_;dirty_=true;return true;
   case RoutingFxAction::RecordToggle:return request_file(FileAction::RecordToggle);
   case RoutingFxAction::Overdub:if(live_recording_)return false;return request_file(FileAction::Overdub);
   case RoutingFxAction::Quantize:if(live_recording_)return false;record_quantized_=!record_quantized_;dirty_=true;return true;
   case RoutingFxAction::Mixer:set_mode(UiMode::Mixer);return true;
   case RoutingFxAction::Monitor:{Command after{CommandType::SetRouting,0,0,0},before=after;before.routing=project_.routing;after.routing=project_.routing;after.routing.monitor=after.routing.monitor>0?0:0.25f;return edit(after,before);}
   default:break;
  }}

  if(mode_==UiMode::Step && step_button_control(index,step_controls()).action==StepAction::ViewCycle){if(tracker_view_){tracker_view_=false;step_overview_=false;}else if(step_overview_){step_overview_=false;tracker_view_=true;follow_display_=true;}else step_overview_=true;dirty_=true;return true;}
  if(mode_==UiMode::Mixer && index==1)return twin_button();
  if(mode_==UiMode::Mixer && mixer_button(index,track_const(selected_).frozen_asset<kSampleAssets,page_,history_cursor_,history_size_).action==MixerAction::Freeze){if(track_const(selected_).frozen_asset<kSampleAssets)return set_frozen_asset(selected_,project_.active_pattern,255);return request_file(FileAction::BeginFreeze);}if(mode_==UiMode::Perform){switch(perform_button_control(index,perform_controls()).action){
   case PerformAction::FxQuantize:performance_fx_quantize_=uint8_t((performance_fx_quantize_+1)%3);dirty_=true;return true;
   case PerformAction::FxBank:performance_fx_bank_^=1;dirty_=true;return true;
   case PerformAction::FxTail:{bool previous=performance_fx_tail_;performance_fx_tail_=!previous;if(!set_performance_fx(performance_fx_mode_,performance_fx_amount_,performance_fx_latched_)){performance_fx_tail_=previous;return false;}return true;}
   case PerformAction::GotoMutation:if(performance_page_==4 && performance_fx_mode_!=PerformanceFx::Off){release_performance_fx_=true;retry_releases();}performance_page_=3;dirty_=true;return true;
   case PerformAction::MutationGenerate:return mutation_generate();
   case PerformAction::MutationFreeze:return mutation_freeze();
   case PerformAction::MutationCommit:return mutation_commit();
   case PerformAction::MutationRevert:return mutation_set(track_const(selected_).mutation.seed,0,false);
   case PerformAction::Back:performance_page_=0;return true;
   case PerformAction::PagePrev:step_page((page_+15)%16);return true;
   case PerformAction::PageNext:step_page((page_+1)%16);return true;
   case PerformAction::LiveFx:performance_page_=4;dirty_=true;return true;
   case PerformAction::Undo:return undo();
   case PerformAction::Redo:return redo();
   case PerformAction::GotoGesture:if(performance_page_==4 && performance_fx_mode_!=PerformanceFx::Off){release_performance_fx_=true;retry_releases();}performance_page_=2;dirty_=true;return true;
   case PerformAction::CaptureRetro:return capture_retro();
   case PerformAction::GestureRecord:return gesture_record(!gesture_recording_);
   case PerformAction::GesturePlay:return gesture_play(!gesture_playing_);
   case PerformAction::GestureClear:return confirm_erase(UiErase::GestureClear);
   case PerformAction::Dice:return dice();
   case PerformAction::PageMorph:case PerformAction::PageMacros:if(performance_page_==4 && performance_fx_mode_!=PerformanceFx::Off){release_performance_fx_=true;retry_releases();}performance_page_=uint8_t(index);if(index==1 && control_all_)return set_control_all(false);return true;
   case PerformAction::StoreA:return store_morph(0);
   case PerformAction::StoreB:return store_morph(1);
   case PerformAction::ResetLive:return reset_live();
   case PerformAction::ControlAll:return set_control_all(!control_all_);
   default:break;
  }}
  if(mode_==UiMode::Modulation){auto ma=modulation_button_control(index,modulation_controls()).action;if(ma==ModulationAction::AutoPagePrev || ma==ModulationAction::AutoPageNext){automation_cell_=uint8_t(bounded(int(automation_cell_)+(ma==ModulationAction::AutoPagePrev?-16:16),0,int(kAutomationSteps)-1));dirty_=true;return true;}}
  if(mode_==UiMode::Modulation && modulation_button_control(index,modulation_controls()).action==ModulationAction::Page){modulation_page_=uint8_t(modulation_button_control(index,modulation_controls()).minimum);dirty_=true;return true;}
  if(mode_==UiMode::Clip){switch(clip_button_control(index,clip_controls()).action){
   case ClipControlAction::Variation:return begin_variation();
   case ClipControlAction::Copy:return copy_pattern();
   case ClipControlAction::Double:return extend_track();
   case ClipControlAction::Paste:return paste_pattern();
   case ClipControlAction::Launch:return launch_clip(selected_,project_.active_pattern,!shifted() && launch_quantized_);
   case ClipControlAction::Stop:return stop_clip(selected_,!shifted() && launch_quantized_);
   case ClipControlAction::Follow:clip_follow_page_=!clip_follow_page_;dirty_=true;return true;
   case ClipControlAction::NextPattern:return select_pattern((project_.active_pattern+1)%16);
   case ClipControlAction::Undo:return undo();
   case ClipControlAction::Redo:return redo();
   default:return false;
  }}
  if(mode_==UiMode::Scene){switch(scene_button(index,launch_quantized_,following_clip_,project_.chain.enabled,history_cursor_,history_size_,shifted()).action){
   case SceneAction::Duplicate:return begin_scene_duplicate();
   case SceneAction::Store:return store_scene(scene_slot_);
   case SceneAction::Follow:return chain_enabled(!project_.chain.enabled);
   case SceneAction::LaunchMode:launch_quantized_=!launch_quantized_;return true;
   case SceneAction::Perform:set_mode(UiMode::Perform);return true;
   case SceneAction::ClipFollow:following_clip_=!following_clip_;dirty_=true;return true;
   case SceneAction::PatNext:return select_pattern((project_.active_pattern+1)%16);
   case SceneAction::Undo:return undo();
   case SceneAction::Redo:return redo();
   default:return false;
  }}
  if(mode_==UiMode::Project && pending_project_==ProjectAction::None && project_button(index,shifted(),false,midi_capture_state_!=0,history_cursor_,history_size_).action==ProjectTopAction::InternalRec)return request_file(FileAction::MidiRecordInternal);
  if(mode_==UiMode::Project && index<4){switch(project_button(index,shifted(),false,midi_capture_state_!=0,history_cursor_,history_size_).action){
   case ProjectTopAction::MidiRecord:return request_file(FileAction::MidiRecord);
   case ProjectTopAction::Save:request_save();return true;
   case ProjectTopAction::Open:return open_project_browser();
   case ProjectTopAction::New:request_project(ProjectAction::New);return true;
   case ProjectTopAction::SaveAs:return begin_project_save_as();
   default:return false;
  }}
if(mode_==UiMode::Settings){switch(settings_button(index,update_confirm_,settings_page_).action){
   case SettingsAction::ConfirmUpdate:update_confirm_=false;if(playing_){notify(UiNotice::Failed);return false;}update_requested_=true;return true;
   case SettingsAction::CancelUpdate:update_confirm_=false;return true;
   case SettingsAction::Page:settings_page_=uint8_t(settings_button(index,false,settings_page_).minimum);settings_cursor_=0;encoder_edit_=false;dirty_=true;return true;
   case SettingsAction::More:settings_page_=uint8_t(((settings_page_/3+1)*3)%9);if(settings_page_>=7)settings_page_=0;settings_cursor_=0;encoder_edit_=false;dirty_=true;return true;
   case SettingsAction::Routing:set_mode(UiMode::Routing);return true;
   case SettingsAction::Midi:modulation_page_=3;set_mode(UiMode::Modulation);return true;
   case SettingsAction::Diagnostics:set_mode(UiMode::Diagnostics);return true;
   case SettingsAction::Transfer:transfer_request_=true;dirty_=true;return true;
   case SettingsAction::Update:if(playing_)return false;update_confirm_=true;dirty_=true;return true;
   default:return false;
  }}if(mode_==UiMode::Project){if(index==4)return request_file(FileAction::Collect);if(index==5)return open_project_browser();}if(mode_==UiMode::Generate && gen_page_==2){auto ga=generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_).action;if(ga==GenerateAction::Extract){groove_clip_clipped_=extract_groove(track_const(selected_),project_.patterns[project_.active_pattern].swing,groove_clip_timing_,groove_clipboard_);groove_copied_=true;dirty_=true;if(groove_clip_clipped_)notify(UiNotice::GrooveClipped);return true;}if(ga==GenerateAction::Paste){Command after{CommandType::SetGrooveSnapshot,selected_,0,0},before=after;after.groove=groove_clipboard_;bool limited=paste_groove(groove_clip_timing_,project_.patterns[project_.active_pattern].swing,after.groove);before.groove=track_const(selected_).groove;bool ok=edit(after,before);if(ok && (limited || groove_clip_clipped_))notify(UiNotice::GrooveClipped);return ok;}}switch(default_button(index,page_,history_cursor_,history_size_).action){
  case DefaultAction::Page:if(mode_==UiMode::Generate){gen_page_=uint8_t(generate_button(index,shifted(),gen_page_,groove_copied_,history_cursor_,history_size_).minimum);dirty_=true;}else step_page((page_/4)*4+index);return true;
  case DefaultAction::PatPrev:return select_pattern((project_.active_pattern+15)%16);
  case DefaultAction::PatNext:return select_pattern((project_.active_pattern+1)%16);
  case DefaultAction::Undo:return undo();
  case DefaultAction::Redo:return redo();
  default:return false;
 }}
auto Performance::select_track(unsigned index)  -> bool{ if(index>=16) return false;if(index!=selected_){bake_stage_=BakeStage::Off;release_strip();loop_confirm_=false;sn_ref_armed_=false;extend_pending_=false;} selected_=uint8_t(index);axes_.focus=Focus::Track;project_.active_pattern=uint8_t(pending_[index]>=0?pending_[index]:project_.clips[index].pattern);following_clip_=true;if(mode_==UiMode::Effects && fx_page_==4 && track_const(selected_).instrument!=Instrument::DrumKit)fx_page_=3; refresh_steps(); dirty_=true; return true; }
auto Performance::select_capture_source(int delta,bool track_only) -> bool{if(!recording_source_control(recording_controls()).enabled){notify(UiNotice::Failed);return false;}Command after{CommandType::SetRouting,0,0,0},before=after;before.routing=project_.routing;after.routing=before.routing;unsigned source=before.routing.capture_bus,kind=capture_kind(source),track=capture_track(source);if(track_only){if(source>=53 && source<=60){after.routing.capture_bus=uint8_t(53+bounded(int(source-53)+delta,0,7));return edit(after,before);}if(track>=16)return false;track=unsigned(bounded(int(track)+delta,0,15));after.routing.capture_bus=uint8_t(3+16*(kind-3)+track);}else{kind=unsigned(bounded(int(kind)+delta,0,8));if(track>=16)track=selected_;after.routing.capture_bus=uint8_t(kind<3?kind:kind<6?3+16*(kind-3)+track:kind==6?51:kind==7?52:53+group_cursor_);}return after.routing.capture_bus==before.routing.capture_bus || edit(after,before);}
auto Performance::select_pattern(unsigned index)  -> bool{ if(index>=kPatterns) return false;if(index!=project_.active_pattern){release_strip();sn_ref_armed_=false;extend_pending_=false;} project_.active_pattern=uint8_t(index);following_clip_=false; refresh_steps(); dirty_=true; return true; }
auto Performance::other_preview_target()const -> bool{if(!browser_.previewing || browser_.cursor>=browser_.count || browser_.entries[browser_.cursor].directory || !is_wav_file(browser_.entries[browser_.cursor].name))return false;char path[160];return join_path(path,sizeof path,browser_.directory,browser_.entries[browser_.cursor].name) && __builtin_strcmp(path,browser_.audition_path)!=0;}
auto Performance::encoder_hint()const -> const char*{if(browser_editing())return browser_confirmation()?"PUSH:OK LEFT:BACK":"PUSH:APPLY LEFT:CANCEL";if(modal_active())return modal_progress()?"LEFT:CANCEL":"PUSH:OK LEFT:BACK";if(mode_==UiMode::Clip && clip_follow_page_)return "TURN: FOLLOW ACTION   PUSH: DONE";
  if(main_encoder_mode_)return main_encoder_mode_==1?"HOLD VOLUME: TRACK LEVEL":main_encoder_mode_==2?"HOLD SWING: SWING":"HOLD TEMPO: BPM";
  if(shifted())return mode_==UiMode::Browser?"SHIFT + TURN: TRACK   PUSH: SAVE PRESET":"SHIFT + TURN: TRACK";
  if(mode_==UiMode::Browser)return "TURN: FILE   PUSH: OPEN / PREVIEW";
  if(mode_==UiMode::Song)return project_.song.length && song_cursor_<project_.song.length && project_.song.sections[song_cursor_].ticks?"TURN: SECTION   PLAY: WHOLE ARRANGEMENT":"TURN: SECTION   PUSH: SCENE";
  if(mode_==UiMode::Step)return tracker_view_?(follow_display_?"TRACKER / FOLLOW / * = MORE":"TRACKER / EDIT / * = MORE"):"TURN: STEP   PUSH: TOGGLE";
  if(mode_==UiMode::Clip || mode_==UiMode::Scene)return "TURN: SELECT   PUSH: LAUNCH";
  if(mode_==UiMode::Sampler)return "TURN: PAD / SLICE   PUSH: PREVIEW";
  if(mode_==UiMode::Settings)return encoder_edit_?"PUSH: DONE":"TURN / PUSH: EDIT";
  if(mode_==UiMode::Effects && (fx_page_==3 || fx_page_==4))return encoder_edit_?"EDIT FX TYPE   PUSH: DONE":"TURN: SLOT / PAD   PUSH: EDIT";
  if(mode_==UiMode::Routing && routing_fx_page_ && group_send_page_)return "TURN: GROUP   L/R: INSERTS";
  if(mode_==UiMode::Routing && routing_fx_page_)return encoder_edit_?"EDIT FX TYPE   PUSH: DONE":"TURN: GROUP   PUSH: EDIT";
  if(mode_==UiMode::Modulation && modulation_page_==2)return encoder_edit_?"EDIT TARGET   PUSH: DONE":"TURN: ROUTE   PUSH: EDIT";
  if(mode_==UiMode::Perform && performance_page_==0 && control_all_)return "TURN: TARGET   PUSH: CENTER";
  if(mode_==UiMode::Library)return "TURN: SOUND   PUSH: LOAD / PAGE BACK: RETURN";
  if(mode_==UiMode::Project)return pending_project_!=ProjectAction::None?"PUSH: CONFIRM   LEFT: CANCEL":"TURN: ACTION   PUSH: APPLY";
  if(mode_==UiMode::Diagnostics)return "TURN: SCROLL   L/R: PAGE";
  if(mode_==UiMode::Pad && track_const(selected_).instrument==Instrument::DrumKit)return "TURN: PAD   PUSH: SAMPLE";
  if(mode_==UiMode::Synth && source_select_)return "TURN: ENGINE   PUSH: SOUNDS / TOP 5: LOAD ENGINE";
  if(mode_==UiMode::Mixer || mode_==UiMode::Routing)return "TURN / ARROWS: TRACK";
  return "TURN: TRACK   L/R: PAGE";
 }
auto Performance::encoder(int delta) -> bool{if(transfer_view_.active)return false;if(modal_active() && !browser_editing())return false;if(mode_==UiMode::Clip && clip_follow_page_)return knob(0,int16_t(bounded(delta,-16,16)*1000));if(browser_editing())return browser_edit_character(bounded(delta,-16,16));
  delta=bounded(delta,-16,16);if(!delta)return true;
  if(main_encoder_mode_){if(main_encoder_mode_==1)return set_parameter(1,ui::clamp01(track_const(selected_).level+delta*0.02f));Command after{main_encoder_mode_==2?CommandType::SetSwing:CommandType::SetTempo,0,0,uint16_t(main_encoder_mode_==2?bounded(int(project_.patterns[project_.active_pattern].swing)+delta,0,100):bounded(int(project_.tempo)+delta,20,300))},before=after;before.value=main_encoder_mode_==2?project_.patterns[project_.active_pattern].swing:project_.tempo;return edit(after,before);}
  if(shifted())return select_track(unsigned(bounded(int(selected_)+delta,0,15)));
  dirty_=true;const bool kit=track_const(selected_).instrument==Instrument::DrumKit;
  if(library_catalog_controls())return move_preset_library(delta);
  if(mode_==UiMode::Browser)return move_browser(delta);
  if(mode_==UiMode::Diagnostics){diagnostic_offset_=unsigned(bounded(int(diagnostic_offset_)+delta,0,diagnostics_.log_count>3?int(diagnostics_.log_count-3):0));return true;}
  if(mode_==UiMode::Step){if(tracker_view_)follow_display_=false;selected_step_=uint8_t(bounded(int(selected_step_)+delta,0,kSteps-1));page_=selected_step_/16;refresh_steps();return true;}
  if(mode_==UiMode::Clip)return select_pattern(unsigned(bounded(int(project_.active_pattern)+delta,0,kPatterns-1)));
  if(mode_==UiMode::Scene){scene_slot_=uint8_t(bounded(int(scene_slot_)+delta,0,15));return true;}
  if(mode_==UiMode::Song){unsigned count=song_display_count();song_cursor_=uint8_t(bounded(int(song_cursor_)+delta,0,int(count?count-1:0)));dirty_=true;return true;}   if(mode_==UiMode::Sampler){if(kit)return select_kit_pad(unsigned(bounded(int(selected_kit_pad_)+delta,0,15)));const auto& t=track_const(selected_);unsigned count=t.sample_edit.slice_count?t.sample_edit.slice_count:t.sample_slices;return set_sample_region(unsigned(bounded(int(t.sample_slice)+delta,0,int(count)-1)),count,t.sample_loop,t.sample_reverse);}
  if(mode_==UiMode::Pad && kit)return select_kit_pad(unsigned(bounded(int(selected_kit_pad_)+delta,0,15)));
  if(mode_==UiMode::Settings){if(encoder_edit_)return knob(0,int16_t(delta*1000));settings_cursor_=uint8_t(bounded(int(settings_cursor_)+delta,0,settings_count()?int(settings_count()-1):0));return true;}
  if(mode_==UiMode::Project){if(pending_project_!=ProjectAction::None)return false;project_list_cursor_=uint8_t(bounded(int(project_list_cursor_)+delta,0,2));return true;}
  if(mode_==UiMode::Effects && fx_page_==5){chain_node_=uint8_t(bounded(int(chain_node_)+delta,0,3));return true;}
  if(mode_==UiMode::Effects && (fx_page_==3 || fx_page_==4)){if(encoder_edit_)return knob(fx_page_==4?1:unsigned(fx_focus_slot_)*4,int16_t(delta*1000));if(fx_page_==4)return select_kit_pad(unsigned(bounded(int(selected_kit_pad_)+delta,0,15)));fx_focus_slot_=uint8_t(bounded(int(fx_focus_slot_)+delta,0,1));return true;}
  if(mode_==UiMode::Routing && routing_fx_page_){if(encoder_edit_ && !group_send_page_)return knob(1,int16_t(delta*1000));group_cursor_=uint8_t(bounded(int(group_cursor_)+delta,0,7));return true;}
  if(mode_==UiMode::Modulation && modulation_page_==2)return knob(encoder_edit_?2:1,int16_t(delta*1000));
  if(source_select_){return select_source(unsigned(bounded(int(source_cursor_)+delta,0,int(instrument_count())-1)));}
  if(mode_==UiMode::Perform && performance_page_==0 && control_all_)return select_control_all_target(delta);
  return select_track(unsigned(bounded(int(selected_)+delta,0,15)));
 }
auto Performance::encoder_push() -> bool{if(select_held() && mode_!=UiMode::Browser && mode_!=UiMode::Library && !modal_active())return request_file(FileAction::SaveCurrentPreset);NavigationScope navigation_scope(*this);if(modal_active())return modal_accept();if(mode_==UiMode::Clip && clip_follow_page_){clip_follow_page_=false;dirty_=true;return true;}if(browser_editing())return browser_edit_key(0);
  if(mode_==UiMode::Browser && shifted())return request_file(FileAction::SavePreset);
  dirty_=true;
  if(mode_==UiMode::Sampler && sampler_page_==4 && shifted()){const auto& t=track_const(selected_);auto e=t.instrument==Instrument::DrumKit?t.kit.pads[selected_kit_pad_].edit:t.sample_edit;unsigned count=e.slice_count?e.slice_count:t.sample_slices;if(!count)count=1;unsigned idx=slice_cursor_%count;e.slice_warp[idx]=e.slice_warp[idx]?0:1;return sample_edit(e);}
  if(main_encoder_mode_ || shifted())return false;
  if(mode_==UiMode::Library)return request_file(FileAction::Load);
  if(mode_==UiMode::Browser)return request_file(FileAction::Open);
  if(mode_==UiMode::Clip)return launch_clip(selected_,project_.active_pattern,launch_quantized_);
  if(mode_==UiMode::Scene)return launch_scene(scene_slot_,launch_quantized_);
  if(mode_==UiMode::Song){if(song_cursor_<project_.song.length){if(project_.song.sections[song_cursor_].ticks){copy_text(scene_take_message_,sizeof scene_take_message_,"Recorded phases / PLAY starts arrangement from beginning");notify(UiNotice::Failed);dirty_=true;return false;}return launch_scene(project_.song.sections[song_cursor_].scene);}if(song_cursor_<project_.chain.length)return launch_scene(project_.chain.scenes[song_cursor_]);return false;}
  if(mode_==UiMode::Step){const auto& t=track_const(selected_);if(t.instrument==Instrument::DrumKit){auto s=t.kit.steps[selected_kit_pad_][selected_step_];s.enabled=!s.enabled;return set_kit_step(selected_kit_pad_,selected_step_,s);}return set_step(selected_step_,!t.steps[selected_step_].enabled);}
  if(mode_==UiMode::Sampler && sampler_page_==4){zone_vel_=!zone_vel_;dirty_=true;return true;}
  if(mode_==UiMode::Sampler)return request_file(FileAction::AuditionSelectedSample);
  if(mode_==UiMode::Effects && fx_page_==5)return chain_edit();
  if(mode_==UiMode::Settings || (mode_==UiMode::Effects && (fx_page_==3 || fx_page_==4)) || (mode_==UiMode::Modulation && modulation_page_==2) || (mode_==UiMode::Routing && routing_fx_page_ && !group_send_page_)){encoder_edit_=!encoder_edit_;return true;}
  if(mode_==UiMode::Project && pending_project_!=ProjectAction::None)return confirm_project();
  if(mode_==UiMode::Project){if(project_list_cursor_==1)return store_sound(sound_slot_);if(project_list_cursor_==2)return recall_sound(sound_slot_);return open_project_browser();}
  if(source_select_)return open_preset_library();
  if(mode_==UiMode::Perform && performance_page_==0 && control_all_)return reset_control_all_offset();
  if(mode_==UiMode::Pad){set_mode(track_const(selected_).instrument==Instrument::DrumKit?UiMode::Sampler:UiMode::Synth);return true;}
  return false;
 }
auto Performance::synth_adjacent_page(int delta)const -> int{
  if(wave_profile() || repeat_mode_ || (delta!=1 && delta!=-1))return -1;
  auto context=synth_controls();context.shift=false;
  for(int page=int(instrument_view_)+delta;page>=0 && page<4;page+=delta){
   const auto d=synth_button_control(unsigned(page),context);
   if(d.enabled && (d.action==SynthAction::Generator || d.action==SynthAction::Tone || d.action==SynthAction::Shape || d.action==SynthAction::Lab))return page;
  }
  return -1;
 }
auto Performance::encoder_direction_available(uint8_t direction)const -> bool{
  if(modal_active())return browser_editing() || direction==0x20;
  const bool previous=direction==0x20 || direction==0x04;
  const bool horizontal=direction==0x20 || direction==0x08;
  if(horizontal && select_held() && mode_!=UiMode::Browser && mode_!=UiMode::Library)return !file_action_pending();
  auto range=[&](unsigned position,unsigned count){return previous?position>0:position+1<count;};
  if(browser_editing())return true;
  if(mode_==UiMode::Project && pending_project_!=ProjectAction::None)return direction==0x20;
  if(horizontal){
   if(mode_==UiMode::Library)return false;
   if(mode_==UiMode::Browser)return previous || browser_.cursor<browser_.count;
   if(shifted())return range(project_.active_pattern,kPatterns);
   if(mode_==UiMode::Sampler)return range(sampler_page_,5);
   if(mode_==UiMode::Settings)return range(settings_page_,7);
   if(mode_==UiMode::Effects)return range(fx_page_,6);
   if(mode_==UiMode::Synth)return source_select_?range(source_cursor_,instrument_count()):synth_adjacent_page(previous?-1:1)>=0;
   if(mode_==UiMode::Modulation)return range(modulation_page_,5);
   if(mode_==UiMode::Generate)return range(gen_page_,6);
   if(mode_==UiMode::Diagnostics)return false;
   if(mode_==UiMode::Routing && routing_fx_page_)return group_send_page_ || range(group_fx_slot_,2);
   if(mode_==UiMode::Perform)return range(performance_page_,6);
   if(mode_==UiMode::Step && tracker_view_){return range(selected_,16);}if(mode_==UiMode::Step || mode_==UiMode::Pad || mode_==UiMode::Keyboard || mode_==UiMode::Chord)return range(page_,16);
  }
  if(main_encoder_mode_ || (mode_==UiMode::Clip && clip_follow_page_))return true;
  if(shifted())return range(selected_,16);
  if(mode_==UiMode::Library)return !file_action_pending() && browser_.library_total>1;
  if(mode_==UiMode::Browser)return range(browser_.folder_offset+browser_.cursor,browser_.folder_total?browser_.folder_total:browser_.count);
  if(mode_==UiMode::Diagnostics)return range(diagnostic_offset_,diagnostics_.log_count>3?diagnostics_.log_count-2:1);
  if(mode_==UiMode::Step)return range(selected_step_,kSteps);
  if(mode_==UiMode::Clip)return range(project_.active_pattern,kPatterns);
  if(mode_==UiMode::Scene)return range(scene_slot_,16);
  if(mode_==UiMode::Song)return range(song_cursor_,song_display_count());
  const auto& track=track_const(selected_);
  if(mode_==UiMode::Sampler)return track.instrument==Instrument::DrumKit?range(selected_kit_pad_,16):range(track.sample_slice,track.sample_edit.slice_count?track.sample_edit.slice_count:track.sample_slices);
  if(mode_==UiMode::Pad && track.instrument==Instrument::DrumKit)return range(selected_kit_pad_,16);
  if(mode_==UiMode::Settings)return range(settings_cursor_,settings_count());
  if(mode_==UiMode::Project)return range(project_list_cursor_,3);
  if(mode_==UiMode::Effects && fx_page_==5)return range(chain_node_,4);
  if(mode_==UiMode::Effects && fx_page_==3)return range(fx_focus_slot_,2);
  if(mode_==UiMode::Effects && fx_page_==4)return range(selected_kit_pad_,16);
  if(mode_==UiMode::Routing && routing_fx_page_)return range(group_cursor_,8);
  if(mode_==UiMode::Modulation && modulation_page_==2)return range(macro_route_,4);
  if(source_select_)return range(source_cursor_,instrument_count());
  if(mode_==UiMode::Perform && performance_page_==0 && control_all_)return true;
  return range(selected_,16);
 }
auto Performance::encoder_direction(uint8_t rising) -> bool{NavigationScope navigation_scope(*this);if(modal_active() && (rising&0x20))return modal_cancel();if(modal_active() && !browser_editing())return false;if(browser_editing()){if(rising&1)return browser_edit_key(0);if(rising&(0x20|0x08))return browser_edit_key((rising&0x20)?6:7);if(rising&(0x04|0x10))return browser_edit_character((rising&0x04)?-1:1);return true;}
  if(rising&0x01)return encoder_push();
  if(rising&(0x04|0x10)){encoder_edit_=false;return encoder((rising&0x04)?-1:1);}
  if(!(rising&(0x20|0x08)))return true;
  int delta=(rising&0x20)?-1:1;encoder_edit_=false;dirty_=true;
  if(mode_==UiMode::Project && pending_project_!=ProjectAction::None){if(delta<0)cancel_project();return true;}
  if(select_held() && mode_!=UiMode::Browser && mode_!=UiMode::Library)return request_file(delta<0?FileAction::LibraryPrev:FileAction::LibraryNext);
  if(mode_==UiMode::Library)return false;
  if(mode_==UiMode::Browser)return request_file(delta<0?FileAction::Up:FileAction::Open);
  if(shifted())return select_pattern(unsigned(bounded(int(project_.active_pattern)+delta,0,kPatterns-1)));
  if(mode_==UiMode::Sampler){sampler_page_=uint8_t(bounded(int(sampler_page_)+delta,0,4));return true;}
  if(mode_==UiMode::Settings){settings_page_=uint8_t(bounded(int(settings_page_)+delta,0,6));settings_cursor_=0;return true;}
  if(mode_==UiMode::Effects){int nf=bounded(int(fx_page_)+delta,0,5);if(track_const(selected_).instrument!=Instrument::DrumKit && nf==4)nf=delta>0?5:3;fx_page_=uint8_t(nf);return true;}
  if(mode_==UiMode::Synth){if(source_select_){return select_source(unsigned(bounded(int(source_cursor_)+delta,0,int(instrument_count())-1)));}int target=synth_adjacent_page(delta);return target<0?true:soft_key(unsigned(target));}
  if(mode_==UiMode::Modulation){modulation_page_=uint8_t(bounded(int(modulation_page_)+delta,0,4));return true;}
  if(mode_==UiMode::Generate){gen_page_=uint8_t(bounded(int(gen_page_)+delta,0,5));return true;}
  if(mode_==UiMode::Diagnostics)return false;
  if(mode_==UiMode::Routing && routing_fx_page_){if(group_send_page_)group_send_page_=false;else group_fx_slot_=uint8_t(bounded(int(group_fx_slot_)+delta,0,1));return true;}
  if(mode_==UiMode::Perform){if(performance_page_==4 && performance_fx_mode_!=PerformanceFx::Off){release_performance_fx_=true;retry_releases();}performance_page_=uint8_t(bounded(int(performance_page_)+delta,0,5));return true;}
  if(mode_==UiMode::Step && tracker_view_){return select_track(unsigned(bounded(int(selected_)+delta,0,15)));}if(mode_==UiMode::Step || mode_==UiMode::Pad || mode_==UiMode::Keyboard || mode_==UiMode::Chord){step_page(unsigned(bounded(int(page_)+delta,0,15)));return true;}
  return encoder(delta);
 }
auto Performance::help_button()const -> const char*{switch(mode_){case UiMode::Pad:return "pad_mode";case UiMode::Step:return "step";case UiMode::Clip:return "pattern";case UiMode::Scene:return "scene";case UiMode::Mixer:return "mixer";case UiMode::Synth:return "plugin";case UiMode::Effects:case UiMode::Routing:return "channel";case UiMode::Perform:return "perform";case UiMode::Generate:return "variations";case UiMode::Keyboard:return "keyboard";case UiMode::Sampler:return "sampling";case UiMode::Project:return "file";case UiMode::Settings:return "settings";case UiMode::Modulation:return "auto";case UiMode::Library:return "plugin";case UiMode::Browser:return "browser";case UiMode::Diagnostics:return "top_8";case UiMode::Chord:return "chords";case UiMode::Song:return "arranger";}return "pad_mode";}
auto Performance::synth_core_page()const -> bool{return mode_==UiMode::Synth && !source_select_ && !wave_profile() && instrument_view_==0 && track_const(selected_).instrument!=Instrument::Layer && track_const(selected_).instrument!=Instrument::DrumKit && track_const(selected_).instrument!=Instrument::ExternalMidi;}
auto Performance::native_synth_page()const -> bool{return mode_==UiMode::Synth && !source_select_ && !wave_profile() && ((track_const(selected_).instrument==Instrument::Resonator && (instrument_view_==0 || instrument_view_==2)) || (track_const(selected_).instrument==Instrument::Texture && (instrument_view_==0 || instrument_view_==2 || instrument_view_==3)));}
auto Performance::toggle_source_select() -> bool{
  NavigationScope navigation_scope(*this);
  if(source_select_){source_select_=false;source_arm_unknown_=false;browser_.library_list=false;browser_refresh_pending_=false;dirty_=true;return true;}
  source_select_=true;source_arm_unknown_=false;
  unsigned current=unsigned(track_const(selected_).instrument);
  return select_source(current<instrument_count() && current!=unsigned(Instrument::Empty)?current:0);
 }
auto Performance::pad_hint()const -> const char*{
  if(pad_role()==ui::PadRole::Clips)return select_held()?"PADS: EDIT PATTERN":shifted()?"PADS: STOP CLIP":"PADS: LAUNCH CLIP";
  if(pad_role()==ui::PadRole::Scenes)return select_held()?"PADS: STORE SCENE":shifted()?"PADS: APPEND CHAIN":"PADS: LAUNCH SCENE";
  if(pad_role()==ui::PadRole::Steps && (shifted() || select_held() || mode_==UiMode::Routing))return "PADS: SELECT STEP";
  return ui::pad_role_label(pad_role());
 }
auto Performance::keyboard_pad_state(unsigned n)const -> ui::LedState{
  using S=ui::LedState;const auto t=track_const(selected_);int input=keyboard_note(n);
  if(input<0 || t.instrument==Instrument::Empty)return S::Off;
  unsigned pitch=unsigned(bounded(input+int(t.transpose),0,127));unsigned pc=(pitch+12-t.scale_root)%12;
  if(!(t.scale_mask&(1u<<pc)))return S::Off;
  return pc==0?S::Selected:S::Assigned;
 }
auto Performance::base_button_leds(uint8_t out[62]) const  -> void{
  for(unsigned i=0;i<62;++i)out[i]=0;
  // NHL2 bright 0x9e follows encdr's encoding, pending physical calibration.
  out[41]=playing_?0x9e:0;out[43]=playing_?0:0x9e;out[44]=(controls_[0]&0x40)?0x9e:0;
  out[47]=mode_==UiMode::Keyboard?0x9e:0;out[48]=mode_==UiMode::Chord?0x9e:0;out[5]=mode_==UiMode::Sampler?0x9e:0;
  out[46]=mode_==UiMode::Pad?0x9e:0;out[49]=mode_==UiMode::Step?0x9e:0;out[50]=mode_==UiMode::Scene?0x9e:0;out[51]=mode_==UiMode::Clip?0x9e:0;
  out[3]=mode_==UiMode::Mixer?0x9e:0;out[1]=mode_==UiMode::Synth?0x9e:0;out[0]=(mode_==UiMode::Effects || mode_==UiMode::Routing)?0x9e:0;out[27]=mode_==UiMode::Perform?0x9e:0;out[53]=mode_==UiMode::Generate?0x9e:0;out[29+selected_%8]=31;
  out[56]=(controls_[4]&0x10)?mk3::mono_active:0;out[57]=(controls_[4]&0x20)?mk3::mono_active:0;
  for(unsigned i=0;i<8;++i)out[12+i]=soft_enabled(i)?(soft_selected(i)?mk3::mono_active:mk3::mono_dim):0;
  if(mode_==UiMode::Mixer && !modal_active()){auto twin=twin_status();out[17]=twin_queued()?((now_ms_/180)%2?mk3::mono_active:0):twin==TwinStatus::Active?mk3::mono_active:twin==TwinStatus::Ready?mk3::mono_dim:twin==TwinStatus::Stale?((now_ms_/500)%2?mk3::mono_dim:0):0;}
 }
auto Performance::synth_key(unsigned index)  -> bool{
  const auto control=synth_button_control(index,synth_controls());
  if(!control.enabled)return false;
  switch(control.action){
   case SynthAction::LayerUse:return layer_use();
   case SynthAction::LayerRemove:return confirm_erase(UiErase::LayerRemove);
   case SynthAction::BakePreset:return begin_bake();
   case SynthAction::StoreSound:return store_sound(sound_slot_);
   case SynthAction::RecallSound:return recall_sound(sound_slot_);
   case SynthAction::Source:
    source_select_=true;
    source_cursor_=unsigned(track_const(selected_).instrument)<instrument_count() && track_const(selected_).instrument!=Instrument::Empty?uint8_t(track_const(selected_).instrument):uint8_t(0);
    return inspect_source_library();
   case SynthAction::Sample:set_mode(UiMode::Sampler);return true;
   case SynthAction::Trim:sampler_page_=1;set_mode(UiMode::Sampler);return true;
   case SynthAction::Profile:return set_wave_profile(!wave_profile());
   case SynthAction::Undo:return undo();
   case SynthAction::Redo:return redo();
   case SynthAction::Generator:case SynthAction::Tone:case SynthAction::Shape:
   case SynthAction::Lab:case SynthAction::Modulation:
    source_select_=false;source_arm_unknown_=false;
    if(control.action==SynthAction::Modulation)set_mode(UiMode::Modulation);
    else instrument_view_=uint8_t(index);
    dirty_=true;return true;
   case SynthAction::WaveMacros:case SynthAction::WaveClock:{
    sn_ref_armed_=false;
    const uint16_t bit=uint16_t(1u<<selected_);
    project_.ui.wave_clock_mask=control.action==SynthAction::WaveClock
     ?uint16_t(project_.ui.wave_clock_mask|bit):uint16_t(project_.ui.wave_clock_mask&~bit);
    dirty_=true;return true;
   }
   case SynthAction::WaveTrigger:{
    sn_ref_armed_=false;Command c{CommandType::SendMidiCC,selected_,26,127};return send(c);
   }
   case SynthAction::WaveReference:{
    if(!sn_ref_confirmation()){
     sn_ref_armed_=true;sn_ref_track_=selected_;sn_ref_pattern_=project_.active_pattern;
     sn_ref_channel_=track_const(selected_).midi_channel;sn_ref_armed_ms_=now_ms_;
     notify(UiNotice::SnRefConfirm);return true;
    }
    sn_ref_armed_=false;Command c{CommandType::SendMidiCC,selected_,27,wave_ref_[selected_]};
    c.pattern=project_.active_pattern;bool ok=send(c);
    notify(ok?UiNotice::SnRefQueued:UiNotice::Failed);return ok;
   }
   default:return false;
  }
 }
}
