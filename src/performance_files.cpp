#include "twin_digest.h"
// Shared UI implementation: ordinary translation-unit boundaries, uniform -O3.
#include "performance.h"
#include "content_shelf.h"
namespace groove {
auto Performance::resample_view()const -> ResampleRequest{if(resampling_)return active_resample_;if(file_action_==FileAction::Resample)return queued_resample_;return {current_recording_target(),uint8_t(capture_source()),resample_bars_};}
auto Performance::wave_value(unsigned item)const -> unsigned{
  if(item>=10)return 0;
  unsigned cc=item<4?16+item:20+item-4;const auto& t=track_const(selected_);
  unsigned raw=(t.midi_cc_used[cc/8]&(1u<<(cc%8)))?t.midi_cc[cc]:((item==4 || item>=8)?0:64);
  if(lock_path())for(const auto& lock:t.steps[selected_step_].locks)if(lock.enabled && lock.id==128+cc){raw=unsigned(lock.value);break;}
  return item==9?(raw>=64?1:0):unsigned(bounded(int(raw),0,int(wave_max(item))));
 }
auto Performance::set_wave_profile(bool on) -> bool{if(on && track_const(selected_).instrument!=Instrument::ExternalMidi)return false;sn_ref_armed_=false;const uint16_t bit=uint16_t(1u<<selected_);project_.ui.wave_profile_mask=on?uint16_t(project_.ui.wave_profile_mask|bit):uint16_t(project_.ui.wave_profile_mask&~bit);dirty_=true;return true;}
auto Performance::sample_auditioning()const -> bool{const auto& t=track_const(selected_);unsigned slot=t.instrument==Instrument::DrumKit?t.kit.pads[selected_kit_pad_].sample[0]:t.instrument==Instrument::Sampler?t.sample_slot:kSampleAssets;if(slot>=kSampleAssets)return false;const auto& asset=project_.assets[slot];return browser_.previewing && browser_.audition_assigned && browser_.audition_track==selected_ && browser_.audition_pattern==project_.active_pattern && browser_.audition_pad==(t.instrument==Instrument::DrumKit?selected_kit_pad_:255) && asset.path[0] && !__builtin_strcmp(asset.path,browser_.audition_path);}
auto Performance::stretch_format() -> void{if(stretch_modal_==2){copy_text(stretch_text_,sizeof stretch_text_,"Rendering 00% - CANCEL stops");unsigned v=stretch_progress_;if(v>99)v=99;stretch_text_[10]=char('0'+(v/10)%10);stretch_text_[11]=char('0'+v%10);}else{copy_text(stretch_text_,sizeof stretch_text_,"Ratio x0.00 - START / CANCEL");stretch_text_[7]=char('0'+(stretch_ratio_pct_/100)%10);stretch_text_[9]=char('0'+(stretch_ratio_pct_/10)%10);stretch_text_[10]=char('0'+stretch_ratio_pct_%10);}}
auto Performance::select_source(unsigned engine) -> bool{
 if(engine>=instrument_count())return false;
 source_cursor_=uint8_t(engine);source_arm_unknown_=false;return inspect_source_library();
}
auto Performance::inspect_source_library() -> bool{
 browser_.library_instrument=Instrument(source_cursor_);browser_.library_kits=Instrument(source_cursor_)==Instrument::DrumKit;
 browser_.library_resume=false;browser_.library_originals=false;browser_.library_offset=browser_.library_total=browser_.cursor=browser_.count=0;browser_.library_list=true;
 browser_refresh_pending_=true;dirty_=true;return true;
}
auto Performance::open_preset_library(bool kits) -> bool{
 NavigationScope navigation_scope(*this);
 if(file_action_pending() || modal_active())return false;
 const bool from_engine=source_select_;const auto instrument=from_engine?Instrument(source_cursor_):track_const(selected_).instrument;
 const unsigned offset=browser_.library_offset,cursor=browser_.cursor;const auto origin=mode_;set_mode(UiMode::Library);
 browser_.library_return=uint8_t(origin);browser_.library_instrument=instrument;
 browser_.library_kits=kits || instrument==Instrument::DrumKit;browser_.library_originals=false;browser_.library_offset=browser_.library_total=browser_.cursor=0;browser_.library_delta=0;browser_.library_list=true;
 browser_refresh_pending_=false;if(from_engine){browser_.library_offset=offset;browser_.cursor=cursor;}return request_file(from_engine?FileAction::LibraryRestore:FileAction::LibraryOpen);
}
auto Performance::move_browser(int delta) -> bool{
 if(file_action_pending()){return false;}unsigned total=browser_.folder_total?browser_.folder_total:browser_.count;
 unsigned cursor=unsigned(bounded(int(browser_.folder_offset+browser_.cursor)+delta,0,total?int(total-1):0));
 if(cursor/64==browser_.folder_offset/64){browser_.cursor=cursor-browser_.folder_offset;dirty_=true;return true;}
 browser_.folder_delta=int(cursor)-int(browser_.folder_offset+browser_.cursor);return request_file(FileAction::FolderMove);
}
auto Performance::move_preset_library(int delta) -> bool{if(file_action_pending())return false;browser_.library_delta=delta;return request_file(FileAction::LibraryMove);}
auto Performance::preset_fx_editing()const -> bool{return browser_editing() && (browser_.edit.operation==BrowserOperation::SavePreset || browser_.edit.operation==BrowserOperation::OverwritePreset);}
auto Performance::open_project_browser() -> bool{browser_.new_project_list=false;if(file_action_pending() || browser_editing())return false;char parent[160];copy_text(parent,sizeof parent,project_path_);char* slash=nullptr;for(char* p=parent;*p;++p)if(*p=='/')slash=p;if(slash)*slash=0;else parent[0]=0;copy_text(browser_.directory,sizeof browser_.directory,parent);set_mode(UiMode::Browser);return request_file(FileAction::Refresh);}
auto Performance::begin_project_save_as() -> bool{if(file_action_pending() || browser_editing())return false;BrowserFileEdit next;next.operation=BrowserOperation::SaveProject;copy_text(next.source,sizeof next.source,kUserProject);copy_text(next.name,sizeof next.name,project_name());data_strip_mgx_suffix(next.name);browser_.edit=next;axes_.modal=ModalKind::Rename;dirty_=true;return true;}
auto Performance::bake_control()const -> bool{return bake_current() && bake_stage_!=BakeStage::Collect && !browser_editing() && !repeat_mode_ && !freeze_modal_ && !stretch_modal_ && (mode_==UiMode::Routing || mode_==UiMode::Sampler || mode_==UiMode::Browser || (mode_==UiMode::Project && bake_stage_==BakeStage::Done));}
auto Performance::bake_hint()const -> const char*{if(!bake_current())return "";switch(bake_stage_){case BakeStage::Capture:return "BAKE 1/5: PLAY / SHIFT + RESAMPLE";case BakeStage::Trim:return "BAKE 2/5: TRIM / NEXT: ROOT + ZONE";case BakeStage::Tune:return "BAKE 3/5: ROOT + ZONE / SAVE PRESET";case BakeStage::Name:return "BAKE 4/5: NAME / SAVE PRESET";case BakeStage::Collect:return "BAKE 5/5: COLLECT PRESET + SAMPLE";case BakeStage::Done:return "BAKE DONE: PORTABLE PRESET SAVED";default:return "";}}
auto Performance::bake_next_label()const -> const char*{if(shifted())return "EXIT BAKE";switch(bake_stage_){case BakeStage::Trim:return "ROOT / ZONE";case BakeStage::Tune:case BakeStage::Name:return "SAVE PRESET";case BakeStage::Collect:return "COLLECT";case BakeStage::Done:return "DONE";default:return "EXIT BAKE";}}
auto Performance::begin_bake() -> bool{
  const auto t=track_const(selected_);if(recording_configuration_locked(recording_controls()) || browser_editing() || audio_lane().kind!=TrackKind::Instrument || t.instrument==Instrument::Empty || t.instrument==Instrument::Unknown || t.instrument==Instrument::ExternalMidi || t.instrument==Instrument::DrumKit)return false;
  Command after{CommandType::SetRouting,0,0,0},before=after;before.routing=after.routing=project_.routing;after.routing.capture_bus=uint8_t(3+selected_);
  if(!edit(after,before))return false;
  bake_track_=selected_;bake_generation_=engine_.staged_generation();bake_revision_=assignment_revision_[selected_];bake_stage_=BakeStage::Capture;bake_path_[0]=0;routing_fx_page_=false;resample_bars_=1;set_mode(UiMode::Routing);return true;
 }
auto Performance::bake_recorded(unsigned track,bool assigned) -> void{if(bake_stage_!=BakeStage::Capture || track!=bake_track_ || bake_generation_!=engine_.staged_generation())return;if(!assigned)return;if(track!=selected_ || track_const(track).instrument!=Instrument::Sampler){bake_stage_=BakeStage::Off;return;}bake_revision_=assignment_revision_[track];bake_stage_=BakeStage::Trim;sampler_page_=1;set_mode(UiMode::Sampler);}
auto Performance::bake_saved(const char* path) -> void{if(!bake_current() || bake_stage_!=BakeStage::Name)return;copy_text(bake_path_,sizeof bake_path_,path);bake_stage_=BakeStage::Collect;cancel_browser_edit();set_mode(UiMode::Project);dirty_=true;}
auto Performance::bake_next() -> bool{if(!bake_control())return false;if(shifted() || bake_stage_==BakeStage::Capture || bake_stage_==BakeStage::Done){bake_stage_=BakeStage::Off;dirty_=true;return true;}if(recording_configuration_locked(recording_controls()))return false;if(bake_stage_==BakeStage::Trim){auto e=track_const(selected_).sample_edit;if(!e.slice_count){e.slice_count=1;e.markers[0]=0;e.markers[1]=1;for(auto& pad:e.pads)pad=0;}if(!e.zone_count){e.zone_count=1;e.zones[0].key_low=0;e.zones[0].key_high=127;e.zones[0].velocity_low=1;e.zones[0].velocity_high=127;e.zones[0].root=60;}if(!sample_edit(e))return false;bake_stage_=BakeStage::Tune;sampler_page_=4;set_mode(UiMode::Sampler);return true;}if(bake_stage_==BakeStage::Tune || bake_stage_==BakeStage::Name){if(!begin_preset_save())return false;bake_stage_=BakeStage::Name;return true;}return bake_stage_==BakeStage::Collect && request_file(FileAction::Collect);}
auto Performance::begin_preset_save(bool duplicate) -> bool{
  if(file_action_pending() || browser_editing())return false;
  const auto& t=track_const(selected_);if(audio_lane().kind!=TrackKind::Instrument || t.instrument==Instrument::Empty || t.instrument==Instrument::Unknown){notify(UiNotice::Failed);return false;}
  preset_save_track_=selected_;preset_save_generation_=engine_.staged_generation();preset_save_revision_=assignment_revision_[selected_];
  BrowserFileEdit next;next.duplicate=duplicate;next.operation=BrowserOperation::SavePreset;copy_text(next.source,sizeof next.source,data_shelf_for_content(unsigned(t.instrument==Instrument::DrumKit?ContentKind::Kit:ContentKind::Preset)));
  const auto* d=engine_descriptor(unsigned(t.instrument));copy_text(next.name,sizeof next.name,t.preset_name[0]?t.preset_name:d?d->instrument.name:"Preset");
  set_mode(UiMode::Browser);browser_.edit=next;axes_.modal=ModalKind::Rename;dirty_=true;return true;
 }
auto Performance::begin_track_rename() -> bool{
 if(file_action_pending() || modal_active())return false;
 track_name_return_mode_=mode_;track_name_target_=selected_;track_name_generation_=engine_.staged_generation();BrowserFileEdit next;next.operation=BrowserOperation::RenameTrack;copy_text(next.name,96,track_const(selected_).track_name);copy_text(next.source,160,"Empty name uses preset / engine name");set_mode(UiMode::Browser);browser_.edit=next;axes_.modal=ModalKind::Rename;dirty_=true;return true;
}
auto Performance::rename_track(unsigned track,const char* name) -> bool{
 if(track>=kTracks || !name)return false;
 char checked[96]{};if(!copy_text(checked,sizeof checked,name) || !valid_preset_name(checked))return false;
 auto* before=alloc_new<TrackHistory>(track_const(track));auto* after=before?alloc_new<TrackHistory>(*before):nullptr;if(!before || !after){delete before;delete after;return memory_refused();}capture_track_morph(before->morph,project_,track);after->morph=before->morph;copy_text(after->track_name,96,checked);
 if(!engine_.replace_track(project_.active_pattern,track,*after,true)){delete before;delete after;return false;}Command c{CommandType::ApplyPattern,uint8_t(track),1,project_.active_pattern};c.parameter=uint16_t(1u<<track);push_history(c,c,nullptr,nullptr,before,after);install_single_track(project_.active_pattern,track,*after,false);dirty_=true;return true;
}
auto Performance::confirm_preset_overwrite(const char* path) -> void{copy_text(browser_.edit.source,sizeof browser_.edit.source,path);browser_.edit.operation=BrowserOperation::OverwritePreset;axes_.modal=ModalKind::Confirm;copy_text(browser_.status,sizeof browser_.status,"Name exists / confirm replacement");dirty_=true;}
auto Performance::begin_browser_edit(BrowserOperation operation) -> bool{
  if(mode_!=UiMode::Browser || file_action_pending() || browser_editing())return false;
  BrowserFileEdit next;next.operation=operation;
  if(operation==BrowserOperation::NewFolder){copy_text(next.name,sizeof next.name,"New Folder");}
  else {if(browser_.cursor>=browser_.count)return false;const auto& entry=browser_.entries[browser_.cursor];if(!join_path(next.source,sizeof next.source,browser_.directory,entry.name) || !copy_text(next.name,sizeof next.name,entry.name))return false;next.directory=entry.directory;}
  browser_.edit=next;axes_.modal=operation==BrowserOperation::Delete?ModalKind::Confirm:ModalKind::Rename;dirty_=true;return true;
 }
auto Performance::browser_edit_character(int delta) -> bool{
  auto& edit=browser_.edit;if(browser_confirmation())return true;
  static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
  unsigned length=0;while(edit.name[length])++length;if(edit.cursor>=95)return false;
  unsigned current=0;for(unsigned i=0;alphabet[i];++i)if(alphabet[i]==edit.name[edit.cursor]){current=i;break;}
  unsigned count=sizeof alphabet-1;int n=(int(current)+delta)%int(count);if(n<0)n+=int(count);edit.name[edit.cursor]=alphabet[n];if(edit.cursor==length)edit.name[length+1]=0;dirty_=true;return true;
 }
auto Performance::browser_edit_key(unsigned index) -> bool{
  auto& edit=browser_.edit;unsigned length=0;while(edit.name[length])++length;dirty_=true;
  if(index==1){cancel_browser_edit();return true;}
  if(index==0){if(edit.operation==BrowserOperation::RenameTrack){if(track_name_generation_!=engine_.staged_generation() || !rename_track(track_name_target_,edit.name))return false;cancel_browser_edit();return true;}if(edit.operation!=BrowserOperation::Delete && !browser_name_valid(edit.name)){copy_text(browser_.status,sizeof browser_.status,"Invalid name / no path separators");return false;}FileAction action=edit.operation==BrowserOperation::OverwritePreset?FileAction::OverwritePreset:edit.operation==BrowserOperation::SavePreset?FileAction::SavePreset:edit.operation==BrowserOperation::SaveProject?FileAction::SaveProject:edit.operation==BrowserOperation::Rename?FileAction::RenameFile:edit.operation==BrowserOperation::Delete?FileAction::DeleteFile:FileAction::NewFolder;if(!request_file(action))return false;cancel_browser_edit();return true;}
  if(browser_confirmation())return true;
  if(index==2 || index==3)return browser_edit_character(index==2?-1:1);
  if(index==4 && length<95){for(unsigned n=length+1;n>edit.cursor;--n)edit.name[n]=edit.name[n-1];edit.name[edit.cursor]=' ';}
  if(index==5 && length){unsigned at=edit.cursor<length?edit.cursor:length-1;for(unsigned n=at;n<length;++n)edit.name[n]=edit.name[n+1];if(edit.cursor>at)edit.cursor=at;}
  if(index==6 && edit.cursor)--edit.cursor;
  if(index==7 && edit.cursor<length)++edit.cursor;
  return true;
 }
auto Performance::request_file(FileAction action) -> bool{
  if(action==FileAction::MidiImport)return begin_midi_import();
  if(action==FileAction::SaveKit || (action==FileAction::SavePreset && browser_.edit.operation!=BrowserOperation::SavePreset))return begin_preset_save();
  if(action==FileAction::AudioRecord || action==FileAction::AudioOverdub){if(live_recording_)return request_file(FileAction::RecordToggle);if(file_action_pending() || recording_file_busy_ || !playing_ || !audio_available_ || audio_lane().kind==TrackKind::Instrument)return false;queued_take_={current_recording_target(),audio_lane().source,true,RecordingOrigin::Source};queued_audio_revision_=audio_revision_[selected_];queued_audio_clip_=audio_clip_cursor_;file_action_=action;dirty_=true;return true;}
  if(action==FileAction::CancelResample){if(!resampling_ || resample_cancel_pending_ || resample_cancel_request_)return false;resample_cancel_request_=true;dirty_=true;return true;}
  if((action==FileAction::RecordToggle || action==FileAction::SampleRecordToggle) && live_recording_){if(resampling_)return request_file(FileAction::CancelResample);if(record_stop_pending_ || record_stop_request_)return false;record_stop_request_=true;dirty_=true;return true;}
  if(file_action_!=FileAction::None)return false;
  if(action==FileAction::Resample){if(!resample_start_allowed(recording_controls()))return false;queued_resample_={current_recording_target(),uint8_t(capture_source()),resample_bars_};}
  if(action==FileAction::Capture || action==FileAction::Overdub){queued_take_={current_recording_target(),uint8_t(capture_source()),record_quantized_,action==FileAction::Overdub?RecordingOrigin::Overdub:RecordingOrigin::Source};}
  if(action==FileAction::RecordToggle || action==FileAction::SampleRecordToggle){if(!recording_start_allowed(recording_controls()))return false;queued_record_={current_recording_target(),uint8_t(action==FileAction::SampleRecordToggle?1:capture_source()),record_quantized_,action==FileAction::SampleRecordToggle?RecordingOrigin::Input:RecordingOrigin::Source};}
  file_action_=action;dirty_=true;return true;
 }
auto Performance::pending_file_action()const -> FileAction{return resample_cancel_request_?FileAction::CancelResample:record_stop_request_?FileAction::RecordToggle:file_action_!=FileAction::None?file_action_:browser_refresh_pending_?(mode_==UiMode::Library || browser_.library_resume?FileAction::LibraryRestore:source_select_?FileAction::LibraryInspect:FileAction::Refresh):FileAction::None;}
auto Performance::take_file_action() -> FileAction{if(resample_cancel_request_){resample_cancel_request_=false;return FileAction::CancelResample;}if(record_stop_request_){record_stop_request_=false;return FileAction::RecordToggle;}auto a=file_action_;file_action_=FileAction::None;if(a==FileAction::None && browser_refresh_pending_){browser_refresh_pending_=false;const auto refresh=mode_==UiMode::Library || browser_.library_resume?FileAction::LibraryRestore:source_select_?FileAction::LibraryInspect:FileAction::Refresh;browser_.library_resume=false;return refresh;}if(a==FileAction::Refresh)browser_refresh_pending_=false;return a;}
auto Performance::browser_controls()const -> BrowserContext{bool part=browser_.cursor<browser_.count && !browser_.entries[browser_.cursor].directory && is_recording_part(browser_.entries[browser_.cursor].name);bool mf=browser_.cursor<browser_.count && is_midi_file(browser_.entries[browser_.cursor].name);return {shifted(),midi_browser(),browser_.previewing,other_preview_target(),part,mf,browser_.new_project_list};}
auto Performance::set_step_sample_lock(unsigned step,uint16_t id,int value) -> bool{if(step>=kSteps || (id!=300 && id!=301 && id!=302))return false;if(value>=0)return set_lock(step,id,float(value));const auto& old=track_const(selected_).steps[step];auto next=old;bool found=false,any=false;for(auto& lock:next.locks){if(lock.enabled && lock.id==lock_target(selected_step_,id)){lock.enabled=false;found=true;}any|=lock.enabled;}if(!found)return true;if(!any)next.lock_only=false;Command after{CommandType::SetRecordedStep,selected_,uint8_t(step),0},before=after;after.recorded_step=next;before.recorded_step=old;return edit(after,before);}
auto Performance::set_sound_override(unsigned slot) -> bool{if(slot>=kPresetSlots && slot!=65535)return false;Command after{CommandType::SetSoundOverride,selected_,0,uint16_t(slot)},before=after;before.value=track_const(selected_).sound_override;return edit(after,before);}
auto Performance::set_sound_lock(unsigned step,unsigned sound) -> bool{if(step>=kSteps || (sound>=kPresetSlots && sound!=kNoPreset))return false;Command after{CommandType::SetSoundLock,selected_,uint8_t(step),uint16_t(sound)},before=after;before.value=track(selected_).steps[step].sound;return edit(after,before);}
auto Performance::sample_favorite(const char* path)const -> bool{if(!path || !path[0])return false;bool known=false;uint32_t crc=0;for(const auto& a:project_.assets)if(a.path[0] && !__builtin_strcmp(a.path,path)){known=true;crc=a.metadata.content_crc;break;}for(const auto& a:project_.assets)if(a.path[0] && (!__builtin_strcmp(a.path,path) || (known && a.metadata.content_crc==crc)) && a.metadata.favorite)return true;return false;}
auto Performance::set_sample_favorite(const char* path,bool enabled) -> bool{
 if(!path || !path[0])return false;
 bool known=false;uint32_t crc=0;for(const auto& a:project_.assets)if(a.path[0] && !__builtin_strcmp(a.path,path)){known=true;crc=a.metadata.content_crc;break;}
 auto match=[&](const SampleAssetState& a){return a.path[0] && (!__builtin_strcmp(a.path,path) || (known && a.metadata.content_crc==crc));};unsigned count=0,found=0;for(const auto& a:project_.assets)if(match(a)){++found;count+=a.metadata.favorite!=enabled;}
 if(!found || engine_.command_space()<count)return false;
 EditGroup edit_group(*this);bool ok=true;for(unsigned i=0;i<kSampleAssets && ok;++i)if(match(project_.assets[i]) && project_.assets[i].metadata.favorite!=enabled){Command after{CommandType::SetAssetFavorite,0,uint8_t(enabled),uint16_t(i)},before=after;before.step=uint8_t(project_.assets[i].metadata.favorite);ok=edit(after,before);}return edit_group.finish(ok);
}
auto Performance::take_asset_clone(unsigned& to,unsigned& from) -> bool{if(!asset_clone_pending_)return false;to=unsigned(__builtin_ctzll(asset_clone_pending_));asset_clone_pending_&=asset_clone_pending_-1;from=asset_clone_alias_[to];return true;}
auto Performance::mirror_track_install(const TrackInstall& install,unsigned track) -> void{
 install_track_resources(project_,install,uint16_t(1u<<track));
 for(unsigned i=0;i<install.clones.count;++i){const auto& clone=install.clones.rows[i];asset_clone_alias_[clone.to]=(asset_clone_pending_&(uint64_t(1)<<clone.from))?asset_clone_alias_[clone.from]:clone.from;asset_clone_pending_|=uint64_t(1)<<clone.to;}
}
auto Performance::asset_playback(unsigned slot,unsigned root,unsigned bpm) -> bool{
 if(slot>=kSampleAssets || !project_.assets[slot].path[0] || root>127 || bpm>300)return false;
 const auto& old=project_.assets[slot].metadata;auto m=old;m.root=uint8_t(root);m.bpm=uint16_t(bpm);bool playback=old.root!=m.root || old.bpm!=m.bpm;
 const uint64_t bit=uint64_t(1)<<slot;
 if(!playback)return true;
 if(!(project_asset_mask(project_,selected_)&bit)){
  Command after{CommandType::SetAssetPlayback,0,0,uint16_t(slot)},before=after;after.metadata=m;before.metadata=old;return edit(after,before);
 }
 // Presets, other tracks and phrase/audio references own their existing metadata.
 // Editing the current working sound forks its asset binding atomically.
 const auto current=track_const(selected_);const uint64_t needed=sound_asset_mask(current);
 if(!(needed&bit)){copy_text(browser_.status,96,"Material belongs to another sound / kept");notify(UiNotice::Failed);return false;}
 auto* before=alloc_new<TrackHistory>(current);auto* after=before?alloc_new<TrackHistory>(*before):nullptr;auto* sound=alloc_new<SoundState>();
 if(!before || !after || !sound){delete before;delete after;delete sound;return memory_refused();}
 snapshot_sound(*sound,current);before->install.annotations=after->install.annotations=true;capture_track_morph(before->morph,project_,selected_);after->morph=before->morph;
 auto* incoming=alloc_array<SampleAssetState>(kSampleAssets);if(!incoming){delete sound;delete after;delete before;return memory_refused();}for(unsigned i=0;i<kSampleAssets;++i)incoming[i]=project_.assets[i];incoming[slot].metadata=m;MaterialMapping mapping;bool planned=plan_material_mapping(project_,*sound,incoming,mapping);
 if(!planned){delete[] incoming;delete sound;delete after;delete before;copy_text(browser_.status,96,"No free material binding / original kept");notify(UiNotice::Failed);return false;}
 for(unsigned source=0;source<kSampleAssets;++source)if((mapping.needed&(uint64_t(1)<<source)) && !same_material_binding(project_.assets[mapping.slots[source]],incoming[source])){auto& clone=after->install.clones.rows[after->install.clones.count++];clone.from=uint8_t(source);clone.to=mapping.slots[source];clone.asset=incoming[source];}
 remap_sound_assets(*sound,mapping.slots);mount_sound(TrackView(*after),*sound);delete[] incoming;delete sound;
 bool ok=engine_.replace_track(project_.active_pattern,selected_,*after,true,nullptr,nullptr,nullptr,&after->morph,&after->install);
 if(ok){Command c{CommandType::ApplyPattern,selected_,1,project_.active_pattern};c.parameter=uint16_t(1u<<selected_);push_history(c,c,nullptr,nullptr,before,after);install_single_track(project_.active_pattern,selected_,*after,false);mirror_track_install(after->install,selected_);dirty_=true;}
 else{delete before;delete after;}return ok;
}

auto Performance::sample_edit(const SampleEdit& e) -> bool{if(!valid_sample_edit(e))return false;const auto& t=track_const(selected_);if(t.instrument==Instrument::DrumKit){auto pad=t.kit.pads[selected_kit_pad_];pad.edit=e;return set_kit_pad(selected_kit_pad_,pad);}Command after{CommandType::SetSampleEdit,selected_,0,0},before=after;after.sample_edit=e;before.sample_edit=t.sample_edit;after.parameter=before.parameter=1;before.step=t.sample_slice;before.value=t.sample_slices;after.value=e.slice_count?e.slice_count:t.sample_slices;after.step=uint8_t(t.sample_slice<after.value?t.sample_slice:after.value-1);return edit(after,before);}
auto Performance::set_sample_region(unsigned slice,unsigned slices,bool loop,bool reverse) -> bool{if(!slices || slices>64 || slice>=slices)return false;const auto& t=track(selected_);Command after{CommandType::SetSampleRegion,selected_,uint8_t(slice),uint16_t(slices)},before{CommandType::SetSampleRegion,selected_,t.sample_slice,t.sample_slices};after.parameter=uint16_t(loop)|(uint16_t(reverse)<<1);before.parameter=uint16_t(t.sample_loop)|(uint16_t(t.sample_reverse)<<1);return edit(after,before);}
auto Performance::project_loaded()  -> void{twin_invalidate();for(auto& pending:twin_switch_pending_)pending=false;asset_clone_pending_=0;for(unsigned i=0;i<kTracks;++i){newer_preset_revision[i]=0;embedded_preset_fallback[i]=false;}browser_.new_project_list=false;browser_.analysis_source=nullptr;browser_.analysis_pending_source=nullptr;browser_.analysis_pending=false;step_chord_edit_=false;analysis_valid_=false;analysis_due_ms_=analysis_requested_sample_=0;analysis_backoff_=0;modal_.owner=ModalOwner::None;modal_.value=0;modal_.sync();midi_import_request_={};midi_import_ticket_=midi_import_cancel_ticket_=0;axes_.history.clear();track_peek_slot_=255;track_peek_active_=false;track_bank_=0;step_down_=false;page_left_down_=page_right_down_=page_home_fired_=false;bake_stage_=BakeStage::Off;bake_path_[0]=0;for(unsigned t=0;t<16;++t){audio_revision_[t]=0;audio_undo_valid_[t]=false;}audio_clip_cursor_=0;strip_active_=strip_note_active_=strip_release_pending_=false;arrangement_ui_active_=false;arrangement_ui_mute_=arrangement_ui_solo_=0;resampling_=resample_cancel_pending_=resample_cancel_request_=recording_file_busy_=record_stop_pending_=record_stop_request_=false;queued_record_=LiveRecordRequest{};active_record_origin_=RecordingOrigin::None;queued_resample_=active_resample_=ResampleRequest{};clip_follow_page_=false;scene_take_requested_=false;scene_take_message_[0]=0;cancel_browser_edit();ticket_valid_=false;page_left_pending_=page_right_pending_=false;encoder_edit_=false;main_encoder_mode_=0;note_edit_=false;follow_display_=false;update_confirm_=false;save_requested_=false;++favorite_revision_;sn_ref_armed_=false;recovery_warning_=false;clear_record_ids();for(unsigned i=0;i<128;++i)rotation_revisions_[i]={};rotation_cursor_=0;loop_confirm_=false;browser_.previewing=false;browser_.audition_path[0]=0;settings_.theme=device_.theme;settings_.motion=ui::Motion(project_.ui.motion);settings_.pixel_order=project_.ui.order;settings_.strip=ui::StripProfile(project_.ui.strip?project_.ui.strip:1);project_.ui.strip=uint8_t(settings_.strip);settings_.preview565=project_.ui.preview565;gesture_recording_=gesture_playing_=note_recording_=note_finish_pending_=counting_in_=false;count_beat_=0;repeat_mode_=0;repeat_rate_=project_.ui.repeat_rate;song_position_=0;seek_ready_=false;repeat_latch_=repeat_ignore_release_=false;repeat_release_pending_=false;performance_page_=0;fx_page_=0;chain_node_=0;song_cursor_=0;song_import_armed_=song_clear_armed_=false;playing_=false;pattern_copied_=false;copy_track_only_=clipboard_track_only_=false;step_copied_=false;extend_pending_=false;delete pattern_clipboard_;pattern_clipboard_=nullptr;pending_project_=project_action_=ProjectAction::None;clear_history();instrument_view_=0;source_select_=false;source_arm_unknown_=false;visual_=AudioVisualFrame{};queued_scene_=-1;queued_stops_=0;fill_active_=stutter_active_=false;for(auto& t:trigger_seen_)t=false; generation_=engine_.staged_generation(); selected_=selected_kit_pad_=page_=selected_step_=gen_page_=gen_lane_=gen_cell_=midi_input_cursor_=0;following_clip_=project_.active_pattern==project_.clips[0].pattern; history_cursor_=history_size_=0;release_stutter_=release_fill_=release_control_all_=false;control_all_=false;control_all_position_=0.5f;held_stutter_pad_=held_fill_pad_=255;performance_fx_mode_=PerformanceFx::Off;performance_fx_latched_=false;held_performance_pad_=255;release_performance_fx_=false; for(unsigned t=0;t<16;++t) { pending_[t]=-1; positions_[t]=0;notes_held_[t]=pending_release_[t]=held_[t]=false;pressures_[t]=0;wave_ref_[t]=1; } refresh_steps(); dirty_=true; }
auto Performance::assign_sample(unsigned slot,WavView view,const SampleMetadata* metadata,unsigned destination,unsigned pattern,unsigned kit_pad)  -> bool{unsigned target=destination<16?destination:selected_;unsigned target_pattern=pattern<kPatterns?pattern:project_.active_pattern;if(slot>=kSampleAssets)return false;if(track_views_.at(target_pattern,target).instrument==Instrument::DrumKit){unsigned pad_index=kit_pad<16?kit_pad:selected_kit_pad_;auto pad=track_views_.at(target_pattern,target).kit.pads[pad_index];pad.sample[0]=uint8_t(slot);return set_kit_pad_at(target,target_pattern,pad_index,pad,view,metadata);}const auto old=track_views_.at(target_pattern,target);Command after{CommandType::SetSample,uint8_t(target),0,uint16_t(slot)},before{CommandType::RestoreSampleAssignment,uint8_t(target),uint8_t(old.instrument),old.sample_slot};if(old.instrument==Instrument::Granular)after.step=uint8_t(Instrument::Granular);after.pattern=before.pattern=uint8_t(target_pattern);after.sample=view;after.parameter=1;after.metadata=metadata?*metadata:project_.assets[slot].metadata;after.metadata.rate=view.rate;after.metadata.frames=view.frames;after.metadata.bits=view.bits;after.metadata.channels=uint8_t(view.channels);return edit(after,before); }
auto Performance::set_frozen_asset(unsigned track,unsigned pattern,unsigned slot,const WavView* bindings,const SampleAssetState* assets) -> bool{
  if(track>=16 || pattern>=kPatterns || (slot>=kSampleAssets && slot!=255) || bool(bindings)!=bool(assets))return false;
  auto* before=new PatternState(project_,pattern);auto* after=new PatternState(*before);
  after->tracks[track].frozen_asset=uint8_t(slot);
  uint16_t mask=uint16_t(1u<<track);
  if(!engine_.replace_pattern(pattern,*after,mask,true,nullptr,bindings,assets)){delete before;delete after;return false;}
  Command c{CommandType::ApplyPattern,0,1,uint16_t(pattern)};c.parameter=mask;push_history(c,c,before,after);install_pattern(project_,pattern,*after,mask);
  if(assets)for(unsigned i=0;i<kSampleAssets;++i)project_.assets[i]=assets[i];
  dirty_=true;return true;
 }
auto Performance::apply_sound(const SoundState& sound) -> bool{if(!engine_.preset_assets_ready(sound) || !valid_sound(sound) || sound.instrument==Instrument::Empty || sound.instrument==Instrument::Unknown)return false;Command after{CommandType::ApplySound,selected_,0,0},before=after;after.sound()=sound;snapshot_sound(before.sound(),track_const(selected_));return edit(after,before);}
auto Performance::store_sound(unsigned slot) -> bool{
 if(slot>=kPresetSlots || project_.sounds[slot].instrument==Instrument::Unknown)return false;
 Command after{CommandType::SetSound,0,0,uint16_t(slot)},before=after;snapshot_sound(after.sound(),track_const(selected_));
 const auto source=after.sound().identity;if(source.assigned())for(unsigned i=0;i<kPresetSlots;++i)if(same_preset_revision(source,project_.sounds[i].identity) && after.sound().definition_cookie==project_.sounds[i].definition_cookie && same_preset_content(after.sound(),project_.assets,project_.sounds[i],project_.assets)){sound_slot_=uint16_t(i);dirty_=true;return true;}
 if(project_.sounds[slot].instrument!=Instrument::Empty && preset_slot_referenced(project_,slot)){copy_text(browser_.status,96,"Preset slot is in use / choose an empty slot");notify(UiNotice::Failed);return false;}
 if(preset_guids_.ready() && !preset_guids_.issue(after.sound().identity,&source))return false;
 before.sound()=project_.sounds[slot];return edit(after,before);
}
auto Performance::prepare_preset_references() -> bool{
 for(unsigned t=0;t<16;++t)if(project_.twins[t].prepared())project_.twins[t].active=project_.tracks[t].instrument==Instrument::Sampler && project_.tracks[t].preset_slot==project_.twins[t].sampled;
 unsigned needed=0;for(const auto& sound:project_.sounds)needed+=sound.instrument!=Instrument::Empty && sound.instrument!=Instrument::Unknown && !sound.identity.assigned();
 if(needed>engine_.sound_command_space() || needed>engine_.command_space())return false;
 if(needed && !preset_guids_.ready())return false;
 auto* copy=needed?alloc_new<SoundState>():nullptr;if(needed && !copy)return memory_refused();
 for(unsigned slot=0;slot<kPresetSlots;++slot){const auto& sound=project_.sounds[slot];if(sound.instrument==Instrument::Empty || sound.instrument==Instrument::Unknown || sound.identity.assigned())continue;*copy=sound;if(!preset_guids_.issue(copy->identity)){delete copy;return false;}Command c{CommandType::SetSound,0,0,uint16_t(slot)};c.pattern=project_.active_pattern;c.sound()=*copy;if(!send(c)){delete copy;return false;}mirror(c);}
 delete copy;
 for(unsigned t=0;t<kTracks;++t){auto& track=project_.tracks[t];if(!track.preset_identity.assigned())continue;unsigned slot=kPresetSlots;for(unsigned i=0;i<kPresetSlots;++i)if(same_preset_revision(track.preset_identity,project_.sounds[i].identity)){slot=i;break;}if(slot==kPresetSlots){notify(UiNotice::Failed);return false;}track.preset_slot=uint16_t(slot);}
 return true;
}

auto Performance::swap_kit_pad_sounds(unsigned first,unsigned second) -> bool{
  if(first>=16 || second>=16 || track_const(selected_).instrument!=Instrument::DrumKit)return false;
  if(first==second)return select_kit_pad(second);
  if(engine_.command_space()<2){notify(UiNotice::Failed);return false;}
  const auto a=track_const(selected_).kit.pads[first],b=track_const(selected_).kit.pads[second];EditGroup edit_group(*this);bool ok=set_kit_pad(first,b) && set_kit_pad(second,a);if(!ok){notify(UiNotice::Failed);return false;}select_kit_pad(second);notify(UiNotice::KitPadSwapped);return edit_group.finish(true);
 }
auto Performance::map_kit_slices() -> bool{
  const auto& source=track_const(selected_);if(source.instrument!=Instrument::DrumKit)return false;const auto pad=source.kit.pads[selected_kit_pad_];if(pad.sample[0]>=kSampleAssets || !project_.assets[pad.sample[0]].path[0] || engine_.command_space()<16)return false;
  unsigned slices=pad.edit.slice_count?pad.edit.slice_count:16;if(slices>16)slices=16;EditGroup edit_group(*this);
  for(unsigned i=0;i<16;++i){auto mapped=pad;if(i<slices){float first=pad.edit.slice_count?pad.edit.markers[i]:float(i)/slices,last=pad.edit.slice_count?pad.edit.markers[i+1]:float(i+1)/slices;mapped.edit.start=pad.edit.start+(pad.edit.end-pad.edit.start)*first;mapped.edit.end=pad.edit.start+(pad.edit.end-pad.edit.start)*last;}else mapped.sample[0]=mapped.sample[1]=255;mapped.edit.slice_count=0;mapped.edit.zone_count=0;if(!set_kit_pad(i,mapped)){return false;}}
  return edit_group.finish(true);
 }
auto Performance::register_saved_preset(unsigned track,ConstTrackView saved,const PresetRegistration& registration) -> bool{
 if(track>=kTracks || registration.slot>=kPresetSlots || !valid_sound(registration.sound))return false;
 Command command{CommandType::RegisterPreset,uint8_t(track),0,registration.slot};command.pattern=project_.active_pattern;command.sound()=registration.sound;if(!send(command))return false;mirror(command);
 auto& target=project_.tracks[track];target.preset_identity=saved.preset_identity;target.preset_slot=registration.slot;target.preset_fx=saved.preset_fx;target.output_trim=saved.output_trim;copy_text(target.preset_name,96,saved.preset_name);dirty_=true;return true;
}
auto Performance::apply_preset_track(ConstTrackView source,const Command* sample_binding,const WavView* bindings,const SampleAssetState* assets,const PresetFxSnapshot* fx,const PresetRegistration* preset) -> bool{
  if(fx && !fx->valid())return false;
  if(!allocation_admitted(sizeof(TrackHistory),2))return memory_refused();
  auto* previous=alloc_new<TrackHistory>(track_const(selected_));auto* next=previous?alloc_new<TrackHistory>(*previous):nullptr;
  if(!previous || !next){delete previous;delete next;return memory_refused();}
  capture_track_morph(previous->morph,project_,selected_);
  copy_engine_instance(*next,source);if(fx){next->preset_fx=*fx;fx->apply(TrackView(*next));}if(preset){if(preset->slot>=kPresetSlots || !valid_sound(preset->sound)){delete previous;delete next;return false;}next->install.preset=*preset;next->install.lane_change=true;next->install.lane_kind=TrackKind::Instrument;previous->install.lane_change=true;previous->install.lane_kind=project_.audio[selected_].kind;previous->install.preset.slot=preset->slot;previous->install.preset.sound=project_.sounds[preset->slot];next->preset_slot=preset->slot;}
  bool ok=engine_.replace_track(project_.active_pattern,selected_,*next,false,sample_binding,bindings,assets,nullptr,preset?&next->install:nullptr);
  if(ok){Command c{CommandType::ApplyPattern,selected_,0,project_.active_pattern};c.parameter=uint16_t(1u<<selected_);c.frames=sample_binding?1:0;
   push_history(c,c,nullptr,nullptr,previous,next);install_single_track(project_.active_pattern,selected_,*next,c.frames!=0);capture_track_morph(next->morph,project_,selected_);
   if(preset){mirror_track_install(next->install,selected_);}
   if(assets){for(unsigned i=0;i<kSampleAssets;++i)project_.assets[i]=assets[i];}refresh_steps();visual_.layer_failure[selected_]=0;if(notice()==UiNotice::LayerRefused)notify(UiNotice::None);if(next->instrument==Instrument::Layer){auto reason=layer_warning(false);if(reason!=LayerReason::Ready){notice_layer_reason_=reason;notify(UiNotice::LayerRefused);}}
  }else{delete next;delete previous;}return ok;
 }
auto Performance::take_save_request()  -> bool{
  if(!save_requested_)return false;
  if(engine_.project_event_loss()){save_requested_=false;notify(UiNotice::ProjectUnsynced);return false;}
  if(engine_.project_events_pending()){notify(UiNotice::SaveQueued);return false;}
  save_requested_=false;return true;
 }
}

namespace groove {
auto Performance::twin_status(unsigned t)const -> TwinStatus{
 if(t>=16){t=selected_;}
 const auto& pair=project_.twins[t];if(!pair.prepared())return TwinStatus::Empty;
 if(pair.active)return project_.tracks[t].instrument==Instrument::Sampler && project_.tracks[t].preset_slot==pair.sampled?TwinStatus::Active:TwinStatus::Stale;
 if(twin_cached_track_==t && twin_checked_edit_==edit_serial_ && now_ms_<twin_checked_ms_+200)return twin_cached_;
 twin_cached_track_=t;twin_checked_edit_=edit_serial_;twin_checked_ms_=now_ms_;twin_cached_=TwinStatus::Stale;
 const auto& sample=project_.sounds[pair.sampled];if(sample.instrument!=Instrument::Sampler || sample.sample_slot>=kSampleAssets || project_.assets[sample.sample_slot].metadata.missing || !project_.assets[sample.sample_slot].path[0])return twin_cached_;
 auto* sound=alloc_new<SoundState>();uint8_t digest[32];if(sound){snapshot_sound(*sound,track_views_.at(project_.active_pattern,t));if(twin_content_digest(*sound,project_,t,digest) && !__builtin_memcmp(digest,pair.digest,32))twin_cached_=TwinStatus::Ready;delete sound;}return twin_cached_;
}
auto Performance::twin_setup() -> bool{if(modal_active() || file_action_pending() || recording_file_busy_)return false;twin_modal_=1;dirty_=true;return true;}
auto Performance::twin_button() -> bool{
 if(modal_active() || file_action_pending() || twin_switch_pending_[selected_] || audio_lane().kind!=TrackKind::Instrument)return false;
 if(shifted())return twin_setup();
 auto status=twin_status();if(status==TwinStatus::Empty || status==TwinStatus::Stale)return request_file(FileAction::BeginTwin);
 if(!send({CommandType::ToggleTwin,selected_,0,0})){return false;}
 twin_switch_pending_[selected_]=true;dirty_=true;return true;
}
}
