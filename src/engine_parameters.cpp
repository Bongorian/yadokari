#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
void Engine::refresh_cc(unsigned t){auto& r=runtime_[t];const auto& state=track(t);if(state.instrument!=Instrument::ExternalMidi){r.nrpn_channel=255;r.cc_channel=255;r.program_channel=255;return;}
 {uint64_t h=1469598103934665603ULL;auto mix=[&](uint64_t v){h^=v;h*=1099511628211ULL;};auto mixf=[&](float f){uint32_t u;__builtin_memcpy(&u,&f,4);mix(u);};
  mix(cc_generation_);mix(r.pattern);mix(r.automation_cell);mix(r.automation_live?1:0);mix(control_all_?1:0);mix(control_all_id_);mixf(control_all_delta_);mix(state.midi_channel);mix(state.midi_program);mix(project_->morph_enabled?1:0);mixf(project_->morph_position);
  for(unsigned i=0;i<16;i+=8){uint64_t w;__builtin_memcpy(&w,&state.midi_cc_used[i],8);mix(w);}
  for(unsigned i=0;i<kLocks;++i){const auto& l=r.active_locks[i];mix(l.id);mix(l.enabled?1:0);mixf(l.value);}
  bool gesture_live=gesture_overlay_.count || gesture_overlay_.morph_active;for(bool b:gesture_overlay_.macro_active)gesture_live=gesture_live||b;
  if(!gesture_live && r.cc_primed && !r.cc_dirty && h==r.cc_hash)return;
  r.cc_hash=h;r.cc_primed=true;r.cc_dirty=false;}if(r.program_channel!=state.midi_channel){r.program_channel=state.midi_channel;r.program_sent=255;}if(r.cc_channel!=state.midi_channel){for(auto& value:r.cc_sent)value=255;r.cc_channel=state.midi_channel;}bool wanted[128]{};for(unsigned cc=0;cc<128;++cc)wanted[cc]=r.cc_held[cc] || (state.midi_cc_used[cc/8]&(1u<<(cc%8)));for(const auto& lock:r.active_locks)if(lock.enabled && lock.id>=128 && lock.id<256)wanted[lock.id-128]=true;for(const auto& lane:state.automation)if(r.automation_live && lane.enabled && lane.parameter>=128 && lane.parameter<256)wanted[lane.parameter-128]=true;for(unsigned mi=0;mi<8;++mi)if(project_->macros[mi].active || gesture_overlay_.macro_active[mi])for(const auto& route:project_->macros[mi].routes)if(route.enabled && route.parameter>=128 && route.parameter<256 && (route.tracks&(1u<<t)))wanted[route.parameter-128]=true;
 for(unsigned i=0;i<gesture_overlay_.count;++i){const auto& p=gesture_overlay_.points[i];if(p.track==t && p.pattern==r.pattern && p.parameter>=128 && p.parameter<256)wanted[p.parameter-128]=true;}
 wanted[1]=wanted[1] || r.strip_mod_active;
 auto send_cc=[&](unsigned cc){if(!wanted[cc] && !(control_all_ && control_all_id_==128+cc))return true;auto value=resolve_parameter(*project_,state,t,uint16_t(128+cc),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);bool held=value.sources!=0 || (cc==1 && r.strip_mod_active);uint8_t v=cc==1 && r.strip_mod_active?uint8_t(r.strip_mod*127+0.5f):uint8_t(value.value);if(r.cc_sent[cc]!=v){if(!midi(uint8_t(0xb0|state.midi_channel),uint8_t(cc),v)){r.cc_held[cc]=true;r.cc_dirty=true;return false;}r.cc_sent[cc]=v;if(cc==0 || cc==32)r.program_sent=255;}r.cc_held[cc]=held;return true;};
 // Bank Select must precede Program Change, including a reconnect snapshot.
 bool bank_msb=send_cc(0),bank_lsb=send_cc(32);if(state.midi_program<128 && r.program_sent!=state.midi_program && bank_msb && bank_lsb){if(midi(uint8_t(0xc0|state.midi_channel),state.midi_program,0,2))r.program_sent=state.midi_program;else r.cc_dirty=true;}else if(state.midi_program==128)r.program_sent=255;
 for(unsigned cc=0;cc<128;++cc)if(cc!=0 && cc!=32)send_cc(cc);
 if(r.nrpn_channel!=state.midi_channel){for(auto& value:r.nrpn_sent)value=65535;r.nrpn_channel=state.midi_channel;}
 for(unsigned slot=0;slot<4;++slot){const auto& cfg=state.nrpn[slot];if(!cfg.enabled){r.nrpn_sent[slot]=65535;continue;}uint16_t id=uint16_t(256+slot);auto v=resolve_parameter(*project_,state,t,id,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);uint16_t value=uint16_t(v.value);if(r.nrpn_sent[slot]==value && r.nrpn_number[slot]==cfg.number && r.nrpn_type[slot]==unsigned(cfg.registered))continue;
  // Reserve a whole six-message transaction and the normal Note Off reserve.
  if(midi_events_.free_space()<69 || (midi_capture_active_ && !midi_capture_internal_ && midi_capture_events_->free_space()<7)){r.cc_dirty=true;continue;}
  uint8_t status=uint8_t(0xb0|state.midi_channel);midi(status,cfg.registered?101:99,uint8_t(cfg.number>>7));midi(status,cfg.registered?100:98,uint8_t(cfg.number&127));midi(status,6,uint8_t(value>>7));midi(status,38,uint8_t(value&127));midi(status,101,127);midi(status,100,127);r.nrpn_sent[slot]=value;r.nrpn_number[slot]=cfg.number;r.nrpn_type[slot]=uint8_t(cfg.registered);
 }
}
bool Engine::refresh_group_effects(){
 // Group inserts are global buses. Like shared returns, the strongest active
 // layer wins; ties use the lowest track index. Avoid 512 resolver calls when
 // only scene morph is active, and resolve other layers only for affected IDs.
 bool master_layered=project_->morph_enabled || gesture_overlay_.morph_active;
 uint64_t layered[2]={0,0};auto mark=[&](uint16_t id){if(id>=64 && id<=76)master_layered=true;if(id>=384 && id<474){unsigned n=id-384;layered[n/64]|=uint64_t(1)<<(n%64);}};
 if(control_all_)mark(control_all_id_);
 for(unsigned i=0;i<gesture_overlay_.count;++i)mark(gesture_overlay_.points[i].parameter);
 for(unsigned m=0;m<8;++m)if(project_->macros[m].active || gesture_overlay_.macro_active[m])for(const auto& route:project_->macros[m].routes)if(route.enabled)mark(route.parameter);
 for(unsigned t=0;t<16;++t){const auto& r=runtime_[t];for(const auto& lock:r.active_locks)if(lock.enabled)mark(lock.id);if(r.automation_live)for(const auto& lane:track(t).automation)if(lane.enabled)mark(lane.parameter);}
 bool morph=project_->morph_enabled || gesture_overlay_.morph_active;float position=gesture_overlay_.morph_active?gesture_overlay_.morph:project_->morph_position;
 for(unsigned i=0;i<88;++i){uint16_t id=uint16_t(384+i);unsigned group=i<64?group_fx_group(id):i<80?(i-64)/2:i-80,field=i<64?group_fx_field(id):i<80?8+(i-64)%2:10;float base=i<64?project_->routing.groups[group].insert_fx[field]:i<80?project_->routing.groups[group].send[field-8]:project_->routing.groups[group].parallel;float value=morph?interpolate_parameter(id,project_->morph_a[0].values[morph_index(uint16_t(384+i))],project_->morph_b[0].values[morph_index(uint16_t(384+i))],position):base;
  if(layered[i/64]&(uint64_t(1)<<(i%64))){unsigned priority=0;for(unsigned t=0;t<16;++t){const auto& r=runtime_[t];auto v=resolve_parameter(*project_,track(t),t,id,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);unsigned rank=v.sources&FromControlAll?6:v.sources&FromLock?5:v.sources&FromMacro?4:v.sources&FromGesture?3:v.sources&FromAutomation?2:v.sources&FromMorph?1:0;if(rank>priority){value=v.value;priority=rank;}}}
  group_fx_target_[group][field]=value;
 }
 refresh_performance_fx(uint8_t((layered[1]>>24)&3));
 return master_layered;
}
// The shared performance bus uses the same layer priority as group effects.
// Resolve only IDs that have an active non-scene layer; the ordinary path is
// a pair of base/scene reads, not a full 16-track scan.
void Engine::refresh_performance_fx(uint8_t layered){
 if(!playing_){performance_fx_auto_mode_=PerformanceFx::Off;return;}
 float values[2];bool morph=project_->morph_enabled || gesture_overlay_.morph_active;float position=gesture_overlay_.morph_active?gesture_overlay_.morph:project_->morph_position;
 for(unsigned i=0;i<2;++i){uint16_t id=uint16_t(472+i);
  // Descriptor lookup can return the sentinel; never use it as an array index.
  const unsigned index=morph?morph_index(id):kCommonMorphParameters;
  float value=index<kCommonMorphParameters?interpolate_parameter(id,project_->morph_a[0].values[index],project_->morph_b[0].values[index],position):parameter_base(*project_,track(0),id);
  if(layered & (1u<<i)){unsigned priority=0;for(unsigned t=0;t<16;++t){const auto& r=runtime_[t];auto v=resolve_parameter(*project_,track(t),t,id,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);unsigned rank=v.sources&FromControlAll?6:v.sources&FromLock?5:v.sources&FromMacro?4:v.sources&FromGesture?3:v.sources&FromAutomation?2:v.sources&FromMorph?1:0;if(rank>priority){value=v.value;priority=rank;}}}values[i]=value;
 }
 performance_fx_auto_mode_=PerformanceFx(unsigned(values[0]));performance_fx_auto_amount_=values[1];
 if(performance_fx_manual_mode_==PerformanceFx::Off){performance_fx_applied_=performance_fx_auto_mode_;performance_fx_.set(performance_fx_auto_mode_,performance_fx_auto_amount_);}
}
void Engine::refresh_effects(){
 // The same bounded scan discovers all master, group and live-FX layers.
 bool layered=refresh_group_effects();
 if(!layered){for(unsigned i=0;i<13;++i)effects_target_[i]=parameter_base(*project_,track(0),uint16_t(64+i));return;}
 for(unsigned i=0;i<13;++i){uint16_t id=uint16_t(64+i);effects_target_[i]=parameter_base(*project_,track(0),id);unsigned priority=0;for(unsigned t=0;t<16;++t){const auto& r=runtime_[t];auto v=resolve_parameter(*project_,track(t),t,id,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);unsigned rank=v.sources&FromControlAll?6:v.sources&FromLock?5:v.sources&FromMacro?4:v.sources&FromGesture?3:v.sources&FromAutomation?2:v.sources&FromMorph?1:0;if(rank>priority){effects_target_[i]=v.value;priority=rank;}}}}
ModRuntime Engine::mod_runtime(unsigned t) const {const auto& r=runtime_[t];ModRuntime m;m.lfo=0.5f+0.5f*sinf(r.lfo_phase*6.2831853f);m.pressure=r.pressure<0?0:r.pressure>1?1:r.pressure;m.envelope=voices_[t].env;for(const auto& p:poly_voices_)if(p.owner==t && p.voice.env>m.envelope)m.envelope=p.voice.env;m.envelope=m.envelope<0?0:m.envelope>1?1:m.envelope;m.velocity=r.last_velocity;return m;}
bool Engine::refresh_expression(unsigned t){bool sent=true;auto& r=runtime_[t];ModRuntime mod=mod_runtime(t);const auto& state=track(t);auto bend=resolve_parameter(*project_,state,t,260,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod);auto pressure=resolve_parameter(*project_,state,t,261,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod);int bend_raw=int(bend.value)+r.strip_pitch;unsigned raw=unsigned(bend_raw<0?0:bend_raw>16383?16383:bend_raw);r.pressure_target=pressure.value/127.0f;if(r.expression_bend!=raw){r.expression_bend=uint16_t(raw);float shift=(float(raw)-8192)/4096;auto tune=[&](Voice& v){v.pitch_bend=shift+v.layer_detune;v.frequency=440*powf(2,(float(v.note)+project_->tuning[v.note%12]/100+v.pitch_bend-69)/12);float hz=v.frequency<float(rate_)*0.45f?v.frequency:float(rate_)*0.45f;v.saw.set_frequency(hz);v.daisy.set_note_float(float(v.note)+project_->tuning[v.note%12]/100+v.pitch_bend);if(auto* dsp=plaits_dsp(t,v))dsp->set_note_float(float(v.note)+project_->tuning[v.note%12]/100+v.pitch_bend);};tune(voices_[t]);for(auto& p:poly_voices_)if(p.owner==t)tune(p.voice);}
 if(midi_capture_active_ && midi_capture_internal_ && capture_internal_track(t)){if(capture_bend_[t]!=raw){if(capture_bend_[t]==65535 && raw==8192)capture_bend_[t]=8192;else if(capture_internal(uint8_t(0xe0|t),uint8_t(raw&127),uint8_t(raw>>7)))capture_bend_[t]=uint16_t(raw);}capture_pressure(t,uint8_t(pressure.value));}
 if(state.instrument==Instrument::ExternalMidi){if(r.expression_channel!=state.midi_channel){r.expression_channel=state.midi_channel;r.expression_sent_bend=65535;r.expression_sent_pressure=255;}bool want_bend=r.strip_pitch_active||(state.expression_used&1)||bend.sources,want_pressure=(state.expression_used&2)||pressure.sources;if((want_bend||r.expression_held_bend) && r.expression_sent_bend!=raw){if(midi(uint8_t(0xe0|state.midi_channel),uint8_t(raw&127),uint8_t(raw>>7)))r.expression_sent_bend=uint16_t(raw);else sent=false;}if((want_pressure||r.expression_held_pressure) && r.expression_sent_pressure!=unsigned(pressure.value)){if(midi(uint8_t(0xd0|state.midi_channel),uint8_t(pressure.value),0,2))r.expression_sent_pressure=uint8_t(pressure.value);else sent=false;}if(r.expression_sent_bend==raw)r.expression_held_bend=want_bend;if(r.expression_sent_pressure==unsigned(pressure.value))r.expression_held_pressure=want_pressure;}else r.expression_channel=255;return sent;}
float Engine::sampler_speed(unsigned t,Voice& voice,const ModRuntime& modulation){
 const auto& state=track(t);const auto& r=runtime_[t];
 // Epoch zero is the existing frozen-audio path, whose playback contract is
 // separate from an attached instrument. A retired instance keeps its last
 // resolved speed, including any lock/automation active before replacement.
 if(voice.sampler_source_epoch && (voice.sampler_source_epoch!=sampler_source_epoch_[t]))return voice.sample_speed;
 voice.sample_speed=resolve_target_value(*project_,state,t,engine_target(Instrument::Sampler,0),voice.sound_locked?voice.base_sample_edit.speed:state.sample_edit.speed,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&modulation).value;
 return voice.sample_speed;
}
void Engine::refresh_parameters(unsigned t,const StepState* step) {
 const auto& state=track(t);auto& r=runtime_[t];ModRuntime mod=mod_runtime(t);if(step)for(unsigned i=0;i<kLocks;++i)r.active_locks[i]=step->locks[i];
 refresh_expression(t);
 // A bounded dry overdub assumes an unchanged, full-length source at unity
 // pitch/speed. Check resolved layers on the audio owner, so a change that
 // occurs and is restored between task-side polls cannot be missed.
 if(!(audio_lane_mask_&(1u<<t)) && t==live_record_restart_track_ && __atomic_load_n(&live_record_state_,__ATOMIC_RELAXED)==2){
  const ParameterTarget ids[]={engine_target(Instrument::Sampler,0),engine_target(Instrument::Sampler,1),engine_target(Instrument::Sampler,2),260};const float expected[]={1,0,1,8192};
  for(unsigned i=0;i<4;++i)if(resolve_target_value(*project_,state,t,ids[i],parameter_base(*project_,state,ids[i]),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value!=expected[i]){__atomic_store_n(&live_record_conflict_,1,__ATOMIC_RELEASE);break;}
 }
 for(unsigned i=0;i<kTrackParameters;++i)r.target[i]=resolve_parameter(*project_,state,t,track_parameter_id(i),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value;
 if(audio_lane_mask_&(1u<<t)){r.target[0]=project_->audio[t].gain;r.target[1]=project_->audio[t].pan;}
 r.tempo_lfo.request(resolve_parameter(*project_,state,t,11,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value,resolve_parameter(*project_,state,t,12,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value);
 r.reverb_target=resolve_parameter(*project_,state,t,10,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value;
 r.pressure_depth_target=resolve_parameter(*project_,state,t,9,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value;
 auto& voice=voices_[t];if(state.instrument!=Instrument::DrumKit || voice.instrument!=unsigned(Instrument::Empty) || voice.env>0){if(!voice.sound_locked)for(unsigned i=0;i<5;++i)voice.base_parameters[i]=state.engine_parameters[i];
 for(unsigned i=0;i<5;++i){voice.targets[i]=(i<macro_controls(voice.instrument)?resolve_target_value(*project_,state,t,engine_target(Instrument(voice.instrument),i),voice.base_parameters[i],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value:voice.base_parameters[i]);if(plaits_ && voice.instrument==unsigned(Instrument::PlaitsMacro))plaits_[t].set_parameter(i,voice.targets[i]);}}
 if(voice.instrument==unsigned(Instrument::Sampler) && voice.sampler.active()){float speed=sampler_speed(t,voice,mod);float pitch=powf(2,(float(voice.note)+project_->tuning[voice.note%12]/100+voice.pitch_bend-voice.sample_root)/12);voice.sampler.speed(float(voice.sample.rate)/rate_*pitch*speed);}
 bool pad_fx_layered=project_->morph_enabled || gesture_overlay_.morph_active || gesture_overlay_.count || (control_all_ && ((control_all_id_>=320 && control_all_id_<384) || (control_all_id_>=474 && control_all_id_<554)));
 for(unsigned m=0;m<8 && !pad_fx_layered;++m)pad_fx_layered=project_->macros[m].active || gesture_overlay_.macro_active[m];
 for(const auto& lock:r.active_locks)if(lock.enabled && ((lock.id>=320 && lock.id<384) || (lock.id>=474 && lock.id<554)))pad_fx_layered=true;
 if(r.automation_live)for(const auto& lane:state.automation)if(lane.enabled && ((lane.parameter>=320 && lane.parameter<384) || (lane.parameter>=474 && lane.parameter<554)))pad_fx_layered=true;
 for(auto& p:poly_voices_)if(p.owner==t){auto& v=p.voice;if(v.kit_pad<16 && !v.sound_locked && v.sampler_source_epoch==sampler_source_epoch_[t]){const auto source=track_views_.at(v.source_pattern,t);for(unsigned n=0;n<4;++n)v.kit_insert_target[n]=(pad_fx_layered || v.kit_fx_locked)?resolve_parameter(*project_,source,t,uint16_t(320+v.kit_pad*4+n),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,v.kit_fx_locks,&mod).value:source.kit.pads[v.kit_pad].insert_fx[n];for(unsigned n=0;n<5;++n){uint16_t id=kit_mix_parameter_id(v.kit_pad,n);v.kit_mix_target[n]=(pad_fx_layered || v.kit_mix_locked)?resolve_parameter(*project_,source,t,id,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,v.kit_mix_locks,&mod).value:track_parameter(source,id);}}if(v.instrument==unsigned(Instrument::PlaitsMacro) && !v.sound_locked)for(unsigned i=0;i<5;++i)v.base_parameters[i]=state.engine_parameters[i];if(v.kit_pad>=16)for(unsigned i=0;i<5;++i)v.targets[i]=(i<macro_controls(v.instrument)?resolve_target_value(*project_,state,t,engine_target(Instrument(v.instrument),i),v.base_parameters[i],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value:v.base_parameters[i]);if(auto* dsp=plaits_dsp(t,v))for(unsigned i=0;i<5;++i)dsp->set_parameter(i,v.targets[i]);if(v.kit_pad<16 && v.instrument==unsigned(Instrument::Sampler) && v.sampler.active()){float speed=!v.sound_locked && v.sampler_source_epoch==sampler_source_epoch_[t]?track(t).kit.pads[v.kit_pad].edit.speed:v.kit_speed_value,tuning=project_->tuning[v.note%12];if(v.pitch_bend!=v.kit_speed_bend || tuning!=v.kit_speed_tuning || speed!=v.kit_speed_value){float pitch=powf(2,(float(v.note)+tuning/100+v.pitch_bend-v.sample_root)/12);v.sampler.speed(float(v.sample.rate)/rate_*pitch*speed);v.kit_speed_bend=v.pitch_bend;v.kit_speed_tuning=tuning;v.kit_speed_value=speed;}}else if(v.kit_pad>=16 && v.instrument==unsigned(Instrument::Sampler) && v.sampler.active()){float speed=sampler_speed(t,v,mod);float pitch=powf(2,(float(v.note)+project_->tuning[v.note%12]/100+v.pitch_bend-v.sample_root)/12);v.sampler.speed(float(v.sample.rate)/rate_*pitch*speed);}}
 for(unsigned i=0;i<7;++i)r.compressor_target[i]=resolve_parameter(*project_,state,t,uint16_t(87+i),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value;
 bool insert_layered=project_->morph_enabled || gesture_overlay_.morph_active || gesture_overlay_.count || (control_all_ && control_all_id_>=94 && control_all_id_<=101);
 for(unsigned m=0;m<8 && !insert_layered;++m)insert_layered=project_->macros[m].active || gesture_overlay_.macro_active[m];
 for(const auto& lock:r.active_locks)if(lock.enabled && lock.id>=94 && lock.id<=101)insert_layered=true;
 if(r.automation_live)for(const auto& lane:state.automation)if(lane.enabled && lane.parameter>=94 && lane.parameter<=101)insert_layered=true;
 for(unsigned i=0;i<8;++i)r.insert_target[i]=insert_layered?resolve_parameter(*project_,state,t,uint16_t(94+i),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value:state.insert_fx[i];
 if(state.instrument==Instrument::DaisySynth){const auto& descriptor=*engine_descriptor(unsigned(Instrument::DaisySynth));float values[4],adsr[4];for(unsigned i=0;i<4;++i)values[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::DaisySynth,i),engine_parameter_value(state,i),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;descriptor.bind(descriptor.parameters,values,4,{nullptr,nullptr,nullptr,adsr});if(!voices_[t].sound_locked)voices_[t].daisy.set_adsr(adsr[0],adsr[1],adsr[2],adsr[3]);for(auto& p:poly_voices_)if(p.owner==t && !p.voice.sound_locked)p.voice.daisy.set_adsr(adsr[0],adsr[1],adsr[2],adsr[3]);}
 refresh_cc(t);
}
}
