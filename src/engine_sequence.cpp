#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
void Engine::reset_track(unsigned t,bool reset_position) {
 clear_kit_early(t);
 auto& r=runtime_[t]; const auto& state=track(t); r.retriggers=0;for(auto& repeat:kit_repeat_[t])repeat.remaining=0;kit_repeat_pads_[t]=0;kit_repeat_tracks_&=uint16_t(~(1u<<t)); for(auto& lock:r.active_locks)lock.enabled=false;
 if(reset_position) { r.position=state.direction==PlaybackDirection::Reverse?uint8_t(state.length-1):0; r.backward=false; r.cycle=0;r.follow_due=false; r.automation_live=false;r.lane_triggers=r.automation_tick=r.automation_cell=0;event_seed_tick_valid_[t]=false;r.arp_random=project_->random_seed^(0x9e3779b9u*(t+1));if(!r.arp_random)r.arp_random=1;r.last_step=r.position;r.grid=r.last_grid=sample_count_;r.last_period_q32=0; r.fraction=0; }
 else r.position%=state.length;
 refresh_parameters(t);update_timing(t);
}
uint64_t Engine::step_period(unsigned t) const {return runtime_[t].period_q32;}
// Keep an absolute integer and a fractional component so long sessions do not
// overflow an absolute Q32 sample clock. Origins may precede sample zero after
// slowing an already-running step.
void Engine::schedule_retrigger(unsigned t){auto& r=runtime_[t];uint64_t offset=(r.retrigger_period*r.retrigger_index)/r.retrigger_count;uint64_t fraction=uint64_t(r.retrigger_fraction)+uint32_t(offset);int64_t due=r.retrigger_origin+int64_t(offset>>32)+int64_t(fraction>>32);r.retrigger_at=due<0?0:uint64_t(due);}
void Engine::set_tempo(unsigned tempo){if(tempo<20 || tempo>300 || tempo==bpm_)return;unsigned old=bpm_;bpm_=uint16_t(tempo);project_->tempo=uint16_t(tempo);if(!external_clock_)tempo_q16_=uint32_t(tempo)<<16;for(unsigned i=0;i<80;++i){auto& pad=i<64?repeat_pads_[i]:repeat_midi_[i-64];if(pad.gate_due>sample_count_){uint64_t remaining=pad.gate_due-sample_count_;uint64_t scaled=(remaining*old+tempo/2)/tempo;pad.gate_due=sample_count_+(scaled?scaled:1);}}refresh_repeat_gate();}
// SPP counts six MIDI clocks (one sixteenth note). Do not reconstruct
// probability history or scene-follow decisions; seek the active clips only.
bool Engine::seek_song(unsigned position,bool send_midi){
 if(playing_ || position>16383 || (project_->song.enabled && song_has_snapshots(project_->song))){seek_armed_=false;event(EngineEventType::SongPosition,0,0,1,uint16_t(position));return false;}
 if(send_midi && !external_clock_ && !midi(0xf2,uint8_t(position&127),uint8_t(position>>7))){seek_armed_=false;event(EngineEventType::SongPosition,0,0,1,uint16_t(position));return false;}
 reset_clip_follow_seeds();stopped_at_=sample_count_;song_position_=uint16_t(position);song_phase_=0;lfo_transport_beat_=position/4;lfo_transport_fraction_=uint64_t(position%4)*uint64_t(rate_)*60/4;for(auto& r:runtime_)r.tempo_lfo.initialized=false;clock_phase_=0;bar_phase_=uint64_t(position%16)*uint64_t(rate_)*60;metro_ticks_=position*6;pending_scene_=-1;pending_stop_=0;for(auto& pending:pending_pattern_)pending=-1;chain_bars_=chain_position_=0;chain_armed_=true;song_armed_=true;song_finished_=false;song_section_=song_bars_=0;song_transpose_=0;song_mute_mask_=0;
 for(unsigned t=0;t<16;++t){note_off(t);reset_track(t,true);auto& r=runtime_[t];const auto& state=track(t);unsigned numerator=position*state.speed_num,den=state.speed_den;unsigned index=(numerator+den-1)/den;unsigned rem=index*den-numerator;unsigned len=state.length;
  if(state.direction==PlaybackDirection::Forward){r.position=uint8_t(index%len);r.cycle=index/len;r.backward=false;}
  else if(state.direction==PlaybackDirection::Reverse){r.position=uint8_t(len-1-index%len);r.cycle=index/len;r.backward=false;}
  else if(len==1){r.position=0;r.cycle=index;r.backward=false;}
  else{unsigned span=2*(len-1),phase=index%span;r.position=uint8_t(phase<len?phase:span-phase);r.cycle=index/span;r.backward=phase>=len-1 || (phase==0 && index!=0);}
  r.automation_tick=index;r.automation_cell=index;r.lane_triggers=0;uint64_t delay=(r.period_q32*rem)/den;r.grid=int64_t(sample_count_)+int64_t(delay>>32);r.fraction=uint32_t(delay);r.last_grid=r.grid;r.last_step=r.position;r.last_period_q32=0;r.due=step_due(t);
 }
 seek_armed_=true;event(EngineEventType::SongPosition,0,0,0,uint16_t(position));return true;
}
void Engine::update_timing(unsigned t){
 const auto& state=track(t);auto& r=runtime_[t];
 // Recompute the period only when its inputs change. The deadline still needs
 // refreshing: edits to swing, microtiming, groove or mutation can move it.
 if(r.timing_bpm==bpm_ && r.timing_tempo_q16==tempo_q16_ && r.timing_num==state.speed_num && r.timing_den==state.speed_den){r.due=step_due(t);retime_kit_early(t);return;}
 r.timing_bpm=uint16_t(bpm_);r.timing_tempo_q16=tempo_q16_;r.timing_num=state.speed_num;r.timing_den=state.speed_den;
 // Step period in Q32 samples from the Q16 tempo. The two-step division is
 // exact (floor) and identical to the historical integer formula whenever
 // the tempo fraction is zero, so master-mode timing is unchanged.
 uint64_t period_num=(uint64_t(rate_)*60*state.speed_den)<<32,period_den=uint64_t(tempo_q16_)*4*state.speed_num;
 uint64_t next=((period_num/period_den)<<16)+(((period_num%period_den)<<16)/period_den);uint64_t anchor=playing_?sample_count_:stopped_at_;
 if(playing_ && r.period_q32>1 && next!=r.period_q32){
  double ratio=double(next)/double(r.period_q32);
  if(r.retriggers){double elapsed=double(int64_t(anchor)-r.retrigger_origin)-double(r.retrigger_fraction)/4294967296.0;double offset=-elapsed*ratio;double whole=floor(offset);r.retrigger_origin=int64_t(anchor)+int64_t(whole);r.retrigger_fraction=uint32_t((offset-whole)*4294967296.0);r.retrigger_period=next;schedule_retrigger(t);}
  auto scale_off=[&](Voice& voice){if(voice.off_at>anchor){double remaining=double(voice.off_at-anchor)*ratio;uint64_t rounded=uint64_t(remaining+0.5);voice.off_at=anchor+(rounded?rounded:1);}};
  scale_off(voices_[t]);for(auto& voice:poly_voices_)if(voice.owner==t && voice.sequenced && !voice.pending)scale_off(voice.voice);
 }
 if(r.period_q32>1 && next!=r.period_q32 && r.due>anchor){double remaining=double(r.grid-int64_t(anchor))+double(r.fraction)/4294967296.0;double scaled=remaining*double(next)/double(r.period_q32);double integral=floor(scaled);r.grid=int64_t(anchor)+int64_t(integral);r.fraction=uint32_t((scaled-integral)*4294967296.0);}
 r.period_q32=next;
 r.due=step_due(t);retime_kit_early(t);
}
uint64_t Engine::step_due(unsigned t) const {
 const auto& r=runtime_[t]; const auto& pattern=project_->patterns[r.pattern]; const auto& state=track_views_.at(r.pattern,t);
 int64_t period=int64_t(step_period(t)>>32);
 // Microtiming units are 1/384 step; swing delays alternate steps by up to 1/4 step.
 int micro=state.mutation.amount?mutation_step(state,r.position).microtiming:state.steps[r.position].microtiming;
 int timing=bounded(micro+groove_timing(state.groove,r.position)*state.groove.amount/100,-192,192);
 int64_t offset=period*timing/384;
 if(r.position&1) offset+=period*pattern.swing/400;
 int64_t due=r.grid+offset;return due<0?0:uint64_t(due);
}
void Engine::clear_kit_early(unsigned t){kit_early_planned_[t]=kit_early_pads_[t]=kit_early_started_[t]=0;kit_early_tracks_&=uint16_t(~(1u<<t));}
// Before the first hit, keep lookahead aligned with tempo/swing edits. Once
// a ratchet has sounded, preserve its established spacing like normal hits.
void Engine::retime_kit_early(unsigned t){const auto& r=runtime_[t];const auto& state=track(t);uint64_t period=step_period(t)>>32;if(!period)period=1;for(uint32_t pads=kit_early_pads_[t]&~kit_early_started_[t];pads;pads&=pads-1){unsigned pad=unsigned(__builtin_ctz(pads));auto& early=kit_early_[t][pad];const auto& ks=state.kit.steps[pad][early.step];if(ks.microtiming>=0)continue;uint64_t lead=uint64_t(-int(ks.microtiming))*period/96;early.due=r.due>lead?r.due-lead:0;if(early.due<sample_count_)early.due=sample_count_;early.period=period/ks.ratchet;if(!early.period)early.period=1;}}
// Prepare only the next grid step. Two bounded banks keep its early hits and
// the previous step's ratchets independent until the next grid boundary.
void Engine::plan_kit_early_pad(unsigned t,unsigned pad){
 const uint16_t bit=uint16_t(1u<<pad);if(kit_early_started_[t]&bit)return;
 kit_early_planned_[t]&=uint16_t(~bit);kit_early_pads_[t]&=uint16_t(~bit);kit_early_[t][pad]=KitRepeat{};
 const auto& r=runtime_[t];const auto& state=track(t);const auto& ks=state.kit.steps[pad][r.position];
 if(r.playing && state.instrument==Instrument::DrumKit && ks.enabled && ks.microtiming<0){kit_early_planned_[t]|=bit;if(ks.velocity && ks.probability && step_probability_draw(project_->random_seed,t*16+pad,r.pattern,r.automation_tick)<ks.probability){
  uint64_t period=step_period(t)>>32;if(!period)period=1;auto& early=kit_early_[t][pad];early.step=uint8_t(r.position);early.velocity=ks.velocity;early.remaining=ks.ratchet;early.period=period/ks.ratchet;if(!early.period)early.period=1;uint64_t lead=uint64_t(-int(ks.microtiming))*period/96;early.due=r.due>lead?r.due-lead:0;if(early.due<sample_count_)early.due=sample_count_;kit_early_pads_[t]|=bit;
 }}if(kit_early_pads_[t])kit_early_tracks_|=uint16_t(1u<<t);else kit_early_tracks_&=uint16_t(~(1u<<t));
}
void Engine::plan_kit_early(unsigned t){
 clear_kit_early(t);const auto& r=runtime_[t];if(!r.playing || track(t).instrument!=Instrument::DrumKit)return;for(unsigned pad=0;pad<16;++pad)plan_kit_early_pad(t,pad);
}
void Engine::advance(unsigned t) {
 auto& r=runtime_[t]; const auto& state=track(t); uint64_t q=step_period(t);uint32_t previous_cycle=r.cycle;
 uint64_t fraction=uint64_t(r.fraction)+uint32_t(q); r.grid+=int64_t((q>>32)+(fraction>>32)); r.fraction=uint32_t(fraction);
 if(state.direction==PlaybackDirection::Forward) { if(++r.position>=state.length) { r.position=0; ++r.cycle; } }
 else if(state.direction==PlaybackDirection::Reverse) { if(r.position==0) { r.position=uint8_t(state.length-1); ++r.cycle; } else --r.position; }
 else if(state.length==1) { ++r.cycle; }
 else if(!r.backward) { if(r.position+1>=state.length) { r.backward=true; --r.position; } else ++r.position; }
 else if(r.position==0) { r.backward=false; ++r.position; } else {--r.position;if(!r.position)++r.cycle;}
 r.due=step_due(t);
 bool follow_owner=(scene_take_.active() || (!(project_->song.enabled && project_->song.length) && !(project_->chain.enabled && project_->chain.length))) && scene_take_.state()!=SceneTake::Ready;
 r.follow_due=r.cycle!=previous_cycle && follow_owner && state.clip_follow.action!=ClipFollowAction::Off && r.cycle%state.clip_follow.cycles==0;
 if(r.follow_due)clear_kit_early(t);else plan_kit_early(t);
}
}

namespace groove {
// --- External clock phase lock --------------------------------------------
// The internal tick position is tick_count_ plus the fraction of the current
// tick held in clock_phase_. Anchors (Start/Continue/SPP) align the master
// tick count with that position; each arriving 0xF8 then yields a phase
// error in Q32 ticks (positive = internal ahead of the master). A PI loop
// trims the applied Q16 tempo around the estimator's interval average, with
// the correction clamped to +/-3%. Errors beyond half a sixteenth (3 ticks)
// snap the transport phase back inside the current step and re-anchor.
// While no clock arrives the applied tempo simply holds: the transport
// free-runs. On the first returning tick the master is credited with the
// whole ticks elapsed during the gap (inferred from the gap length at the
// applied tempo), so the measured error stays fractional: a dropout then
// re-locks by PI pull-in with no jump, and only a genuine phase error
// beyond half a sixteenth takes the resync path.
int64_t Engine::slave_phase_error_q32() const {
 uint64_t period=uint64_t(rate_)*60;if(!period)return 0;
 uint64_t internal=(tick_count_<<32)+((clock_phase_<<32)/period);
 return int64_t(internal-slave_anchor_q32_)-int64_t(slave_master_ticks_<<32);
}
void Engine::slave_anchor(bool reset_loop){
 uint64_t period=uint64_t(rate_)*60;
 slave_anchor_q32_=(tick_count_<<32)+(period?(clock_phase_<<32)/period:0);
 slave_master_ticks_=0;slave_anchored_=true;
 if(reset_loop){slave_integrator_q32_=0;slave_phase_max_mt_=0;}
}
static int64_t slave_shift_phase(uint64_t& phase,uint64_t period,int64_t delta_units){
 if(!period)return 0;
 int64_t total=int64_t(phase)-delta_units,crossed=0;
 if(total<0){crossed=-((-total)+int64_t(period)-1)/int64_t(period);total+=-crossed*int64_t(period);}
 else if(total>=int64_t(period)){crossed=total/int64_t(period);total-=crossed*int64_t(period);}
 phase=uint64_t(total);return crossed;
}
void Engine::slave_resync(int64_t error_q32){
 const int64_t step_q32=int64_t(6)<<32,half_step=int64_t(3)<<32;
 int64_t e=error_q32%step_q32;if(e>half_step)e-=step_q32;if(e<-half_step)e+=step_q32;
 uint64_t clock_period=uint64_t(rate_)*60;if(!clock_period || !e){slave_anchor(false);return;}
 // Shift every transport phase back by e ticks and move the step grids
 // later by the same time, keeping the audible grid and the phase
 // accumulators aligned (both preserve their within-interval fraction).
 double tick_samples=double(rate_)*60.0/(24.0*(double(tempo_q16_)/65536.0));
 int64_t shift_samples=int64_t((double(e)/4294967296.0)*tick_samples+(e>0?0.5:-0.5));
 auto units=[&](uint64_t per_tick){return int64_t((e*int64_t(per_tick))>>32);};
 int64_t crossed_ticks=slave_shift_phase(clock_phase_,clock_period,units(clock_period));
 tick_count_=uint64_t(int64_t(tick_count_)+crossed_ticks);metro_ticks_=uint32_t(int64_t(metro_ticks_)+crossed_ticks);
 int64_t crossed_steps=slave_shift_phase(song_phase_,clock_period,units(clock_period/6));
 int64_t position=(int64_t(song_position_)+crossed_steps)%16384;if(position<0)position+=16384;song_position_=uint16_t(position);
 slave_shift_phase(bar_phase_,clock_period*16,units(clock_period/6));
 int64_t crossed_song=slave_shift_phase(arrangement_tick_phase_,clock_period,units(clock_period*(kSongTicksPerBeat/24)));
 arrangement_tick_=uint64_t(int64_t(arrangement_tick_)+crossed_song);
 int64_t crossed_beats=slave_shift_phase(lfo_transport_fraction_,clock_period,units(clock_period/24));
 if(crossed_beats<0 && uint64_t(-crossed_beats)>lfo_transport_beat_)lfo_transport_beat_=0;else lfo_transport_beat_=uint64_t(int64_t(lfo_transport_beat_)+crossed_beats);
 if(shift_samples)for(unsigned t=0;t<16;++t){auto& r=runtime_[t];r.grid+=shift_samples;if(r.retriggers && r.retrigger_at)r.retrigger_at=uint64_t(int64_t(r.retrigger_at)+shift_samples);r.due=step_due(t);}
 ++slave_resyncs_;slave_anchor(false);
}
void Engine::slave_clock_tick(){
 uint64_t now=sample_count_;
 uint64_t tick_gap_samples=slave_have_tick_?now-slave_last_tick_sample_:0;
 uint32_t base_q16=0;bool fresh=external_clock_estimator_.observe_q16(now,rate_,base_q16);
 // Until one real interval has been measured the applied tempo is the
 // project's saved tempo, not the master's: it must not be used to infer
 // lost ticks, and any error it caused is a start-up artefact to remove.
 bool had_base=slave_base_valid_;
 // The estimator treats any interval beyond a 20 BPM tick (rate/8 samples)
 // as a new stream; use the same bound to spot clock dropouts.
 bool gap=slave_have_tick_ && now-slave_last_tick_sample_>uint64_t(rate_)/8;
 slave_last_tick_sample_=now;slave_have_tick_=true;
 if(fresh){
  if(!slave_base_valid_ || !playing_){slave_base_q16_=base_q16;slave_base_valid_=true;}
  else{
   // Slew-limit the feedforward while playing: one stretched interval
   // (dropped ticks, a master hiccup) enters the estimator's average for
   // eight ticks and must not drag the applied tempo with it. Genuine
   // tempo movement is far slower than 0.5% per tick.
   int64_t delta=int64_t(base_q16)-int64_t(slave_base_q16_),max_step=int64_t(slave_base_q16_)/200;
   if(delta>max_step)delta=max_step;
   if(delta<-max_step)delta=-max_step;
   slave_base_q16_=uint32_t(int64_t(slave_base_q16_)+delta);
  }
 }
 if(gap)slave_freerun_=true;
 if(!playing_){
  // Transport parked: keep the anchor on the internal position and let the
  // applied/displayed tempo follow the master, as the follower always did.
  slave_anchor(false);slave_freerun_=false;
  if(slave_base_valid_)tempo_q16_=slave_base_q16_;
 } else if(!slave_anchored_){
  slave_anchor(false);
 } else {
  // A gap well beyond one tick period means ticks were lost in transit
  // (or the master paused): credit the master with the whole ticks that
  // elapsed at the applied tempo, so the phase error stays a true phase
  // (fractional) measurement and a dropout re-locks without a jump.
  if(had_base && slave_have_tick_ && tempo_q16_){
   uint64_t gap_samples=tick_gap_samples;
   uint64_t period_samples=(uint64_t(rate_)*60*65536)/(24*uint64_t(tempo_q16_));
   if(period_samples && gap_samples>period_samples+period_samples/2){
    uint64_t elapsed=(gap_samples+period_samples/2)/period_samples;
    if(elapsed>1)slave_master_ticks_+=elapsed-1;
   }
  }
  ++slave_master_ticks_;
  int64_t e=slave_phase_error_q32();
  slave_phase_mt_=int32_t((e*1000)>>32);
  int32_t magnitude=slave_phase_mt_<0?-slave_phase_mt_:slave_phase_mt_;
  if(magnitude>slave_phase_max_mt_)slave_phase_max_mt_=magnitude;
  const int64_t startup_slack=int64_t(1)<<30; // 0.25 tick
  if(!had_base && (e>startup_slack || e<-startup_slack)){slave_resync(e);if(slave_base_valid_)tempo_q16_=slave_base_q16_;}
  else if(e>(int64_t(3)<<32) || e<-(int64_t(3)<<32))slave_resync(e);
  else if(slave_base_valid_){
   // PI trim around the measured average; e>0 (ahead) slows the transport.
   slave_integrator_q32_-=e/200;
   const int64_t corr_clamp=(int64_t(1)<<32)*3/100;
   if(slave_integrator_q32_>corr_clamp)slave_integrator_q32_=corr_clamp;
   if(slave_integrator_q32_<-corr_clamp)slave_integrator_q32_=-corr_clamp;
   int64_t corr=-e/10+slave_integrator_q32_;
   if(corr>corr_clamp)corr=corr_clamp;
   if(corr<-corr_clamp)corr=-corr_clamp;
   uint64_t applied=(uint64_t(slave_base_q16_)*uint64_t((int64_t(1)<<32)+corr))>>32;
   tempo_q16_=applied<(20u<<16)?(20u<<16):applied>(300u<<16)?(300u<<16):uint32_t(applied);
   slave_locked_=magnitude<250;
  }
  slave_freerun_=false;
 }
 unsigned rounded=(tempo_q16_+(1u<<15))>>16;
 if(rounded!=bpm_){set_tempo(rounded);external_tempo_pending_=true;}
 if(tempo_q16_!=slave_last_applied_q16_){slave_last_applied_q16_=tempo_q16_;for(unsigned t=0;t<16;++t)update_timing(t);}
 if(external_tempo_pending_ && events_.free_space()){event(EngineEventType::TempoChanged,0,0,0,0,float(bpm_));external_tempo_pending_=false;}
}
Engine::SlaveClockState Engine::slave_clock_state() const {
 SlaveClockState state;state.phase_error_milliticks=slave_phase_mt_;state.max_phase_error_milliticks=slave_phase_max_mt_;state.master_ticks=slave_master_ticks_;state.resyncs=slave_resyncs_;state.applied_tempo_q16=tempo_q16_;state.locked=slave_locked_;state.freerun=slave_freerun_;state.anchored=slave_anchored_;return state;
}
}
