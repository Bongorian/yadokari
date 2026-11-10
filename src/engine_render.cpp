#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
void Engine::render_ports(float* output,float* cue,const float* input,size_t frames) {
 if(!output) return;
 dsp_profile_.block_begin();render_audit_.generation(generation_);if(render_audit_.clock())event_work_={};
 auto profile_clock=stage_profile_.sample();uint32_t profile_mark=profile_clock?profile_clock():0,profile_control=0,profile_events=0,profile_tracks=0,profile_output=0;
 // Retire queued note-offs before accepting new notes on the same channel.
 for(auto& pad:local_midi_)for(auto& n:pad)if(n.held && n.releasing && midi(uint8_t(0x80|n.channel),n.note,0))n.held=false;
 for(unsigned t=0;t<16;++t)if(pending_midi_off_&(1u<<t))note_off_main(t);
 for(auto& p:poly_voices_)if(p.pending)release_poly(p);
 bool timing_changed=false,snapshot_applied=false; for(unsigned n=0;n<255;++n){const CommandState* front=commands_->peek_front();if(!front)break;const SoundState* payload=nullptr;if(carries_sound(front->type)){if(!sound_commands_->pop(*sound_command_consumer_)){__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);commands_->pop_advance();continue;}payload=sound_command_consumer_;}if(front->restore_morph && !morph_commands_->pop(*morph_command_consumer_)){__atomic_store_n(&project_event_loss_,1,__ATOMIC_RELEASE);commands_->pop_advance();continue;}snapshot_applied|=front->type==CommandType::ApplySnapshot;auto audit_at=render_audit_.command_begin();command(*front,payload);render_audit_.command_end(audit_at,unsigned(front->type),front->track);commands_->pop_advance();timing_changed=true;}
 if(__atomic_exchange_n(&panic_requested_,0,__ATOMIC_ACQUIRE)){auto audit_at=render_audit_.command_begin();command({CommandType::Panic,0,0,0});render_audit_.command_end(audit_at,unsigned(CommandType::Panic),0);timing_changed=true;}
 consume_freeze_capture();
 // A reconnect snapshot must not echo a slave clock. In master mode reserve
 // the complete SPP/Continue pair before sending either part.
 if(transport_resend_){if(external_clock_)transport_resend_=false;else if(midi_events_.free_space()>=(playing_?2u:1u)){if(playing_){midi(0xf2,uint8_t(song_position_&127),uint8_t(song_position_>>7));midi(0xfb,0,0,1);}else midi(0xfc,0,0,1);transport_resend_=false;}}
 // Cache rational periods and deadlines at the control boundary rather than
 // performing sixteen 64-bit divisions at every audio sample.
 // A strip sweep can queue many positions before one audio callback. Apply
 // only the latest position and spread target recomputation across callbacks.
 for(unsigned mi=0;mi<8;++mi)if(project_->macros[mi].active)for(const auto& route:project_->macros[mi].routes)if(route.enabled && route.source!=ModSource::Macro)control_refresh_mask_|=route.tracks;
 for(unsigned budget=0;budget<2 && control_refresh_mask_;++budget){
  unsigned t=control_refresh_cursor_;
  while(!(control_refresh_mask_&(1u<<t)))t=(t+1)&15;
  control_refresh_cursor_=uint8_t((t+1)&15);
  control_refresh_mask_&=uint16_t(~(1u<<t));refresh_parameters(t);
 }
 for(unsigned t=0;t<16;++t){if(timing_changed)update_timing(t);refresh_cc(t);if((expression_resend_mask_&(1u<<t)) && refresh_expression(t))expression_resend_mask_&=uint16_t(~(1u<<t));}
 // Step and explicit parameter changes already refresh effects at their
 // boundary. A 200 Hz pass is sufficient for continuous morph targets.
 if(sample_count_>=effects_refresh_due_){refresh_effects();effects_refresh_due_=sample_count_+rate_/200;}
 for(unsigned ch=0;ch<16;++ch)if(panic_channels_&(1u<<ch)){if(!midi(uint8_t(0xb0|ch),120,0))break;panic_channels_&=uint16_t(~(1u<<ch));}
 if(__atomic_load_n(&capture_state_,__ATOMIC_ACQUIRE)==1) {
  capture_frozen_=capture_active_;capture_active_^=1;captures_[capture_active_].reset();__atomic_store_n(&capture_state_,2,__ATOMIC_RELEASE);event(EngineEventType::CaptureReady,0,0);
 }
 const uint8_t* group_for=group_routing_cache_.resolve(project_->routing.groups);
 const unsigned capture_track_index=capture_track(project_->routing.capture_bus),capture_tap=capture_track_tap(project_->routing.capture_bus),record_track_index=capture_track(live_record_source_),record_tap=capture_track_tap(live_record_source_);
 const uint64_t clock_period=uint64_t(rate_)*60, bar_period=clock_period*16;
 const auto take_state=scene_take_.state();const bool take_session=take_state==SceneTake::Armed || take_state==SceneTake::Recording || take_state==SceneTake::Finishing || take_state==SceneTake::Ready;
 const bool song_session=project_->song.enabled && project_->song.length,arrangement_clock=take_session || song_session;
 if(profile_clock){uint32_t now=profile_clock();profile_control=now-profile_mark;profile_mark=now;}
 prepare_va_candidates();
 render_audit_.control_end();dsp_profile_.control_end(generation_,snapshot_applied);dsp_profile_.cut(DspProfile::Control,unsigned(frames));
 if(!twin_capture_request_ && !playing_ && (freeze_state_==1 || freeze_state_==2))freeze_state_=4;
 for(size_t i=0;i<frames;++i) {
  TimedMidi incoming;for(unsigned budget=0;budget<256 && timed_midi_.peek(incoming);++budget){if(incoming.generation!=generation_ || incoming.epoch!=__atomic_load_n(&midi_epoch_,__ATOMIC_ACQUIRE)){timed_midi_.pop(incoming);continue;}if(incoming.packet.sample>sample_count_)break;timed_midi_.pop(incoming);CommandState midi_command{CommandType::MidiInput,incoming.track,incoming.packet.bytes[1],incoming.packet.bytes[0]};midi_command.scalar=incoming.packet.length==3?incoming.packet.bytes[2]:0;midi_command.parameter=incoming.packet.cable;command(midi_command);}
  repeat_tick();kit_midi_tick();
  if(counting_ && count_phase_>=clock_period){count_phase_-=clock_period;if(count_beat_>=4)command({CommandType::Start,0,0,0});else{++count_beat_;start_click(false);event(EngineEventType::CountInState,0,count_beat_);}}
  if(playing_) {
   if(restart_pending_ && !first_ && bar_phase_+uint64_t(bpm_)*4>=bar_period)command({CommandType::Start,0,0,0});
   bool musical_tick=false;if(arrangement_clock && !first_){arrangement_tick_phase_+=uint64_t(bpm_)*kSongTicksPerBeat;if(arrangement_tick_phase_>=clock_period){arrangement_tick_phase_-=clock_period;++arrangement_tick_;musical_tick=true;}}
   bool beat_boundary=first_;song_phase_+=uint64_t(bpm_)*4;if(song_phase_>=clock_period){song_phase_-=clock_period;song_position_=uint16_t((song_position_+1)&0x3fff);beat_boundary=(song_position_&3)==0;}
   bool boundary=first_;
   if(first_) clock_event(ClockEventType::Tick);
   else { clock_phase_+=uint64_t(bpm_)*24; if(clock_phase_>=clock_period) { clock_phase_-=clock_period; clock_event(ClockEventType::Tick); }
    bar_phase_+=uint64_t(bpm_)*4; if(bar_phase_>=bar_period) { bar_phase_-=bar_period; boundary=true; } }
   first_=false;
   if(performance_fx_quantize_pending_ && (performance_fx_quantize_pending_==1?beat_boundary:boundary)){performance_fx_manual_mode_=performance_fx_pending_mode_;performance_fx_manual_amount_=performance_fx_pending_amount_;performance_fx_applied_=performance_fx_pending_mode_;performance_fx_.set(performance_fx_applied_,performance_fx_pending_amount_);performance_fx_quantize_pending_=0;}
   bool taking=take_session;
   bool song_owner=!taking && song_session && follow_song(boundary,musical_tick);
   if(boundary && playing_) {
    unsigned record_state=__atomic_load_n(&live_record_state_,__ATOMIC_ACQUIRE);
    if(!taking && !song_owner)follow_scene();
    for(unsigned t=0;t<16;++t){if(twin_pending_&(1u<<t))apply_twin_switch(t);}
    twin_pending_=0;
    apply_launches();
    if(scene_take_.state()==SceneTake::Armed)scene_take_.begin(arrangement_tick_,project_->active_scene,project_->clips,captured_mute_mask());
    else if(scene_take_.state()==SceneTake::Finishing)scene_take_.end(arrangement_tick_);
    if(record_state==2 && live_record_bars_ && !--live_record_bars_){__atomic_store_n(&live_record_bar_done_,1,__ATOMIC_RELEASE);__atomic_store_n(&live_record_state_,3,__ATOMIC_RELEASE);}
    if(record_state==4){if(live_record_restart_track_<16){if(audio_lane_mask_&(1u<<live_record_restart_track_))start_audio_clip(live_record_restart_track_,0);else launch_clip(live_record_restart_track_,live_record_restart_pattern_,true);}__atomic_store_n(&live_record_tick_,audio_song_tick(),__ATOMIC_RELEASE);__atomic_store_n(&live_record_state_,2,__ATOMIC_RELEASE);}else if(record_state==5)__atomic_store_n(&live_record_state_,3,__ATOMIC_RELEASE);
   }
   if(audio_lane_mask_ && (boundary || musical_tick))tick_audio_lanes(boundary);
   if(playing_){
   bool parameter_boundary=boundary;for(unsigned t=0;t<16;++t) {
    auto& r=runtime_[t];
    if((audio_lane_mask_&(1u<<t)) || (twin_isolated_ && t==freeze_track_))continue;
    if(r.follow_due && int64_t(sample_count_)>=r.grid)follow_clip(t);
    if(r.playing) for(unsigned budget=0;budget<4 && sample_count_>=r.due && (!r.follow_due || int64_t(sample_count_)>=r.grid);++budget) {if(t==freeze_track_ && (freeze_state_==1 || freeze_state_==2) && r.position==0){if(r.pattern!=freeze_pattern_)freeze_state_=4;else if(freeze_state_==1){freeze_state_=2;freeze_frames_=0;}else freeze_state_=3;}play_step(t);advance(t);parameter_boundary=true;}
    if(r.retriggers && sample_count_>=r.retrigger_at) { if(track(t).arp_mode)trigger_step_arp(t,r.retrigger_velocity,r.retrigger_note,r.effective_step,r.accumulator_offset,r.accumulator_degrees,r.retrigger_index);else trigger_step_chord(t,r.retrigger_velocity,r.retrigger_note,r.effective_step,r.accumulator_offset,r.accumulator_degrees); --r.retriggers;++r.retrigger_index;schedule_retrigger(t); }
   }freeze_write_=freeze_state_==2;if(parameter_boundary)refresh_effects();
   if(arrangement_clock && (scene_take_.state()==SceneTake::Recording || scene_take_.state()==SceneTake::Finishing)){uint64_t tick=arrangement_tick_+(arrangement_tick_phase_>=clock_period/2);if(arrangement_dirty_)scene_take_.observe(tick,project_->active_scene,project_->clips,captured_mute_mask(),arrangement_restarts_);else if(musical_tick)scene_take_.advance(arrangement_tick_);}
   arrangement_dirty_=false;arrangement_restarts_=0;
   }
  }
  for(uint32_t tracks=kit_early_tracks_;tracks;tracks&=tracks-1){unsigned t=unsigned(__builtin_ctz(tracks));const auto& state=track(t);for(uint32_t pads=kit_early_pads_[t];pads;pads&=pads-1){unsigned pad=unsigned(__builtin_ctz(pads));auto& early=kit_early_[t][pad];if(sample_count_>=early.due){const auto& ks=state.kit.steps[pad][early.step];bool audible=playing_ && runtime_[t].playing && state.instrument==Instrument::DrumKit && !audio_muted(t) && (!audio_has_solo() || audio_solo_track(t)) && !state.kit.pads[pad].muted && (!state.kit.solo_mask || (state.kit.solo_mask&(1u<<pad))) && ks.enabled;if(audible){kit_early_started_[t]|=uint16_t(1u<<pad);trigger(t,early.velocity,-1,nullptr,&state,0,false,true,-1,int(pad),&ks);}if(!audible || !--early.remaining){early.remaining=0;kit_early_pads_[t]&=uint16_t(~(1u<<pad));}else early.due+=early.period;}}if(!kit_early_pads_[t])kit_early_tracks_&=uint16_t(~(1u<<t));}
  for(uint32_t tracks=kit_repeat_tracks_;tracks;tracks&=tracks-1){unsigned t=unsigned(__builtin_ctz(tracks));const auto& state=track(t);for(uint32_t pads=kit_repeat_pads_[t];pads;pads&=pads-1){unsigned pad=unsigned(__builtin_ctz(pads));auto& repeat=kit_repeat_[t][pad];if(sample_count_>=repeat.due){const auto& ks=state.kit.steps[pad][repeat.step];trigger(t,repeat.velocity,-1,nullptr,&state,0,false,true,-1,int(pad),&ks);if(!--repeat.remaining)kit_repeat_pads_[t]&=uint16_t(~(1u<<pad));else repeat.due+=repeat.period;}}if(!kit_repeat_pads_[t])kit_repeat_tracks_&=uint16_t(~(1u<<t));}
  gesture_tick();for(unsigned t=0;t<16;++t)follower_[t]+=(follower_next_[t]-follower_[t])*(follower_next_[t]>follower_[t]?0.01f:0.0005f);float master_magnitude=__builtin_fabsf(feedback_input_);follower_master_+=(master_magnitude-follower_master_)*(master_magnitude>follower_master_?0.01f:0.0005f);
  if(profile_clock){uint32_t now=profile_clock();profile_events+=now-profile_mark;profile_mark=now;}
#if GROOVE_DETAILED_RENDER_ATTRIBUTION
  render_audit_.cut(0);
#endif
  dsp_profile_.cut(DspProfile::Events,1);
  // Ownership masks are maintained at every allocation and release; ascending
  // set bits preserve the prior voice mix order.
  unsigned requested=__atomic_load_n(&visual_requested_track_,__ATOMIC_RELAXED),focus=requested&15,visual_source=requested>>8;if(visual_tap_track_!=focus || visual_tap_source_!=visual_source || visual_tap_generation_!=generation_){visual_tap_track_=focus;visual_tap_source_=visual_source;visual_tap_generation_=generation_;visual_build_active_=false;visual_next_=sample_count_;__builtin_memset(visual_taps_,0,sizeof visual_taps_);for(float& level:visual_nodes_)level=0;}InputAudio external{0,0};bool input_valid=true;if(input){external={input[2*i],input[2*i+1]};}else input_valid=input_audio_.pop(external);external.l=isfinite(external.l)?__builtin_fmaxf(-1,__builtin_fminf(1,external.l)):0;external.r=isfinite(external.r)?__builtin_fmaxf(-1,__builtin_fminf(1,external.r)):0;input_gain_+=(input_gain_target_-input_gain_)*.002f;external.l*=input_gain_;external.r*=input_gain_;input_peaks_[0]=__builtin_fmaxf(input_peaks_[0],__builtin_fabsf(external.l));input_peaks_[1]=__builtin_fmaxf(input_peaks_[1],__builtin_fabsf(external.r));external.l=__builtin_fmaxf(-1,__builtin_fminf(1,external.l));external.r=__builtin_fmaxf(-1,__builtin_fminf(1,external.r));tuner_sum_+=tuner_source_==0?external.l:tuner_source_==1?external.r:(external.l+external.r)*.5f;if(++tuner_div_==4){tuner_ring_[tuner_write_++&1023]=tuner_sum_*.25f;tuner_div_=0;tuner_sum_=0;}visual_input_peak_=__builtin_fmaxf(visual_input_peak_,__builtin_fmaxf(__builtin_fabsf(external.l),__builtin_fabsf(external.r)));texture_previous_[1]={external.l,external.r};visual_taps_[0][sample_count_&511]=(external.l+external.r)*.5f;
  float left=0,right=0,cue_l=0,cue_r=0,send_l=0,send_r=0,reverb_l=0,reverb_r=0,track_capture_l=0,track_capture_r=0,track_record_l=0,track_record_r=0;float group_l[8]{},group_r[8]{},group_send_l[8]{},group_send_r[8]{},group_reverb_l[8]{},group_reverb_r[8]{};bool solo=audio_has_solo();
  if(va_helper_ && va_candidate_count_ && (sample_count_&31)){DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::AssistDispatch);dispatch_va_samples();}
  for(unsigned t=0;t<16;++t) {
   auto& v=voices_[t];auto& r=runtime_[t];if(v.off_at && sample_count_>=v.off_at && !hold_for_slide(t,v))note_off_main(t);
   if(track(t).instrument==Instrument::Texture && !v.sound_locked)v.instrument=unsigned(Instrument::Texture);
   if(v.instrument!=unsigned(Instrument::Texture) && v.instrument!=unsigned(Instrument::Resonator) && native_budget_.held(t)){native_budget_.release(t);__atomic_fetch_and(&native_active_,~(1u<<t),__ATOMIC_RELEASE);__atomic_fetch_and(&native_refused_,~(1u<<t),__ATOMIC_RELEASE);}
   float l=0,rr=0,kit_delay_l=0,kit_delay_r=0,kit_reverb_l=0,kit_reverb_r=0;bool held_envelope=v.instrument==unsigned(Instrument::PolySaw) || v.instrument==unsigned(Instrument::FM);if(audio_lane_mask_&(1u<<t)){if(playing_)v.sampler.next(l,rr);int clip=audio_clip_[t];if(clip>=0){l*=project_->audio[t].clips[clip].gain;rr*=project_->audio[t].clips[clip].gain;}}else if(v.instrument==unsigned(Instrument::Sampler))render_sampler_voice(v,l,rr);else render_non_sampler_voice(t,v,l,rr,held_envelope,t);
   for(uint32_t mask=poly_track_mask_[t];mask;mask=uint32_t(mask&(mask-1))){unsigned index=unsigned(__builtin_ctz(mask));auto& p=poly_voices_[index];if(p.sequenced && p.voice.off_at && sample_count_>=p.voice.off_at && !p.pending)release_poly(p);if(p.owner>=16 || p.pending)continue;float pl=0,pr=0;if(p.voice.instrument==unsigned(Instrument::Sampler))render_sampler_voice(p.voice,pl,pr);else render_non_sampler_voice(t,p.voice,pl,pr,true,16+index);if(p.voice.composite){const auto& fx=p.voice.leaf->fx;if(fx.present)for(unsigned n=0;n<2;++n){unsigned base=n*4,type=fx.disabled&(1u<<n)?0:unsigned(fx.inserts[base]);if(type || !p.voice.leaf->inserts[n].idle())process_profiled_insert(p.voice.leaf->inserts[n],pl,pr,type,fx.inserts[base+1],fx.inserts[base+2],fx.inserts[base+3],external.l,external.r,4);}pl*=p.voice.layer_gain*(p.voice.layer_pan>0?1-p.voice.layer_pan:1);pr*=p.voice.layer_gain*(p.voice.layer_pan<0?1+p.voice.layer_pan:1);if(fx.present){kit_delay_l+=pl*fx.delay;kit_delay_r+=pr*fx.delay;kit_reverb_l+=pl*fx.reverb;kit_reverb_r+=pr*fx.reverb;}}
    if(p.voice.kit_pad<16){auto& voice=p.voice;if(voice.kit_insert_target[0]!=0 || !voice.kit_insert.idle()){for(unsigned n=0;n<3;++n)voice.kit_insert_current[n]+=(voice.kit_insert_target[n+1]-voice.kit_insert_current[n])*kit_mix_alpha_;process_profiled_insert(voice.kit_insert,pl,pr,unsigned(voice.kit_insert_target[0]),voice.kit_insert_current[0],voice.kit_insert_current[1],voice.kit_insert_current[2],external.l,external.r,4);}voice.kit_mix.process(pl,pr,voice.kit_mix_target[2],voice.kit_mix_target[0],voice.kit_mix_target[1],voice.kit_mix_target[3],voice.kit_mix_target[4],kit_mix_alpha_,kit_delay_l,kit_delay_r,kit_reverb_l,kit_reverb_r);}
    l+=pl;rr+=pr;bool active=p.voice.instrument==unsigned(Instrument::ExternalMidi)?p.voice.midi_note:p.voice.instrument==unsigned(Instrument::Sampler)?p.voice.sampler.active():p.voice.instrument==unsigned(Instrument::DaisySynth)?p.voice.daisy.active():p.voice.env>=0.00001f;if(!active){if(auto* dsp=plaits_dsp(t,p.voice))dsp->stop();capture_voice_off(p.voice);set_poly_owner(p,255);}}
   const bool measure_node=t==visual_tap_track_ && !(sample_count_&31);auto node=[&](unsigned n){if(measure_node)visual_nodes_[n]=fmaxf(visual_nodes_[n],fmaxf(fabsf(l),fabsf(rr)));};const float dry_l=l,dry_r=rr;texture_next_[19+t]={l,rr};
   for(unsigned p=0;p<kTrackParameters;++p){r.current[p]+=(r.target[p]-r.current[p])*0.005f;}r.reverb_current+=(r.reverb_target-r.reverb_current)*0.005f;
   r.lfo_phase=r.tempo_lfo.next(r.lfo_phase,r.current[7],inverse_rate_,playing_,lfo_transport_beat_,lfo_transport_fraction_,clock_period);
   float cutoff;bool chain_live=l!=0 || rr!=0 || __builtin_fabsf(r.filter_l)>0.000001f || __builtin_fabsf(r.filter_r)>0.000001f || !r.insert[0].idle() || !r.insert[1].idle() || r.compressor_target[5]>0.000001f || r.compressor_current[5]>0.000001f;const bool frozen_chain=track(t).frozen_asset<kSampleAssets;if(chain_live && !frozen_chain){
   r.pressure+=(r.pressure_target-r.pressure)*0.005f;r.pressure_depth+=(r.pressure_depth_target-r.pressure_depth)*0.005f;r.strip_mod_current+=(r.strip_mod-r.strip_mod_current)*0.005f;float lfo_depth=r.current[6]+r.strip_mod_current;if(lfo_depth>1)lfo_depth=1;cutoff=modulated_cutoff(r.current[2],lfo_depth,r.lfo_phase,r.pressure,r.pressure_depth);
   const auto& follower=track(t).creative;follower_amount_[t]+=(float(follower.follower_amount)/100-follower_amount_[t])*0.005f;if(follower_amount_[t]!=0)cutoff=clamp_cutoff(cutoff+(follower.follower_source?follower_[follower.follower_source-1]:follower_master_)*follower_amount_[t]*2);
   {DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Filter);if(cutoff<0.999f) { float coefficient=0.001f+cutoff*cutoff*0.95f; r.filter_l+=(l-r.filter_l)*coefficient; r.filter_r+=(rr-r.filter_r)*coefficient; l=r.filter_l; rr=r.filter_r; } else { r.filter_l=l; r.filter_r=rr; }}
   node(0);{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Drive);l=r.drive.process(l,r.current[3]); rr=r.drive.process(rr,r.current[3]);}node(1);
   if(r.current[5]>0.001f) { if(r.crush_steps==0 || __builtin_fabsf(r.current[5]-r.crush_amount)>0.0001f){r.crush_amount=r.current[5];r.crush_steps=powf(2.0f,16-r.current[5]*12);} l=__builtin_floorf(l*r.crush_steps+0.5f)/r.crush_steps; rr=__builtin_floorf(rr*r.crush_steps+0.5f)/r.crush_steps; }
   node(2);auto& comp=r.compressor_current;if(comp[5]<=0.000001f && r.compressor_target[5]<=0.000001f){if(r.compressor_active){track_compressors_[t].reset();r.compressor_active=false;}for(unsigned p=0;p<7;++p)comp[p]=r.compressor_target[p];}
   else{for(unsigned p=0;p<7;++p)if(p==6)comp[p]=r.compressor_target[p];else comp[p]+=(r.compressor_target[p]-comp[p])*0.005f;unsigned key_track=unsigned(comp[6]+0.5f);if(key_track>16)key_track=16;float detector=key_track?follower_[key_track-1]:-1;{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::TrackCompressor);track_compressors_[t].process(l,rr,detector,comp[0],comp[1],comp[2],comp[3],comp[4],comp[5]);}r.compressor_active=true;}
   node(3);for(unsigned slot=0;slot<2;++slot){unsigned base=slot*4;auto& effect=r.insert[slot];auto& current=r.insert_current;auto& target=r.insert_target;current[base]=target[base];if(current[base]==0 && effect.idle()){for(unsigned p=1;p<4;++p)current[base+p]=target[base+p];continue;}for(unsigned p=1;p<4;++p)current[base+p]+=(target[base+p]-current[base+p])*0.005f;process_profiled_insert(effect,l,rr,(track(t).insert_disabled>>slot)&1?0:unsigned(current[base]),current[base+1],current[base+2],current[base+3],external.l,external.r,slot);}
   }if(frozen_chain){r.filter_l=l;r.filter_r=rr;for(unsigned slot=0;slot<2;++slot){unsigned base=slot*4;r.insert_current[base]=0;for(unsigned p=1;p<4;++p)r.insert_current[base+p]=r.insert_target[base+p];}}node(4);const float insert_l=l,insert_r=rr;if((!chain_live || frozen_chain) && r.compressor_active){track_compressors_[t].reset();r.compressor_active=false;}
   // Keep freeze and POST INSERT taps before the trim: a frozen track retains
   // its preset gain, so baking the gain there would apply it twice on replay.
   const float trim=track(t).output_trim;
   if(trim!=1){l*=trim;rr*=trim;kit_delay_l*=trim;kit_delay_r*=trim;kit_reverb_l*=trim;kit_reverb_r*=trim;}
   const bool audible=!audio_muted(t) && (!solo || audio_solo_track(t)) && (!(audio_lane_mask_&(1u<<t)) || !(song_mute_mask_&(1u<<t)));r.audible_gain+=((audible?1.0f:0.0f)-r.audible_gain)*0.01f;
   float pan=r.current[1],level=r.current[0]*r.audible_gain,pan_l=pan>0?1-pan:1,pan_r=pan<0?1+pan:1;for(unsigned bus=0;bus<2;++bus)r.send_pre[bus]+=((track(t).send_prefader&(1u<<bus)?1.0f:0.0f)-r.send_pre[bus])*0.005f;float delay_gain=r.audible_gain*(r.current[0]+(1-r.current[0])*r.send_pre[0]),reverb_gain=r.audible_gain*(r.current[0]+(1-r.current[0])*r.send_pre[1]);float delay_track_l=l*pan_l*delay_gain,delay_track_r=rr*pan_r*delay_gain,reverb_track_l=l*pan_l*reverb_gain,reverb_track_r=rr*pan_r*reverb_gain;kit_delay_l*=delay_gain*pan_l;kit_delay_r*=delay_gain*pan_r;kit_reverb_l*=reverb_gain*pan_l;kit_reverb_r*=reverb_gain*pan_r;l*=level*pan_l;rr*=level*pan_r;node(5);
   if(twin_isolated_ && t==freeze_track_){twin_capture_frame(dry_l,dry_r);texture_next_[3+t]=texture_next_[19+t]=texture_next_[35+t]={0,0};follower_next_[t]=0;continue;}
   if(twin_fade_[t]){float old=float(twin_fade_[t]--)/64; l=l*(1-old)+twin_tail_l_[t]*old;rr=rr*(1-old)+twin_tail_r_[t]*old;}twin_last_l_[t]=l;twin_last_r_[t]=rr;
   texture_next_[3+t]={l,rr};texture_next_[35+t]={insert_l,insert_r};
   if(capture_track_index==t){track_capture_l=capture_tap==1?dry_l:capture_tap==2?insert_l:l;track_capture_r=capture_tap==1?dry_r:capture_tap==2?insert_r:rr;}if(record_track_index==t){track_record_l=record_tap==1?dry_l:record_tap==2?insert_l:l;track_record_r=record_tap==1?dry_r:record_tap==2?insert_r:rr;}if(!twin_capture_request_ && freeze_write_ && freeze_state_==2 && t==freeze_track_){if(freeze_frames_<freeze_capacity_){freeze_buffer_[freeze_frames_*2]=pcm16(insert_l);freeze_buffer_[freeze_frames_*2+1]=pcm16(insert_r);++freeze_frames_;}else freeze_state_=4;}
   float magnitude=__builtin_fabsf(l)>__builtin_fabsf(rr)?__builtin_fabsf(l):fabsf(rr);if(magnitude>visual_peak_[t])visual_peak_[t]=magnitude;
   if(t==visual_tap_track_){visual_taps_[1][sample_count_&511]=(l+rr)*.5f;}follower_next_[t]=magnitude;visual_wave_[t][sample_count_%64]=(l+rr)*0.5f;
   unsigned group=group_for[t];if(group<8){if(track(t).output_bus){group_l[group]+=l;group_r[group]+=rr;group_send_l[group]+=delay_track_l*r.current[4]+kit_delay_l;group_send_r[group]+=delay_track_r*r.current[4]+kit_delay_r;group_reverb_l[group]+=reverb_track_l*r.reverb_current+kit_reverb_l;group_reverb_r[group]+=reverb_track_r*r.reverb_current+kit_reverb_r;}}else{if(track(t).output_bus&1){left+=l;right+=rr;send_l+=delay_track_l*r.current[4]+kit_delay_l;send_r+=delay_track_r*r.current[4]+kit_delay_r;reverb_l+=reverb_track_l*r.reverb_current+kit_reverb_l;reverb_r+=reverb_track_r*r.reverb_current+kit_reverb_r;}if(track(t).output_bus&2){cue_l+=l;cue_r+=rr;}}
  }
  if(profile_clock){uint32_t now=profile_clock();profile_tracks+=now-profile_mark;profile_mark=now;}
#if GROOVE_DETAILED_RENDER_ATTRIBUTION
  render_audit_.cut(1);
#endif
  dsp_profile_.cut(DspProfile::Tracks,1);
  for(unsigned g=0;g<8;++g){const auto& bus=project_->routing.groups[g];auto& current=group_fx_current_[g];current[10]+=(group_fx_target_[g][10]-current[10])*0.005f;if(group_fx_target_[g][10]==0 && current[10]<0.000001f)current[10]=0;float dry_group_l=group_l[g],dry_group_r=group_r[g],first_l=0,first_r=0;for(unsigned slot=0;slot<2;++slot){if(slot==1){first_l=group_l[g];first_r=group_r[g];if(current[10]>0){group_l[g]+=(dry_group_l-group_l[g])*current[10];group_r[g]+=(dry_group_r-group_r[g])*current[10];}}unsigned f=slot*4,type=unsigned(group_fx_target_[g][f]);auto& insert=group_inserts_[g][slot];if(type || !insert.idle()){for(unsigned j=1;j<4;++j)current[f+j]+=(group_fx_target_[g][f+j]-current[f+j])*0.005f;process_profiled_insert(insert,group_l[g],group_r[g],type,current[f+1],current[f+2],current[f+3],external.l,external.r,2+slot);}}if(current[10]>0){group_l[g]+=(first_l-group_l[g])*(0.5f*current[10]);group_r[g]+=(first_r-group_r[g])*(0.5f*current[10]);}group_gain_[g]+=(bus.gain-group_gain_[g])*0.005f;current[8]+=(group_fx_target_[g][8]-current[8])*0.005f;current[9]+=(group_fx_target_[g][9]-current[9])*0.005f;if(bus.output&1){float gl=group_l[g]*group_gain_[g],gr=group_r[g]*group_gain_[g];left+=gl;right+=gr;send_l+=group_send_l[g]*group_gain_[g]+gl*current[8];send_r+=group_send_r[g]*group_gain_[g]+gr*current[8];reverb_l+=group_reverb_l[g]*group_gain_[g]+gl*current[9];reverb_r+=group_reverb_r[g]*group_gain_[g]+gr*current[9];}if(bus.output&2){cue_l+=group_l[g]*group_gain_[g];cue_r+=group_r[g]*group_gain_[g];}}
  for(unsigned e=0;e<13;++e)if(e==12)effects_current_[e]=effects_target_[e];else effects_current_[e]+=(effects_target_[e]-effects_current_[e])*0.005f;
  float wet_l=0,wet_r=0;
  {float peak=__builtin_fmaxf(__builtin_fabsf(send_l),__builtin_fabsf(send_r));visual_delay_input_=__builtin_fmaxf(visual_delay_input_,peak);if(peak>delay_env_)delay_env_=peak;else delay_env_*=0.99995f;
   if(!delay_sleep_ || peak>0.0001f){if(delay_sleep_){delay_.reset();delay_sleep_=false;}{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Delay);delay_.process(send_l,send_r,effects_current_[1],unsigned(effects_current_[2]),wet_l,wet_r);}float wpeak=__builtin_fmaxf(__builtin_fabsf(wet_l),__builtin_fabsf(wet_r));if(wpeak>delay_env_)delay_env_=wpeak;if(peak<=0.0001f && delay_env_<0.0001f)delay_sleep_=true;}
   left+=wet_l*effects_current_[0];right+=wet_r*effects_current_[0];}
  wet_l=wet_r=0;
  {float peak=__builtin_fmaxf(__builtin_fabsf(reverb_l),__builtin_fabsf(reverb_r));if(peak>reverb_env_)reverb_env_=peak;else reverb_env_*=0.99995f;
   if(!reverb_sleep_ || peak>0.0001f){if(reverb_sleep_){reverb_.reset();reverb_sleep_=false;}{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Reverb);reverb_.process(reverb_l,reverb_r,effects_current_[4],effects_current_[5],wet_l,wet_r);}float wpeak=__builtin_fmaxf(__builtin_fabsf(wet_l),__builtin_fabsf(wet_r));if(wpeak>reverb_env_)reverb_env_=wpeak;if(peak<=0.0001f && reverb_env_<0.0001f)reverb_sleep_=true;}
   left+=wet_l*effects_current_[3];right+=wet_r*effects_current_[3];}
  const float returns_l=left,returns_r=right;
  float pl,pr;bool was_previewing=preview_.active();preview_.next(pl,pr);if(was_previewing && !preview_.active())__atomic_store_n(&preview_state_,preview_ticket_<<1,__ATOMIC_RELEASE);if(diagnostic_remaining_){float tone=sinf(diagnostic_phase_)*diagnostic_gain_*__builtin_fminf(1,float(diagnostic_remaining_)/240);diagnostic_phase_+=6.2831853f*440/rate_;if(diagnostic_phase_>=6.2831853f)diagnostic_phase_-=6.2831853f;--diagnostic_remaining_;left+=tone;right+=tone;cue_l+=tone;cue_r+=tone;}cue_l+=pl*project_->routing.preview;cue_r+=pr*project_->routing.preview;if(project_->routing.monitor_bus&1){left+=external.l*project_->routing.monitor;right+=external.r*project_->routing.monitor;}if(project_->routing.monitor_bus&2){cue_l+=external.l*project_->routing.monitor;cue_r+=external.r*project_->routing.monitor;}
  {DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Throws);performance_throws_.process(left,right,performance_fx_.delay_send(),performance_fx_.reverb_send(),performance_fx_.delay_return(),performance_fx_.reverb_return(),effects_current_[1],unsigned(effects_current_[2]),effects_current_[4],effects_current_[5],performance_fx_.reverb_freeze());}
  float history_l=left,history_r=right;bool history_frozen;{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::BeatRepeat);history_frozen=beat_repeat_.process(left,right,performance_history_,rate_,bpm_,stutter_,stutter_subdivision_);}
  {DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Performance);performance_fx_.process(left,right,performance_history_,bpm_);}
  const float performance_l=left,performance_r=right;
  unsigned compressor_key=unsigned(effects_current_[12]+0.5f);if(compressor_key>16)compressor_key=16;float key=compressor_key?follower_next_[compressor_key-1]:-1;{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::MasterCompressor);compressor_.process(left,right,key,effects_current_[6],effects_current_[7],effects_current_[8],effects_current_[9],effects_current_[10],effects_current_[11]);}
  left*=project_->routing.main;right*=project_->routing.main;cue_l*=project_->routing.cue;cue_r*=project_->routing.cue;
  // The click is audible but excluded from capture/history. Apply the same
  // limiter gain to the clean bus so capture matches the audible bus when
  // click is disabled, including during overload.
  float clean_l=left,clean_r=right,clean_cue_l=cue_l,clean_cue_r=cue_r;
  if(click_remaining_){float click=sinf(click_phase_)*0.15f*float(click_remaining_)/float(click_length_);click_phase_+=click_increment_;if(click_phase_>6.2831853f)click_phase_-=6.2831853f;--click_remaining_;if(project_->ui.click_main){left+=click*project_->routing.main;right+=click*project_->routing.main;}else{cue_l+=click*project_->routing.cue;cue_r+=click*project_->routing.cue;}}
  float main_gain,cue_gain;{DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::Limiter);main_gain=main_limiter_.process(left,right,visual_invalid_audio_);cue_gain=cue_limiter_.process(cue_l,cue_r,visual_invalid_audio_);}
  clean_l=isfinite(clean_l)?clean_l*main_gain:0;clean_r=isfinite(clean_r)?clean_r*main_gain:0;clean_cue_l=isfinite(clean_cue_l)?clean_cue_l*cue_gain:0;clean_cue_r=isfinite(clean_cue_r)?clean_cue_r*cue_gain:0;
  visual_limit_main_=__builtin_fmaxf(visual_limit_main_,1-main_gain);visual_limit_cue_=__builtin_fmaxf(visual_limit_cue_,1-cue_gain);visual_cue_peak_=__builtin_fmaxf(visual_cue_peak_,__builtin_fmaxf(__builtin_fabsf(cue_l),__builtin_fabsf(cue_r)));
  float master=__builtin_fmaxf(__builtin_fabsf(left),__builtin_fabsf(right));if(master>visual_master_peak_)visual_master_peak_=master;visual_master_wave_[sample_count_%64]=(left+right)*0.5f;visual_taps_[2][sample_count_&511]=(left+right)*.5f;
  if(!history_frozen){performance_history_.push(history_l,history_r);}feedback_input_=(clean_l+clean_r)*0.5f;output[2*i]=left;output[2*i+1]=right;if(cue){cue[2*i]=cue_l;cue[2*i+1]=cue_r;}
  texture_next_[0]={clean_l,clean_r};texture_next_[2]={clean_cue_l,clean_cue_r};texture_next_[51]={returns_l,returns_r};texture_next_[52]={performance_l,performance_r};for(unsigned g=0;g<8;++g)texture_next_[53+g]={group_l[g]*group_gain_[g],group_r[g]*group_gain_[g]};for(unsigned source=0;source<61;++source)if(source!=1)texture_previous_[source]=texture_next_[source];if(visual_tap_source_<=60){const auto& bus=texture_next_[visual_tap_source_];visual_taps_[1][sample_count_&511]=visual_tap_source_==1?(external.l+external.r)*.5f:(bus.l+bus.r)*.5f;}
  {DspTimingScope timer(dsp_profile_,DspProfile::system_base+DspProfile::RecordCapture);auto capture_channel=[&](unsigned source,bool right){return source==1?(right?external.r:external.l):source==2?(right?clean_cue_r:clean_cue_l):source>=3 && source<=50?(right?track_capture_r:track_capture_l):source==51?(right?returns_r:returns_l):source==52?(right?performance_r:performance_l):source>=53 && source<=60?(right?group_r[source-53]:group_l[source-53])*group_gain_[source-53]:(right?clean_r:clean_l);};captures_[capture_active_].push(capture_channel(project_->routing.capture_bus,false),capture_channel(project_->routing.capture_bus,true));unsigned record_state=__atomic_load_n(&live_record_state_,__ATOMIC_RELAXED);if((sample_count_&255u)==0){unsigned used=kLiveRecordCapacity-live_record_queue_->free_space();if(used>__atomic_load_n(&live_record_highwater_,__ATOMIC_RELAXED))__atomic_store_n(&live_record_highwater_,used,__ATOMIC_RELEASE);}if(record_state!=2 && record_state!=5){record_write_=0;record_column_count_=0;record_columns_total_=0;record_peak_=0;}if(record_state==2 || record_state==5){const bool input_in_record=live_record_source_==1 || (project_->routing.monitor>0 && (((live_record_source_==0 || live_record_source_==52) && (project_->routing.monitor_bus&1)) || (live_record_source_==2 && (project_->routing.monitor_bus&2))));if(input_in_record && !input && (!input_valid || input_audio_dropped()!=live_record_input_drops_)){__atomic_store_n(&live_record_failed_,!input_valid?2u:3u,__ATOMIC_RELEASE);__atomic_store_n(&live_record_state_,3,__ATOMIC_RELEASE);}else{auto record_channel=[&](bool right){unsigned source=live_record_source_;return source==1?(right?external.r:external.l):source==2?(right?clean_cue_r:clean_cue_l):source>=3 && source<=50?(right?track_record_r:track_record_l):source==51?(right?returns_r:returns_l):source==52?(right?performance_r:performance_l):source>=53 && source<=60?(right?group_r[source-53]:group_l[source-53])*group_gain_[source-53]:(right?clean_r:clean_l);};LiveRecordFrame recorded{record_channel(false),record_channel(true)};record_peak_=__builtin_fmaxf(record_peak_,__builtin_fmaxf(__builtin_fabsf(recorded.l),__builtin_fabsf(recorded.r)));if(++record_column_count_>=1024){record_column_count_=0;record_wave_[record_write_]=record_peak_;record_peak_=0;record_write_=(record_write_+1)&511;++record_columns_total_;}if(!live_record_queue_->push(recorded)){__atomic_store_n(&live_record_failed_,4u,__ATOMIC_RELEASE);__atomic_store_n(&live_record_state_,3,__ATOMIC_RELEASE);}else if(live_record_bounded_ && --live_record_remaining_==0)__atomic_store_n(&live_record_state_,3,__ATOMIC_RELEASE);}} if(counting_)count_phase_+=bpm_;if(playing_){lfo_transport_fraction_+=bpm_;if(lfo_transport_fraction_>=clock_period){lfo_transport_fraction_-=clock_period;++lfo_transport_beat_;}}
  ++sample_count_;}
  // Spread visual preparation across samples so a 125 us USB audio callback
  // never has to build all 16 tracks in one burst. A project replacement
  // invalidates any in-progress snapshot before it can reach the UI.
  if(visual_build_active_ && visual_build_.generation!=generation_)visual_build_active_=false;
  if(!visual_build_active_ && sample_count_>=visual_next_){
   visual_next_=sample_count_+rate_/20;auto& frame=visual_build_;frame.preview_ticket=preview_ticket_;frame.preview_position=preview_.normalized_position();frame.preview_active=preview_.active();frame.delay_input=visual_delay_input_;visual_delay_input_=0;frame.sample=sample_count_;frame.taps.sample=sample_count_;frame.taps.generation=generation_;frame.taps.track=visual_tap_track_;frame.taps.source=visual_tap_source_;frame.taps.rate=rate_;frame.limiter_main=visual_limit_main_;frame.limiter_cue=visual_limit_cue_;frame.invalid_audio=visual_invalid_audio_;frame.generation=generation_;frame.rate=rate_;for(unsigned e=0;e<13;++e)frame.effects[e]=effects_current_[e];frame.compressor_reduction=compressor_.reduction();frame.compressor_input=compressor_.detector();frame.performance_fx_phase=performance_fx_.visual_phase();frame.performance_fx_actual_mode=uint8_t(performance_fx_.visual_mode());for(unsigned t=0;t<16;++t){for(unsigned i=0;i<7;++i)frame.track_compressor[t][i]=runtime_[t].compressor_current[i];frame.track_compressor_reduction[t]=track_compressors_[t].reduction();frame.track_compressor_input[t]=track_compressors_[t].detector();}frame.gesture_phase=float(gesture_phase_)/(uint64_t(rate_)*60*project_->gesture.ticks);frame.gesture_morph=gesture_overlay_.morph_active;frame.morph_position=gesture_overlay_.morph_active?gesture_overlay_.morph:project_->morph_position;for(unsigned m=0;m<8;++m){frame.macro_active[m]=gesture_overlay_.macro_active[m];frame.macro_value[m]=gesture_overlay_.macro_active[m]?gesture_overlay_.macros[m]:project_->macros[m].value;}frame.playing=playing_;frame.performance_fx_pending=performance_fx_quantize_pending_!=0;frame.performance_fx_mode=uint8_t(performance_fx_applied_);frame.performance_fx_amount=performance_fx_manual_mode_==PerformanceFx::Off?performance_fx_auto_amount_:performance_fx_manual_amount_;frame.song_position=song_position_;frame.input_peak=visual_input_peak_;for(unsigned n=0;n<2;++n){frame.input_lr[n]=input_peaks_[n];input_peaks_[n]=0;}for(unsigned n=0;n<1024;++n)frame.tuner_samples[n]=tuner_ring_[(tuner_write_+n)&1023];frame.cue_peak=visual_cue_peak_;visual_input_peak_=visual_cue_peak_=0;frame.master_peak=visual_master_peak_;visual_master_peak_=0;for(unsigned n=0;n<64;++n)frame.master_wave[n]=visual_master_wave_[(sample_count_+n)%64];for(unsigned n=0;n<512;++n)frame.record_wave[n]=record_wave_[(record_write_+n)&511];frame.record_write=record_write_;frame.record_columns=record_columns_total_;{unsigned rs=__atomic_load_n(&live_record_state_,__ATOMIC_RELAXED);frame.record_active=rs==2 || rs==5;frame.song_section=song_section_;frame.song_bar=song_bars_;frame.song_bars=song_section_bars_;frame.song_tick=song_elapsed_ticks_;frame.song_ticks=song_duration_ticks_;frame.song_snapshot=song_snapshot_;frame.song_muted=song_mute_mask_;frame.song_solo=snapshot_solo_;frame.song_active=playing_ && !scene_take_.active() && project_->song.enabled && project_->song.length && !song_finished_;}frame.beat=float(bar_phase_%(clock_period*4))/float(clock_period*4);frame.bar_phase=float(bar_phase_%(clock_period*16))/float(clock_period*16);
   visual_build_track_=0;visual_build_active_=true;
  }
  if(visual_build_active_){
   auto audit_visual_clock=render_audit_.clock();unsigned audit_visual_at=audit_visual_clock?audit_visual_clock():0;
   auto visual_clock=dsp_profile_.control_clock();unsigned visual_started=visual_clock?visual_clock():0;
   auto& frame=visual_build_;
   unsigned t=visual_build_track_++;for(unsigned tap=0;tap<3;++tap)for(unsigned n=0;n<32;++n)frame.taps.pcm[tap][t*32+n]=visual_taps_[tap][(frame.taps.sample+t*32+n)&511];frame.multitimbral=multitimbral_;frame.module_reason[t]=module_reason_[t];frame.module_slot[t]=module_slot_[t];frame.attached_instrument[t]=uint8_t(track(t).instrument);frame.layer_failure[t]=layer_failure_[t];{auto& r=runtime_[t];const bool plain=plain_visual_parameters(t);ModRuntime mod=plain?ModRuntime{}:mod_runtime(t);Voice* display_voice=&voices_[t];if(live_poly_capable(track(t).instrument)){uint64_t newest=0;bool found=false;for(auto& poly:poly_voices_)if(poly.owner==t && ((poly.voice.instrument==unsigned(Instrument::Sampler) && poly.voice.sampler.active()) || (poly.voice.instrument!=unsigned(Instrument::Sampler) && poly.voice.env>=0.00001f)) && (!found || poly.age>=newest)){display_voice=&poly.voice;newest=poly.age;found=true;}}auto& v=*display_voice;for(unsigned i=0;i<7;++i){const auto target=engine_target(Instrument::Sampler,i);const float base=(v.sound_locked?v.base_sample_edit:track(t).sample_edit).*sample_parameter_member(i);frame.sample_values[t][i]=plain?clamp_target(target,base):resolve_target_value(*project_,track(t),t,target,base,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;}for(unsigned i=0;i<8;++i)frame.track_insert[t][i]=r.insert[i/4].idle()?r.insert_target[i]:r.insert_current[i];frame.reverb_send[t]=r.reverb_current;for(unsigned n=0;n<6;++n){frame.node_activity[t][n]=t==visual_tap_track_?visual_nodes_[n]:0;if(t==visual_tap_track_)visual_nodes_[n]=0;}frame.peak[t]=visual_peak_[t];visual_peak_[t]=0;frame.envelope[t]=v.env;publish_voice_visual(frame,t,v);v.wavetable.visual_controls(frame.wavetable_controls[t]);v.additive.spectrum(frame.harmonics[t]);frame.wavetable_position[t]=v.wavetable.position();frame.wavetable_phase[t]=v.wavetable.scan_phase();for(unsigned slot=0;slot<2;++slot){r.insert[slot].vocoder_levels(frame.vocoder_bands[t][slot]);frame.insert_activity[t][slot]=r.insert[slot].activity();if(t<8){group_inserts_[t][slot].vocoder_levels(frame.group_vocoder_bands[t][slot]);frame.group_insert_activity[t][slot]=group_inserts_[t][slot].activity();}}frame.pitch_bend[t]=r.expression_bend;frame.expression_pressure[t]=uint8_t(r.pressure_target*127+0.5f);frame.expression_pattern[t]=r.pattern;frame.pressure_depth[t]=r.pressure_depth;frame.lfo_phase[t]=r.lfo_phase;frame.lfo_synced[t]=r.tempo_lfo.active;frame.lfo_division[t]=r.tempo_lfo.division;frame.sample_position[t]=v.sample_origin+v.sampler.normalized_position()*v.sample_extent;frame.sample_slot[t]=v.sample_slot;frame.sample_active[t]=v.sampler.active();for(unsigned lane=0;lane<6;++lane)frame.lanes[t][lane]=creative_[t].lanes[lane];frame.creative_position[t]=creative_[t].position;frame.creative_second[t]=creative_[t].second_position;frame.gene_size[t]=creative_[t].grain;frame.chip_clock[t]=creative_[t].clock;frame.chip_divider[t]=creative_[t].divider;for(unsigned b=0;b<16;++b)frame.spectrum[t][b]=creative_[t].spectrum[b];frame.texture_frozen[t]=textures_ && textures_[t].frozen();frame.note[t]=v.note;frame.instrument[t]=v.instrument;frame.pattern[t]=v.source_pattern;frame.model[t]=v.model;for(unsigned p=0;p<5;++p)frame.parameters[t][p]=v.parameters[p];for(unsigned p=0;p<8;++p){frame.track_parameters[t][p]=r.current[p];frame.sources[t][p]=plain?0:uint8_t(resolve_parameter(*project_,track(t),t,uint16_t(p+1),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).sources);}for(unsigned i=0;i<8;++i){uint32_t key=__atomic_load_n(&visual_control_request_[i],__ATOMIC_RELAXED);if((key>>16)==t){uint16_t target=uint16_t(key);frame.controls_target[i]=key;frame.controls_value[i]=target?resolve_parameter(*project_,track(t),t,target,r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern,nullptr,nullptr,&mod).value:0;}}
   for(unsigned n=0;n<64;++n)frame.waveform[t][n]=visual_wave_[t][(sample_count_+n)%64];}
   if(visual_build_track_==16){
   if(visual_frames_.push(frame)){visual_limit_main_=visual_limit_cue_=0;visual_invalid_audio_=false;}
    visual_build_active_=false;
   }
   if(visual_clock){dsp_profile_.visual_work(visual_clock()-visual_started);}
   if(audit_visual_clock){render_audit_.visual(audit_visual_clock()-audit_visual_at);}
  }
  if(profile_clock){uint32_t now=profile_clock();profile_output+=now-profile_mark;profile_mark=now;}
#if GROOVE_DETAILED_RENDER_ATTRIBUTION
  render_audit_.cut(2);
#endif
  dsp_profile_.cut(DspProfile::Output,1);
 }
 publish_freeze_capture();
 if(dsp_profile_.clock()){unsigned main=0,poly=0;for(const auto& v:voices_)main+=v.env>=.00001f;for(const auto& p:poly_voices_)poly+=p.owner<16 && p.voice.env>=.00001f;dsp_profile_.population(main,poly,layer_failure_);}
 dsp_profile_.block_end(unsigned(frames));
 if(profile_clock){stage_profile_.record(profile_control,profile_events,profile_tracks,profile_output);render_audit_.stages(profile_events,profile_tracks,profile_output);}
 prepare_sound_payloads();
 render_audit_.finish(generation_);
}

// Prepare only immutable notification values. The track mount and every audible
// action remain at the original sample; one unpublished 16KiB span is the limit.
void Engine::prepare_sound_payloads(){
 if(!attached_payload_)return;
 if(sound_payload_plan_dirty_){
  uint16_t wanted[48];unsigned count=0;const auto& song=project_->song;
  unsigned next=0;if(playing_ && !song_armed_){next=song_section_+1;if(next>=song.length)next=song.loop?0:song_section_;}
  for(unsigned t=0;t<16;++t){const auto& twin=project_->twins[t];if((twin_pending_&(1u<<t)) && twin.prepared())wanted[count++]=twin.active?twin.source:twin.sampled;unsigned slot=track_views_.at(runtime_[t].pattern,t).sound_override;if(slot<kPresetSlots)wanted[count++]=uint16_t(slot);
   unsigned pattern=runtime_[t].pattern;
   if(pending_pattern_[t]>=0)pattern=unsigned(pending_pattern_[t]);
   else if(pending_scene_>=0)pattern=project_->scenes[unsigned(pending_scene_)].pattern_for_track[t];
   else if(song.enabled && song.length && next<song.length){const auto& section=song.sections[next];pattern=section.ticks?section.patterns[t]:project_->scenes[section.scene].pattern_for_track[t];}
   slot=track_views_.at(pattern,t).sound_override;if(slot<kPresetSlots)wanted[count++]=uint16_t(slot);
  }
  attached_payload_->request_prepare(wanted,count,sound_payload_epoch_);sound_payload_plan_dirty_=false;
 }
 attached_payload_->prepare_one(project_->sounds,sound_payload_epoch_);
}

// Canonical voice addresses remain stable; only typed private VA state is copied.
// Cache an empty VA order until ownership/trigger/command changes. Populated
// orders still rebuild every quantum, including naturally finished envelopes.
void Engine::prepare_va_candidates(){
 if(!va_candidates_dirty_ && !va_candidate_count_)return;
 va_candidates_dirty_=false;va_candidate_count_=0;if(!va_helper_)return;
 for(unsigned t=0;t<16;++t){auto& v=voices_[t];if(!(audio_lane_mask_&(1u<<t)) && v.instrument==unsigned(Instrument::VirtualAnalog) && v.env>0)va_candidates_[va_candidate_count_++]={&v,t};
  for(uint32_t mask=poly_track_mask_[t];mask;mask&=mask-1){auto& p=poly_voices_[unsigned(__builtin_ctz(mask))];if(!p.pending && p.owner<16 && p.voice.instrument==unsigned(Instrument::VirtualAnalog) && p.voice.env>0)va_candidates_[va_candidate_count_++]={&p.voice,16+unsigned(__builtin_ctz(mask))};}}
}
void Engine::dispatch_va_samples(){
 if(!va_helper_->begin(sample_count_,generation_))return;
 VaCandidate chosen[VaSampleSlot::capacity+3];unsigned count=0;
 for(unsigned n=va_candidate_count_;n && count<VaSampleSlot::capacity+3;){auto candidate=va_candidates_[--n];auto* v=candidate.voice;if(v->instrument==unsigned(Instrument::VirtualAnalog) && v->env>0 && (!v->off_at || sample_count_<v->off_at) && VaSampleSlot::eligible(v->va))chosen[count++]=candidate;}
 count=VaSampleSlot::selected_count(count);
 // Give the helper the earliest of the selected late voices first. Mixing and
 // all FX stay in the canonical order, including Layer gains / pan and Kit FX.
 while(count){const auto c=chosen[--count];auto* v=c.voice;uint64_t age=c.key<16?0:poly_voices_[c.key-16].age;unsigned job=va_helper_->add(v,v->va,v->frequency,!v->released,age,c.key);if(job<VaSampleSlot::capacity){va_job_[c.key]=uint8_t(job);va_job_sample_[c.key]=sample_count_;}}
 va_helper_->publish();
}
}
