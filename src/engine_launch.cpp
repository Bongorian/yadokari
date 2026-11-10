#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
void Engine::play_step(unsigned t) {
 RenderEventScope audit(render_audit_,event_work_,RenderAudit::StepWork);
 if(t>=16)return;
 auto& r=runtime_[t];const auto& state=track(t);if(state.frozen_asset<kSampleAssets){if(r.position==0)trigger_frozen(t);refresh_parameters_step(t,nullptr);return;}auto& step=r.effective_step;step=mutation_step(state,r.position);if(project_->twins[t].active)step.sound=kNoPreset;r.retriggers=0;r.automation_live=playing_ && r.playing;r.automation_cell=r.automation_tick++;r.last_step=r.position;r.last_grid=r.grid;r.last_period_q32=step_period(t);
 ++r.step_serial;bool replacing=state.instrument!=Instrument::DrumKit && note_record_armed_ && project_->ui.note_replace && t==note_record_track_ && erase_record_step(t,r.position,r.step_serial);event(EngineEventType::Step,uint8_t(t),r.position);bool fire=r.playing && !audio_muted(t) && (step.enabled || step.lock_only) && !replacing;bool solo=audio_has_solo();if(solo && !audio_solo_track(t))fire=false;
 if(state.instrument==Instrument::DrumKit){
  bool audible=r.playing && !audio_muted(t) && (!solo || audio_solo_track(t));
  uint64_t period=step_period(t)>>32;if(!period)period=1;kit_repeat_pads_[t]=0;kit_repeat_tracks_&=uint16_t(~(1u<<t));
  for(unsigned pad=0;pad<16;++pad){auto& repeat=kit_repeat_[t][pad];repeat.remaining=0;const auto& ks=state.kit.steps[pad][r.position];bool pad_audible=audible && !state.kit.pads[pad].muted && (!state.kit.solo_mask || (state.kit.solo_mask&(1u<<pad)));
   if(kit_early_planned_[t]&(1u<<pad)){auto& early=kit_early_[t][pad];if(pad_audible && ks.enabled && (kit_early_pads_[t]&(1u<<pad)) && early.remaining && early.step==r.position){repeat=early;kit_repeat_pads_[t]|=uint16_t(1u<<pad);}continue;}
   if(!pad_audible || !ks.enabled || !ks.velocity || !ks.probability)continue;
   if(step_probability_draw(project_->random_seed,t*16+pad,r.pattern,r.automation_cell)>=ks.probability)continue;
   repeat.step=r.position;repeat.velocity=ks.velocity;repeat.remaining=ks.ratchet;repeat.period=period/ks.ratchet;if(!repeat.period)repeat.period=1;
   repeat.due=sample_count_+(ks.microtiming>0?uint64_t(ks.microtiming)*period/96:0);kit_repeat_pads_[t]|=uint16_t(1u<<pad);
  }
  if(kit_repeat_pads_[t])kit_repeat_tracks_|=uint16_t(1u<<t);
  clear_kit_early(t);
  // The kit's pad lanes have their own sample locks. Track and FX locks
  // remain on the shared step grid and apply even on a silent pad step.
  if(audible)refresh_parameters_step(t,&step);
  else refresh_parameters_step(t,nullptr);
  return;
 }
 fire=fire && step_condition_passes(step,r.cycle,fill_);
 if(fire)fire=step_probability_draw(project_->random_seed,t,r.pattern,r.automation_cell)<step.probability;
 if(!fire){if(voices_[t].tie_pending)note_off_main(t);refresh_parameters_step(t,nullptr);return;}
 // Parameter-only triggers share the ordinary grid and conditions, but never
 // start, release, or retrigger a note and do not advance melodic lanes.
 if(!step.enabled){if(voices_[t].tie_pending)note_off_main(t);refresh_parameters_step(t,&step);return;}
 if(state.lanes_enabled){step.note=state.lanes[0].values[lane_cell(state.lanes[0],r.lane_triggers)];step.velocity=state.lanes[1].values[lane_cell(state.lanes[1],r.lane_triggers)];step.gate=state.lanes[2].values[lane_cell(state.lanes[2],r.lane_triggers)];++r.lane_triggers;}
 const auto& a=state.accumulator;uint32_t cycle=a.reset?r.cycle%a.reset:r.cycle;
 // Use wide intermediates for indefinitely running performances.
 int64_t velocity=int64_t(step.velocity)+int64_t(cycle)*a.velocity+groove_velocity(state.groove,r.position)*state.groove.amount/100;
 step.velocity=uint8_t(velocity<0?0:velocity>127?127:velocity);
 int64_t gate=int64_t(step.gate)+int64_t(cycle)*a.gate;step.gate=uint16_t(gate<1?1:gate>384?384:gate);
 r.accumulator_offset=int16_t(accumulated_pitch(a,r.cycle));r.accumulator_degrees=a.scale_degrees;
 refresh_parameters_step(t,&step);

 int note=int(state.root_note)+int(step.note)+int(song_transpose_)-60; if(note<0)note=0; if(note>127)note=127; r.retrigger_period=step_period(t);r.retrigger_count=uint8_t(step.ratchet*(state.arp_mode?(1u<<state.arp_rate):1));if(state.arp_mode)trigger_step_arp(t,step.velocity,note,step,r.accumulator_offset,a.scale_degrees,0);else trigger_step_chord(t,step.velocity,note,step,r.accumulator_offset,a.scale_degrees);
 r.retrigger_note=int16_t(note); r.retriggers=uint8_t(r.retrigger_count-1); r.retrigger_velocity=step.velocity; r.retrigger_origin=int64_t(sample_count_);r.retrigger_fraction=0;r.retrigger_index=1;schedule_retrigger(t);
}
bool Engine::mount_override(unsigned t,unsigned slot,bool twin){
 if(project_->twins[t].active && !twin)return false;
 {
 RenderEventScope audit(render_audit_,event_work_,RenderAudit::MountValidate);
 if(slot>=kPresetSlots){return false;}const auto& sound=project_->sounds[slot];
 if(sound.instrument==Instrument::Empty || sound.instrument==Instrument::Unknown || !preset_assets_ready(sound))return false;
 if(sound.instrument==Instrument::SoundFont && !soundfont_name(samples_[sound.sample_slot].soundfont,unsigned(sound.soundfont.values[0]),unsigned(sound.soundfont.values[1])))return false;
 if(sound.instrument==Instrument::Layer)for(unsigned i=0;i<sound.layer.count;++i){const auto& child=sound.layer.parts[i].sound;if(child.instrument==Instrument::SoundFont && !soundfont_name(samples_[child.sample_slot].soundfont,unsigned(child.soundfont.values[0]),unsigned(child.soundfont.values[1])))return false;}
 if(sound.instrument==Instrument::Sampler && (sound.sample_slot>=kSampleAssets || !samples_[sound.sample_slot].frames || project_->assets[sound.sample_slot].metadata.missing))return false;
 if(sound.instrument==Instrument::DrumKit)for(const auto& pad:sound.kit.pads)for(auto slot:pad.sample)if(slot<kSampleAssets && (!samples_[slot].frames || project_->assets[slot].metadata.missing))return false;
 if(!events_.free_space() || !attached_payload_ || !attached_payload_->free_space()){__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);return false;}
 }
 const auto& sound=project_->sounds[slot];
 {RenderEventScope audit(render_audit_,event_work_,RenderAudit::MountPublish);if(!attached_payload_->push(sound,slot,sound_payload_epoch_)){__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);return false;}}
 PresetFxSnapshot strip;
 if(twin){strip.capture(track(t));collect_twin_held(t);twin_tail_l_[t]=twin_last_l_[t];twin_tail_r_[t]=twin_last_r_[t];twin_fade_[t]=64;stop_twin_voice(t);}
 {RenderEventScope audit(render_audit_,event_work_,RenderAudit::MountCopy);auto previous=track(t).instrument;retire_sampler_source(t);mount_sound(track(t),sound);track(t).preset_slot=uint16_t(slot);if(twin)strip.apply(track(t));else if(previous!=sound.instrument)reset_track_morph(*project_,t);}
 if(twin)retrigger_twin_held(t);
 event(EngineEventType::SoundAttached,uint8_t(t),uint8_t(twin && !project_->twins[t].active),uint8_t(twin),uint16_t(slot));return true;
}
void Engine::launch_clip(unsigned t,unsigned pattern,bool playing) {
 if(twin_isolated_ && t==freeze_track_){freeze_state_=4;event(EngineEventType::ClipLaunchRejected,uint8_t(t),uint8_t(pattern));return;}
 sound_payload_plan_dirty_=true;
 if(preset_preview_track_==t)stop_preset_preview();
 if(t>=16 || pattern>=kPatterns) return;
 if(audio_lane_mask_&(1u<<t))return;
 unsigned override=project_->twins[t].active?kNoPreset:track_views_.at(pattern,t).sound_override;
 if(playing && override!=kNoPreset && !mount_override(t,override)){event(EngineEventType::ClipLaunchRejected,uint8_t(t),uint8_t(pattern));return;}
 RenderEventScope audit(render_audit_,event_work_,RenderAudit::ClipReset);
 arrangement_restarts_|=uint16_t(1u<<t);arrangement_dirty_=true;
 note_off(t); runtime_[t].pressure_from_pad=false;runtime_[t].pattern=uint8_t(pattern); runtime_[t].playing=playing; project_->clips[t]={uint8_t(pattern),playing}; reset_track(t,true);
 event(EngineEventType::ClipLaunched,uint8_t(t),uint8_t(pattern),uint8_t(playing));
}
void Engine::launch_scene(unsigned scene) {
 if(scene>=kScenes) return;
 const auto& state=project_->scenes[scene];
 for(unsigned t=0;t<16;++t) launch_clip(t,state.pattern_for_track[t],!(state.mute_mask&(1u<<t)));
 project_->active_scene=uint8_t(scene); event(EngineEventType::SceneLaunched,0,uint8_t(scene));
}
uint16_t Engine::captured_mute_mask()const{uint16_t mask=0;for(unsigned t=0;t<kTracks;++t)if(track_muted(*project_,t) || (project_->solo_tracks && !track_solo(*project_,t)))mask|=uint16_t(1u<<t);return mask;}
void Engine::apply_song_section(unsigned index,bool force_restart){
 sound_payload_plan_dirty_=true;
 const auto& song=project_->song;if(index>=song.length)return;const auto& section=song.sections[index];uint16_t previous_mute=song_snapshot_?song_mute_mask_:captured_mute_mask();
 song_snapshot_=section.ticks!=0;snapshot_solo_=0;song_mute_mask_=section.mute_mask;song_transpose_=section.transpose;
 for(unsigned t=0;t<16;++t){unsigned pattern=section.ticks?section.patterns[t]:project_->scenes[section.scene].pattern_for_track[t];bool playing=section.ticks?bool(section.playing_mask&(1u<<t)):!(project_->scenes[section.scene].mute_mask&(1u<<t)) && !(section.mute_mask&(1u<<t));
  if(!section.ticks || force_restart || (section.restart_mask&(1u<<t)) || runtime_[t].pattern!=pattern || runtime_[t].playing!=playing)launch_clip(t,pattern,playing);
  if(section.ticks && !(previous_mute&(1u<<t)) && (section.mute_mask&(1u<<t))){runtime_[t].retriggers=0;note_off(t);}
 }
 project_->active_scene=section.scene;song_section_=uint8_t(index);song_bars_=0;song_section_bars_=uint16_t((song_section_ticks(section)+kSongTicksPerBar-1)/kSongTicksPerBar);song_elapsed_ticks_=0;song_duration_ticks_=song_section_ticks(section);
 event(EngineEventType::SceneLaunched,0,section.scene,uint8_t(section.ticks!=0),section.mute_mask);
}
bool Engine::follow_song(bool boundary,bool tick){
 const auto& song=project_->song;if(!song.enabled || !song.length){song_snapshot_=false;song_transpose_=0;song_mute_mask_=0;return false;}
 if(song_finished_)return true;
 if(song_armed_){if(boundary){song_armed_=false;apply_song_section(0,true);}return true;}
 if(!tick)return true;
 ++song_elapsed_ticks_;song_bars_=uint16_t(song_elapsed_ticks_/kSongTicksPerBar);
 if(song_elapsed_ticks_<song_duration_ticks_)return true;
 unsigned next=song_section_+1;
 if(next>=song.length){if(!song.loop){bool captured=song_has_snapshots(song);song_finished_=true;song_snapshot_=false;song_transpose_=0;song_mute_mask_=0;if(captured)command({CommandType::Stop,0,0,0});return true;}next=0;}
 apply_song_section(next,next==0);return true;
}
 void Engine::reset_clip_follow_seeds(){for(unsigned t=0;t<16;++t){clip_follow_seed_[t]=project_->random_seed^(0x85ebca6bu*(t+1));if(!clip_follow_seed_[t])clip_follow_seed_[t]=1;}}
 void Engine::follow_clip(unsigned t){
  auto& r=runtime_[t];r.follow_due=false;
  if(!r.playing || pending_scene_>=0 || pending_pattern_[t]>=0 || (pending_stop_&(1u<<t)) || (!scene_take_.active() && ((project_->song.enabled && project_->song.length) || (project_->chain.enabled && project_->chain.length))) || scene_take_.state()==SceneTake::Ready)return;
  int destination=clip_follow_destination(track(t).clip_follow,r.pattern,r.cycle,clip_follow_seed_[t]);
  if(destination==-2)launch_clip(t,r.pattern,false);else if(destination>=0)launch_clip(t,unsigned(destination),true);
 }
 void Engine::follow_scene(){
 const auto& chain=project_->chain;if(!chain.enabled || !chain.length)return;
 if(chain_armed_){chain_position_=chain_bars_=0;chain_armed_=false;launch_scene(chain.scenes[0]);return;}
 const auto& rule=chain.follow[chain.scenes[chain_position_]];unsigned bars=rule.bars?rule.bars:chain.bars;
 if(++chain_bars_<bars)return;
 chain_bars_=0;if((rule.condition==FollowCondition::Fill && !fill_) || (rule.condition==FollowCondition::NotFill && fill_))return;
 unsigned probability=rule.probability==255?chain.probability:rule.probability;if(next_random(meta_random_)%100>=probability)return;
 unsigned next=(chain_position_+1)%chain.length;
 if(chain.mode!=FollowMode::Sequential){
  unsigned total=0;for(unsigned i=0;i<chain.length;++i)if(chain.mode!=FollowMode::NoRepeat || chain.scenes[i]!=chain.scenes[chain_position_])total+=chain.follow[chain.scenes[i]].weight;
  if(!total){return;}unsigned choice=next_random(meta_random_)%total;
  for(unsigned i=0;i<chain.length;++i){if(chain.mode==FollowMode::NoRepeat && chain.scenes[i]==chain.scenes[chain_position_])continue;unsigned weight=chain.follow[chain.scenes[i]].weight;if(choice<weight){next=i;break;}choice-=weight;}
 }
 chain_position_=uint8_t(next);launch_scene(chain.scenes[chain_position_]);
}
void Engine::apply_launches() {
 if(pending_scene_>=0) { launch_scene(unsigned(pending_scene_)); pending_scene_=-1; }
 for(unsigned t=0;t<16;++t) {
  if(pending_pattern_[t]>=0) { launch_clip(t,unsigned(pending_pattern_[t]),true); pending_pattern_[t]=-1; }
  if(pending_stop_&(1u<<t)) { launch_clip(t,runtime_[t].pattern,false); pending_stop_&=uint16_t(~(1u<<t)); }
 }
}
void Engine::gesture_capture(const CommandState& c){
 if(!gesture_recording_ || gesture_replay_){return;}GesturePoint p;p.tick=uint16_t(gesture_phase_/(uint64_t(rate_)*60));p.track=c.track;p.pattern=c.pattern<16?c.pattern:runtime_[c.track].pattern;p.value=c.scalar;p.parameter=c.parameter;
 if(c.type==CommandType::SetEngineParameter){p.parameter=engine_target(track(c.track).instrument,c.parameter);if(!valid_target(p.parameter,p.value))return;}
 else if(c.type==CommandType::SetParameter){p.parameter=bind_parameter_target(track(c.track).instrument,p.parameter);if(!valid_target(p.parameter,p.value))return;}
 else if(c.type==CommandType::SetMorph && c.scalar>=0 && c.scalar<=1){p.target=GestureTarget::Morph;p.parameter=0;}
 else if(c.type==CommandType::SetMacroValue && c.step<8 && c.scalar>=0 && c.scalar<=1){p.target=GestureTarget::Macro;p.parameter=c.step;}
 else return;
 auto& g=project_->gesture;if(p.tick>=g.ticks)return;if(g.count && g.points[g.count-1].tick==p.tick && g.points[g.count-1].target==p.target && g.points[g.count-1].track==p.track && g.points[g.count-1].pattern==p.pattern && g.points[g.count-1].parameter==p.parameter)g.points[g.count-1]=p;
 else if(g.count<128)g.points[g.count++]=p;else {gesture_recording_=false;event(EngineEventType::GestureState,0,4);return;}
 event(EngineEventType::GesturePoint,p.track,p.pattern,uint8_t(p.target),p.parameter,p.value,p.tick);
}
void Engine::gesture_tick(){
 if(!playing_ || (!gesture_recording_ && !gesture_playing_)){return;}const auto& g=project_->gesture;uint64_t unit=uint64_t(rate_)*60;
 if(gesture_playing_){unsigned tick=unsigned(gesture_phase_/unit);bool changed=false;while(gesture_cursor_<g.count && g.points[gesture_cursor_].tick<=tick){gesture_overlay_.apply(g.points[gesture_cursor_++]);changed=true;}if(changed)control_refresh_mask_=0xffff;}
 gesture_phase_+=uint64_t(bpm_)*96;if(gesture_phase_>=unit*g.ticks){gesture_phase_-=unit*g.ticks;gesture_cursor_=0;if(gesture_recording_){gesture_recording_=false;event(EngineEventType::GestureState,0,0);}}
}
void Engine::release_local_midi(unsigned pad){if(pad>=16)return;for(auto& n:local_midi_[pad])if(n.held){n.releasing=true;if(midi(uint8_t(0x80|n.channel),n.note,0))n.held=false;}}
void Engine::release_local(){
 for(unsigned pad=0;pad<16;++pad){release_local_midi(pad);replace_repeat_pad(pad);for(unsigned t=0;t<16;++t)release_midi_channel(t,uint16_t(256+pad),false);}
}
}
