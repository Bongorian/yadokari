#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
bool Engine::erase_record_step(unsigned t,unsigned cell,uint64_t serial){
 if(t>=16 || cell>=kSteps || !serial){return false;}if(record_erased_[t][cell]==serial)return true;auto& step=track(t).steps[cell];if(step.enabled){if(!events_.free_space() || !erase_payload_->free_space()){__atomic_add_fetch(&event_dropped_,1,__ATOMIC_RELAXED);__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);return false;}erase_payload_->push(step);QueuedEngineEvent e{};e.type=EngineEventType::RecordErase;e.sample=sample_count_;e.track=uint8_t(t);e.value=uint8_t(cell);e.flags=runtime_[t].pattern;e.generation=generation_;e.pattern_revision=pattern_revisions_->audio[e.flags][t];__atomic_add_fetch(&project_events_pending_,1,__ATOMIC_RELEASE);events_.push(e);}record_erased_[t][cell]=serial;return true;
}
bool Engine::recording_position(unsigned t,unsigned velocity,unsigned& cell,uint16_t& timing,bool& use_previous,bool require_armed,bool quantized){
 if((require_armed && !note_record_armed_) || !playing_ || t>=16 || !runtime_[t].playing || !velocity)return false;
 const auto& r=runtime_[t];const auto& state=track(t);uint64_t period_q32=step_period(t),half=period_q32>>33;use_previous=r.last_period_q32 && r.due>sample_count_ && r.due-sample_count_>half;cell=use_previous?r.last_step:r.position;timing=0;
 if(!quantized){int64_t period=int64_t(period_q32>>32);if(period<1)period=1;const auto& pattern=project_->patterns[r.pattern];auto baseline=[&](int64_t grid,unsigned step,int64_t duration){return grid+((step&1)?duration*pattern.swing/400:0)+duration*groove_timing(state.groove,step)*state.groove.amount/38400;};int64_t last_period=int64_t(r.last_period_q32>>32);if(last_period<1)last_period=period;int64_t next=baseline(int64_t(r.grid),r.position,period),previous=baseline(int64_t(r.last_grid),r.last_step,last_period),now=int64_t(sample_count_);use_previous=r.last_period_q32 && fabs(double(now-previous))<=fabs(double(now-next));cell=use_previous?r.last_step:r.position;int64_t base=use_previous?previous:next,duration=use_previous?last_period:period;int micro=bounded(int(lround(double(now-base)*384.0/double(duration))),-192,192);timing=uint16_t(micro+193);}
 return true;
}
void Engine::record_kit_hit(unsigned t,unsigned pad,unsigned velocity){unsigned rcell=0;uint16_t rt=0;bool rp=false;if(pad<16 && recording_position(t,velocity,rcell,rt,rp,false,false))retro_push(t,rcell,runtime_[t].pattern,1,pad,velocity,rt,runtime_[t].step_serial);unsigned cell=0;uint16_t timing=0;bool previous=false;if(pad>=16 || !recording_position(t,velocity,cell,timing,previous,true,project_->ui.note_quantized))return;event(EngineEventType::RecordedKitHit,uint8_t(t),uint8_t(cell),runtime_[t].pattern,uint16_t(pad),float(velocity),timing);}
void Engine::record_note(unsigned t,unsigned note,unsigned velocity,unsigned channel,bool gated,bool chromatic,bool native_drum){
 {unsigned rcell=0;uint16_t rt=0;bool rp=false;const auto& rstate=track(t);if(rstate.instrument!=Instrument::DrumKit && recording_position(t,velocity,rcell,rt,rp,false,false)){int rel=60+int(note)-int(rstate.root_note);if(rel>=0 && rel<=127)retro_push(t,rcell,runtime_[t].pattern,0,unsigned(rel)|(chromatic?128:0)|(native_drum?256:0),velocity,rt,runtime_[t].step_serial);}}
 unsigned cell=0;uint16_t timing=0;bool use_previous=false;if(!recording_position(t,velocity,cell,timing,use_previous,true,project_->ui.note_quantized))return;const auto& r=runtime_[t];const auto& state=track(t);if(state.instrument==Instrument::DrumKit){unsigned pad=note>=state.root_note && note<unsigned(state.root_note)+16?note-state.root_note:note>=36 && note<52?note-36:0;record_kit_hit(t,pad,velocity);return;}
 int relative=60+int(note)-int(state.root_note);if(relative<0 || relative>127)return;
 // A repeated Note On ends the preceding gate before its identity is reused.
 // This also orders the old gate before any Replace erase for the new note.
 if(gated)finish_recorded_note(t,note,channel);
 if(project_->ui.note_replace && t==note_record_track_ && !erase_record_step(t,cell,r.step_serial+(use_previous?0:1)))return;
 uint32_t id=0;if(gated){RecordedHeld* slot=nullptr;for(auto& held:recorded_held_)if(held.id && held.track==t && held.note==note && held.channel==channel){slot=&held;break;}if(!slot)for(auto& held:recorded_held_)if(!held.id){slot=&held;break;}if(slot){if(++recording_serial_==0)++recording_serial_;id=recording_serial_;*slot={sample_count_,step_period(t),id,uint8_t(t),uint8_t(note),uint16_t(channel),r.pattern,uint8_t(cell),pattern_revisions_->audio[r.pattern][t]};}else event(EngineEventType::RecordCapacity,uint8_t(t),0);}
 event(EngineEventType::RecordedNote,uint8_t(t),uint8_t(cell),r.pattern,uint16_t(relative|(chromatic?128:0)|(native_drum?256:0)),float(velocity),timing,id);
}
void Engine::finish_recorded_note(unsigned t,unsigned note,unsigned channel){for(auto& held:recorded_held_)if(held.id && held.track==t && held.note==note && held.channel==channel){double units=double(sample_count_-held.start)*(96.0*4294967296.0)/double(held.period);unsigned gate=units>=384?384:units<1?1:unsigned(units+0.5);event(EngineEventType::RecordedGate,held.track,held.step,held.pattern,uint16_t(gate),0,0,held.id,held.pattern_revision);held.id=0;}}
void Engine::finish_recorded_channel(unsigned t,unsigned channel){for(auto& held:recorded_held_)if(held.id && held.track==t && held.channel==channel)finish_recorded_note(t,held.note,channel);}
void Engine::finish_recorded_all(bool emit){for(auto& held:recorded_held_)if(held.id){if(emit)finish_recorded_note(held.track,held.note,held.channel);else held.id=0;}}
bool Engine::refresh_parameters_step(unsigned t,const StepState* step) {
 auto& r=runtime_[t];const auto& state=track(t);
 bool gesture_live=gesture_overlay_.count || gesture_overlay_.morph_active;for(bool b:gesture_overlay_.macro_active)gesture_live=gesture_live||b;
 if(gesture_live || control_refresh_mask_){if(step)refresh_parameters(t,step);else{for(auto& lock:r.active_locks)lock.enabled=false;refresh_parameters(t);}return true;}
 uint64_t h=1469598103934665603ULL;auto mix=[&](uint64_t v){h^=v;h*=1099511628211ULL;};auto mixf=[&](float f){uint32_t u;__builtin_memcpy(&u,&f,4);mix(u);};
 mix(cc_generation_);mix(r.pattern);mix(r.automation_cell);mix(r.automation_live?1:0);mix(control_all_?1:0);mix(control_all_id_);mixf(control_all_delta_);mix(project_->morph_enabled?1:0);mixf(project_->morph_position);mix(state.pitch_bend);mix(state.pressure);
 if(step)for(unsigned i=0;i<kLocks;++i){const auto& l=step->locks[i];mix(l.id);mix(l.enabled?1:0);mixf(l.value);}else mix(0x9e3779b97f4a7c15ULL);
 if(r.step_fp_valid && h==r.step_fp)return false;
 if(step)refresh_parameters(t,step);else{for(auto& lock:r.active_locks)lock.enabled=false;refresh_parameters(t);}
 r.step_fp=h;r.step_fp_valid=true;return true;
}
bool Engine::freeze_capture_begin(unsigned t,uint32_t maximum){return capture_buffer_begin(t,maximum,false);}
bool Engine::twin_capture_begin(unsigned t,unsigned note,unsigned velocity,unsigned hold,unsigned maximum){
 if(t>=kTracks || note>127 || !velocity || velocity>127 || hold<10 || hold>4000 || maximum<hold || maximum>10000 || live_record_state())return false;
 return capture_buffer_begin(t,uint32_t(uint64_t(rate_)*maximum/1000),true,note,velocity,unsigned(uint64_t(rate_)*hold/1000));
}
bool Engine::capture_buffer_begin(unsigned track_index,uint32_t maximum_frames,bool twin,unsigned note,unsigned velocity,unsigned hold){
 service_freeze_capture();
 if(track_index>=16 || freeze_capture_state()!=0 || freeze_release_pending_)return false;
 const uint64_t limit=uint64_t(rate_)*120;
 const uint64_t capacity=maximum_frames?maximum_frames:limit;
 if(!capacity || capacity>limit || capacity>0xffffffffu)return false;
 int16_t* storage=twin?nullptr:alloc_array<int16_t>(size_t(capacity)*2);float* floating=twin?alloc_array<float>(size_t(capacity)*2):nullptr;if(twin?!floating:!storage)return false;
 twin_storage_=floating;freeze_storage_=storage;freeze_storage_frames_=uint32_t(capacity);
 freeze_request_track_=track_index;freeze_request_generation_=published_generation_;twin_capture_request_=twin;twin_note_=uint8_t(note);twin_velocity_=uint8_t(velocity);twin_hold_frames_=hold;
 __atomic_store_n(&freeze_cancel_,0u,__ATOMIC_RELAXED);
 __atomic_store_n(&freeze_published_frames_,0u,__ATOMIC_RELAXED);
 __atomic_store_n(&freeze_status_,5u,__ATOMIC_RELEASE);return true;
}
void Engine::freeze_capture_abort(){__atomic_store_n(&freeze_cancel_,1u,__ATOMIC_RELEASE);}
void Engine::freeze_capture_release(){freeze_release_pending_=true;freeze_capture_abort();service_freeze_capture();}
void Engine::service_freeze_capture(){
 if(!freeze_release_pending_)return;
 unsigned status=freeze_capture_state();if(status!=0 && status!=3 && status!=4)return;
 delete[] twin_storage_;twin_storage_=nullptr;delete[] freeze_storage_;freeze_storage_=nullptr;freeze_storage_frames_=0;freeze_release_pending_=false;
 __atomic_store_n(&freeze_status_,0u,__ATOMIC_RELEASE);
}
float Engine::freeze_capture_progress()const{
 unsigned status=freeze_capture_state();if(status==3)return 1;
 if(status!=2)return 0;
 unsigned frames=__atomic_load_n(&freeze_published_frames_,__ATOMIC_RELAXED),estimate=__atomic_load_n(&freeze_published_estimate_,__ATOMIC_RELAXED);
 float progress=float(frames)/float(estimate?estimate:1);return progress>1?1:progress;
}
void Engine::consume_freeze_capture(){
 if(!freeze_audio_active_ && freeze_capture_state()==5){
  freeze_audio_active_=true;freeze_state_=4;freeze_write_=false;freeze_frames_=0;
  const unsigned t=freeze_request_track_;
  if(t<16 && generation_==freeze_request_generation_ && !__atomic_load_n(&freeze_cancel_,__ATOMIC_ACQUIRE)){
   const auto& st=track(t);
   if(twin_capture_request_){
    if(twin_capture_allowed(t)){stop_twin_voice(t);freeze_buffer_=freeze_storage_;freeze_capacity_=freeze_storage_frames_;freeze_track_=uint8_t(t);freeze_pattern_=runtime_[t].pattern;freeze_estimate_=freeze_capacity_;freeze_state_=2;twin_isolated_=true;twin_heard_=false;twin_quiet_=0;twin_capture_revision_=cc_generation_;for(auto& lock:runtime_[t].active_locks)lock.enabled=false;refresh_parameters(t);auto* voice=trigger(t,twin_velocity_,twin_note_,nullptr,nullptr,0,false,true,-1,-1,nullptr,-1,nullptr,nullptr,nullptr,true);if(!voice)freeze_state_=4;else for(auto* child=voice;child;child=child->layer_peer){child->off_at=0;child->daisy.hold();}}
   }else{
   double step_samples=double(rate_)*60.0/double(bpm_)/4.0*double(st.speed_den)/double(st.speed_num?st.speed_num:1);
   uint64_t estimate=uint64_t(step_samples*st.length+0.5),capacity=estimate+rate_;
   if(playing_ && runtime_[t].playing && st.frozen_asset>=kSampleAssets && st.direction==PlaybackDirection::Forward && capacity<=freeze_storage_frames_){
    freeze_buffer_=freeze_storage_;freeze_capacity_=freeze_storage_frames_;freeze_track_=uint8_t(t);freeze_pattern_=runtime_[t].pattern;
    freeze_estimate_=uint32_t(estimate?estimate:1);freeze_state_=1;
   }
   }
  }
 }
 if(freeze_audio_active_ && (generation_!=freeze_request_generation_ || __atomic_load_n(&freeze_cancel_,__ATOMIC_ACQUIRE))){freeze_state_=4;freeze_write_=false;}
}
void Engine::publish_freeze_capture(){
 if(!freeze_audio_active_)return;
 __atomic_store_n(&freeze_published_frames_,freeze_frames_,__ATOMIC_RELAXED);
 __atomic_store_n(&freeze_published_estimate_,freeze_estimate_,__ATOMIC_RELAXED);
 if(freeze_state_==3 || freeze_state_==4){if(twin_isolated_){stop_twin_voice(freeze_track_);twin_isolated_=false;}freeze_audio_active_=false;}
 // Last access to the capture buffer precedes this acknowledgement. The
 // next callback will not use it unless a fresh start is handed over.
 __atomic_store_n(&freeze_status_,unsigned(freeze_state_),__ATOMIC_RELEASE);
}
void Engine::trigger_frozen(unsigned t){
 const auto& st=track(t);unsigned slot=st.frozen_asset;if(slot>=kSampleAssets)return;
 const WavView& view=samples_[slot];if((!view.data && !view.reader) || !view.frames)return;
 auto& v=voices_[t];note_off_main(t);v.sampler.stop();
 v.instrument=unsigned(Instrument::Sampler);v.sample=view;v.sample_slot=uint8_t(slot);v.note=60;v.sample_root=60;v.sample_origin=0;v.sample_extent=1;
 v.sample_mode=SampleMode::OneShot;v.looping=false;v.released=false;v.off_at=0;v.choke_group=0;v.kit_pad=255;
 SampleEdit edit{};v.base_sample_edit=edit;v.sampler_source_epoch=0;v.sample_speed=edit.speed;
 v.sampler.start_edit(view,rate_,1.0f,false,false,edit,1.0f);
 v.env=1;
}
bool Engine::twin_capture_allowed(unsigned t){
 if(t>=16 || audio_lane_mask_&(1u<<t) || track(t).frozen_asset<kSampleAssets)return false;
 const auto engine=track(t).instrument;if(engine==Instrument::Empty || engine==Instrument::Unknown || engine==Instrument::ExternalMidi || engine==Instrument::DrumKit || engine==Instrument::Texture)return false;
 if(!playing_ || !runtime_[t].playing)return true;
 for(const auto& step:track(t).steps)if(step.enabled)return false;
 return true;
}
void Engine::stop_twin_voice(unsigned t){note_off(t);clear_poly(t);if(auto* dsp=plaits_dsp(t,voices_[t]))dsp->stop();voices_[t].sampler.stop();voices_[t].env=0;voices_[t].instrument=unsigned(Instrument::Empty);runtime_[t].retriggers=0;}
void Engine::twin_capture_frame(float l,float r){
 if(freeze_state_!=2)return;
 if(freeze_frames_>=freeze_capacity_){freeze_state_=twin_heard_?3:4;return;}
 if(!isfinite(l) || !isfinite(r)){freeze_state_=4;return;}twin_storage_[freeze_frames_*2]=l;twin_storage_[freeze_frames_*2+1]=r;++freeze_frames_;
 if(fmaxf(fabsf(l),fabsf(r))>0.00025f){twin_heard_=true;twin_quiet_=0;}else ++twin_quiet_;
 if(freeze_frames_==twin_hold_frames_)note_off(freeze_track_);
 if((twin_heard_ && twin_quiet_>=rate_/12 && freeze_frames_>=rate_/100) || freeze_frames_==freeze_capacity_)freeze_state_=twin_heard_?3:4;
}
bool Engine::submit_twin_install(TwinInstall* install){
 if(!install || twin_install_ || !command_space())return false;
 __atomic_store_n(&twin_install_,install,__ATOMIC_RELEASE);
 if(!submit({CommandType::InstallTwin,uint8_t(install->track),0,0})){__atomic_store_n(&twin_install_,static_cast<TwinInstall*>(nullptr),__ATOMIC_RELEASE);return false;}return true;
}
TwinInstall* Engine::twin_install_result()const{auto* p=__atomic_load_n(&twin_install_,__ATOMIC_ACQUIRE);return p && __atomic_load_n(&p->status,__ATOMIC_ACQUIRE)?p:nullptr;}
void Engine::release_twin_install(){auto* p=twin_install_result();if(p){__atomic_store_n(&twin_install_,static_cast<TwinInstall*>(nullptr),__ATOMIC_RELEASE);delete p;}}
void Engine::apply_twin_switch(unsigned t){
 if(t>=16 || !project_->twins[t].prepared())return;
 if((audio_lane_mask_&(1u<<t)) || track(t).frozen_asset<kSampleAssets){event(EngineEventType::TwinRejected,uint8_t(t),0);return;}
 auto& twin=project_->twins[t];unsigned slot=twin.active?twin.source:twin.sampled;
 if(mount_override(t,slot,true)){twin.active=!twin.active;}else event(EngineEventType::TwinRejected,uint8_t(t),0);
}

void Engine::collect_twin_held(unsigned t){
 twin_held_count_=0;
 auto add=[&](const Voice& voice,bool sequenced){if(voice.env<.00001f || voice.released || (voice.off_at && voice.off_at<=sample_count_) || twin_held_count_>=kPolyVoiceCapacity+1)return;
  // A Layer's peers are one key, not independent repeated key presses.
  if(voices_[t].env>=.00001f && voices_[t].layer_peer==&voice)return;
  for(uint32_t mask=poly_track_mask_[t];mask;mask&=mask-1)if(poly_voices_[unsigned(__builtin_ctz(mask))].voice.layer_peer==&voice)return;
  auto& held=twin_held_[twin_held_count_++];held.note=voice.input_note;held.channel=voice.received_channel;held.off=voice.off_at;held.sequenced=sequenced;float scale=track(t).velocity_scale;held.velocity=uint8_t(bounded(int(voice.a*127/(scale>.001f?scale:1)+.5f),1,127));
 };
 add(voices_[t],voices_[t].off_at!=0);for(uint32_t mask=poly_track_mask_[t];mask;mask&=mask-1){const auto& voice=poly_voices_[unsigned(__builtin_ctz(mask))];add(voice.voice,voice.sequenced);}
}
void Engine::retrigger_twin_held(unsigned t){
 refresh_parameters(t);const auto& r=runtime_[t];bool grid_due=playing_ && r.playing && sample_count_>=r.due && track(t).steps[r.position].enabled;
 for(unsigned i=0;i<twin_held_count_;++i){const auto& held=twin_held_[i];if(held.sequenced && grid_due)continue;
  auto* first=trigger(t,held.velocity,held.note,nullptr,nullptr,0,false,true,-1,-1,nullptr,-1,nullptr,nullptr,nullptr,true);
  for(auto* voice=first;voice;voice=voice->layer_peer){voice->off_at=held.off;voice->received_channel=held.channel;voice->input_note=held.note;voice->daisy.hold();for(uint32_t mask=poly_track_mask_[t];mask;mask&=mask-1){auto& slot=poly_voices_[unsigned(__builtin_ctz(mask))];if(&slot.voice==voice)slot.sequenced=held.sequenced;}}
 }
}

}
