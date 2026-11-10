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
 if(reset_position) { r.position=state.direction==PlaybackDirection::Reverse?uint8_t(state.length-1):0; r.backward=false; r.cycle=0;r.follow_due=false; r.automation_live=false;r.lane_triggers=r.automation_tick=r.automation_cell=0;r.arp_random=project_->random_seed^(0x9e3779b9u*(t+1));if(!r.arp_random)r.arp_random=1;r.last_step=r.position;r.grid=r.last_grid=sample_count_;r.last_period_q32=0; r.fraction=0; }
 else r.position%=state.length;
 refresh_parameters(t);update_timing(t);
}
uint64_t Engine::step_period(unsigned t) const {return runtime_[t].period_q32;}
// Keep an absolute integer and a fractional component so long sessions do not
// overflow an absolute Q32 sample clock. Origins may precede sample zero after
// slowing an already-running step.
void Engine::schedule_retrigger(unsigned t){auto& r=runtime_[t];uint64_t offset=(r.retrigger_period*r.retrigger_index)/r.retrigger_count;uint64_t fraction=uint64_t(r.retrigger_fraction)+uint32_t(offset);int64_t due=r.retrigger_origin+int64_t(offset>>32)+int64_t(fraction>>32);r.retrigger_at=due<0?0:uint64_t(due);}
void Engine::set_tempo(unsigned tempo){if(tempo<20 || tempo>300 || tempo==bpm_)return;unsigned old=bpm_;bpm_=uint16_t(tempo);project_->tempo=uint16_t(tempo);for(unsigned i=0;i<80;++i){auto& pad=i<64?repeat_pads_[i]:repeat_midi_[i-64];if(pad.gate_due>sample_count_){uint64_t remaining=pad.gate_due-sample_count_;uint64_t scaled=(remaining*old+tempo/2)/tempo;pad.gate_due=sample_count_+(scaled?scaled:1);}}refresh_repeat_gate();}
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
 if(r.timing_bpm==bpm_ && r.timing_num==state.speed_num && r.timing_den==state.speed_den){r.due=step_due(t);retime_kit_early(t);return;}
 r.timing_bpm=uint16_t(bpm_);r.timing_num=state.speed_num;r.timing_den=state.speed_den;
 uint64_t next=((uint64_t(rate_)*60*state.speed_den)<<32)/(uint64_t(bpm_)*4*state.speed_num);uint64_t anchor=playing_?sample_count_:stopped_at_;
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
