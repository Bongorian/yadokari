#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
PlaitsVoice* Engine::plaits_dsp(unsigned t,Voice& v){
 if(v.instrument!=unsigned(Instrument::PlaitsMacro) || !plaits_)return nullptr;
 if(&v==&voices_[t])return &plaits_[t];
 return poly_plaits_ && v.shared_slot<kSharedVoiceCapacity?&poly_plaits_[v.shared_slot]:nullptr;
}
Engine::PolyVoice* Engine::oldest_shared(VoiceResource resource,unsigned owner,const Voice* protect){
 PolyVoice* chosen=nullptr;
 for(auto& p:poly_voices_)if(p.owner<kTracks && (owner>=kTracks || p.owner==owner) && engine_voice_resource(p.voice.instrument)==resource && !p.pending && !p.voice.composite && &p.voice!=protect){
  if(!chosen || (p.voice.released && !chosen->voice.released) || (p.voice.released==chosen->voice.released && p.age<chosen->age))chosen=&p;
 }
 return chosen;
}
uint8_t Engine::reserve_shared(unsigned t,Instrument engine,unsigned model,bool poly,const Voice* protect){
 const auto* descriptor=engine_descriptor(unsigned(engine));if(!descriptor)return 255;
 const auto& policy=descriptor->polyphony;const auto resource=policy.resource;
 if(resource==VoiceResource::None || !policy.shared_slots || policy.shared_slots>kSharedVoiceCapacity)return 255;
 unsigned limit=instrument_voice_limit(engine,model),incoming=engine_voice_cost(engine,model);
 unsigned main=poly && engine_voice_resource(voices_[t].instrument)==resource && voices_[t].env>=0.00001f?1:0;
 if(!limit || limit<=main || incoming>policy.shared_budget)return 255;
 unsigned allowed=limit-1-main;
 unsigned count=0;for(const auto& p:poly_voices_)if(p.owner==t && engine_voice_resource(p.voice.instrument)==resource)++count;
 while(!layer_launching_ && count>allowed){auto* old=oldest_shared(resource,t,protect);if(!old)return 255;release_poly(*old,true);--count;}
 if(!poly)return 255;
 unsigned cost=0;for(const auto& p:poly_voices_)if(p.owner<kTracks && engine_voice_resource(p.voice.instrument)==resource)cost+=engine_voice_cost(Instrument(p.voice.instrument),p.voice.model);
 while(cost+incoming>policy.shared_budget){if(layer_launching_)return 255;auto* old=oldest_shared(resource,kTracks,protect);if(!old)return 255;cost-=engine_voice_cost(Instrument(old->voice.instrument),old->voice.model);release_poly(*old,true);}
 bool used[kSharedVoiceCapacity]{};for(const auto& p:poly_voices_)if(p.owner<kTracks && engine_voice_resource(p.voice.instrument)==resource && p.voice.shared_slot<policy.shared_slots)used[p.voice.shared_slot]=true;
 for(unsigned i=0;i<policy.shared_slots;++i)if(!used[i])return uint8_t(i);
 return 255;
}
bool Engine::note_off_main(unsigned t) {
 auto& v=voices_[t]; v.step_mono=v.tie_pending=false;v.step_pitch.reset();capture_voice_off(v);if(v.instrument==unsigned(Instrument::Sampler) && (v.looping || v.sample_mode==SampleMode::Gate))v.sampler.release(); v.daisy.note_off(); if(v.instrument==unsigned(Instrument::PolySaw) || v.instrument==unsigned(Instrument::PlaitsMacro) || v.instrument==unsigned(Instrument::FM) || v.instrument==unsigned(Instrument::Acid) || v.instrument>=unsigned(Instrument::WaveSequence))v.released=true;if(auto* dsp=plaits_dsp(t,v))dsp->release(); v.off_at=0;
 if(v.midi_note && !midi(uint8_t(0x80|v.channel),v.note,0)){pending_midi_off_|=uint16_t(1u<<t);return false;}
 v.midi_note=false;pending_midi_off_&=uint16_t(~(1u<<t));return true;
}
bool Engine::release_poly(PolyVoice& p,bool immediate){va_candidates_dirty_=true;auto& v=p.voice;if(p.owner>=16)return true;capture_voice_off(v);if(v.midi_note){if(!midi(uint8_t(0x80|v.channel),v.note,0)){p.pending=true;return false;}v.midi_note=false;p.pending=false;set_poly_owner(p,255);return true;}if(immediate){if(auto* dsp=plaits_dsp(p.owner,v))dsp->stop();v.sampler.stop();v.daisy.note_off();v.env=0;set_poly_owner(p,255);return true;}if(v.instrument==unsigned(Instrument::Sampler) && (v.looping || v.sample_mode==SampleMode::Gate))v.sampler.release();v.daisy.note_off();if(auto* dsp=plaits_dsp(p.owner,v))dsp->release();v.released=true;v.off_at=0;return true;}
bool Engine::note_off(unsigned t){kit_midi_release(255,t);bool ok=true;for(auto& p:poly_voices_)if(p.owner==t && !release_poly(p))ok=false;return note_off_main(t) && ok;}
bool Engine::release_sequence(unsigned t){bool ok=true;for(auto& p:poly_voices_)if(p.owner==t && p.sequenced && !p.voice.released && !release_poly(p))ok=false;return ok;}
void Engine::clear_poly(unsigned t){for(auto& p:poly_voices_)if(p.owner==t)release_poly(p,true);}
bool Engine::release_note(unsigned t,uint8_t note,uint16_t channel){bool ok=true;for(auto& p:poly_voices_)if(p.owner==t && !p.sequenced && p.voice.input_note==note && p.voice.received_channel==channel)if(!release_poly(p))ok=false;auto& v=voices_[t];bool poly_capable=live_poly_capable(Instrument(v.instrument)) || v.instrument==unsigned(Instrument::ExternalMidi);if(!poly_capable && v.input_note==note && v.received_channel==channel)if(!note_off_main(t))ok=false;return ok;}
void Engine::finish_repeat_pad(RepeatPad& pad){finish_recorded_note(pad.track,pad.recorded_note<128?pad.recorded_note:pad.note,pad.channel);pad.recorded_note=255;pad.gate_due=0;}
void Engine::refresh_repeat_gate(){repeat_next_gate_=0;for(unsigned i=0;i<16*kStepNotes+16;++i){const auto& pad=i<16*kStepNotes?repeat_pads_[i]:repeat_midi_[i-16*kStepNotes];if(pad.gate_due && (!repeat_next_gate_ || pad.gate_due<repeat_next_gate_))repeat_next_gate_=pad.gate_due;}}
// Released latch notes are owned by the repeat engine until replaced or stopped.
void Engine::clear_latched_notes(){for(unsigned i=0;i<16*kStepNotes+16;++i){auto& pad=i<16*kStepNotes?repeat_pads_[i]:repeat_midi_[i-16*kStepNotes];if(pad.held && !pad.down){release_note(pad.track,pad.note,pad.channel);finish_repeat_pad(pad);pad.held=false;}}refresh_repeat_gate();}
void Engine::prepare_repeat_input(){if(!repeat_mode_ || !repeat_latch_)return;for(unsigned i=0;i<16*kStepNotes+16;++i){const auto& pad=i<16*kStepNotes?repeat_pads_[i]:repeat_midi_[i-16*kStepNotes];if(pad.held && pad.down)return;}clear_latched_notes();repeat_index_=0;}
void Engine::replace_repeat_pad(unsigned hardware){if(hardware>=16)return;for(unsigned i=0;i<kStepNotes;++i){auto& old=repeat_pads_[hardware+i*16];if(old.held){release_note(old.track,old.note,old.channel);finish_repeat_pad(old);old=RepeatPad{};}}refresh_repeat_gate();}
void Engine::repeat_tick(){
 auto input=[&](unsigned i)->RepeatPad&{return i<16*kStepNotes?repeat_pads_[i]:repeat_midi_[i-16*kStepNotes];};
 if(repeat_next_gate_ && sample_count_>=repeat_next_gate_){for(unsigned i=0;i<16*kStepNotes+16;++i){auto& pad=input(i);if(pad.gate_due && sample_count_>=pad.gate_due){if(release_note(pad.track,pad.note,pad.channel))finish_repeat_pad(pad);else pad.gate_due=sample_count_+64;}}refresh_repeat_gate();}
 if(!repeat_mode_)return;
 static const uint8_t divisions[8]={1,2,4,8,16,3,6,12};
 repeat_phase_+=uint64_t(bpm_)*divisions[repeat_rate_];const uint64_t quarter=uint64_t(rate_)*60;if(repeat_phase_<quarter)return;repeat_phase_-=quarter;
 uint8_t order[16*kStepNotes+16];bool released[16*kStepNotes+16]{};unsigned count=0;
 for(unsigned i=0;i<16*kStepNotes+16;++i)if(input(i).held){auto& pad=input(i);released[i]=release_note(pad.track,pad.note,pad.channel);if(released[i])finish_repeat_pad(pad);order[count++]=uint8_t(i);}
 if(!count){refresh_repeat_gate();return;}
 unsigned octave=0;
 if(repeat_mode_==2){
  for(unsigned i=1;i<count;++i){uint8_t key=order[i];unsigned j=i;while(j && (input(order[j-1]).note>input(key).note || (input(order[j-1]).note==input(key).note && order[j-1]>key))){order[j]=order[j-1];--j;}order[j]=key;}
  struct Choice{uint8_t index,pitch;};Choice choices[(16*kStepNotes+16)*4];unsigned nchoices=0;for(unsigned o=0;o<project_->ui.arp_octaves;++o)for(unsigned i=0;i<count;++i){unsigned pitch=input(order[i]).note+12*o;if(pitch<=127)choices[nchoices++]={order[i],uint8_t(pitch)};}
  if(!nchoices){refresh_repeat_gate();return;}
  for(unsigned i=1;i<nchoices;++i){Choice key=choices[i];unsigned j=i;while(j && (choices[j-1].pitch>key.pitch || (choices[j-1].pitch==key.pitch && choices[j-1].index>key.index))){choices[j]=choices[j-1];--j;}choices[j]=key;}
  unsigned position=0;switch(project_->ui.arp_direction){case 1:position=nchoices-1-repeat_index_%nchoices;break;case 2:if(nchoices>1){unsigned phase=repeat_index_%(2*nchoices-2);position=phase<nchoices?phase:2*nchoices-2-phase;}break;case 3:position=next_random(repeat_random_)%nchoices;break;default:position=repeat_index_%nchoices;break;}
  unsigned selected=choices[position].index;octave=choices[position].pitch-input(selected).note;order[0]=uint8_t(selected);++repeat_index_;count=1;
 }
 uint64_t duration=quarter/(uint64_t(bpm_)*divisions[repeat_rate_]);uint64_t gate=duration*project_->ui.repeat_gate/100;if(!gate)gate=1;
 for(unsigned n=0;n<count;++n){unsigned i=order[n];if(!released[i])continue;auto& pad=input(i);if(audio_muted(pad.track) || (audio_has_solo() && !audio_solo_track(pad.track)))continue;const auto state=track_views_.at(pad.pattern,pad.track);unsigned note=pad.note+(repeat_mode_==2?octave:0);incoming_midi_=pad.native_drum || pad.kit_pad<16;Voice* v=trigger(pad.track,pad.velocity,int(note),nullptr,&state,0,false,true,pad.slice,pad.kit_pad<16?int(pad.kit_pad):-1,nullptr,-1,nullptr,nullptr,nullptr,pad.chromatic);incoming_midi_=false;if(pad.kit_pad<16)record_kit_hit(pad.track,pad.kit_pad,pad.velocity);if(v){for(auto* child=v;child;child=child->layer_peer){child->received_channel=pad.channel;child->input_note=pad.note;child->daisy.hold();}if(pad.kit_pad>=16)record_note(pad.track,note,pad.velocity,pad.channel,true,pad.chromatic,pad.native_drum);pad.recorded_note=uint8_t(note);pad.gate_due=sample_count_+gate;}}
 refresh_repeat_gate();
}

void Engine::release_midi_channel(unsigned t,uint16_t channel,bool immediate){for(auto& p:poly_voices_)if(p.owner==t && !p.sequenced && p.voice.received_channel==channel)release_poly(p,immediate);auto& v=voices_[t];if(v.received_channel==channel){note_off_main(t);if(immediate){v.sampler.stop();v.env=0;}}}
LayerReason Engine::layer_admission(unsigned t,const LayerState& layer,unsigned key,unsigned velocity,uint8_t& selected)const{
 auto reason=layer_status(layer);if(reason!=LayerReason::Ready)return reason;unsigned incoming=layer_note_cost(layer,key,velocity,&selected);if(!selected)return LayerReason::Ready;if(!leaf_runtime_)return LayerReason::Voices;
 unsigned free=0,total=0,local=0;constexpr unsigned resources=unsigned(VoiceResource::Count);unsigned shared[resources]{},shared_cost[resources]{},need_shared[resources]{},need_shared_cost[resources]{};
 for(const auto& p:poly_voices_){if(p.owner==255){++free;continue;}if(p.voice.composite){total+=p.voice.layer_cost;if(p.owner==t)local+=p.voice.layer_cost;}auto resource=engine_voice_resource(p.voice.instrument);if(resource!=VoiceResource::None){++shared[unsigned(resource)];shared_cost[unsigned(resource)]+=engine_voice_cost(Instrument(p.voice.instrument),p.voice.model);}}
 if(local+incoming>VoiceBudget::layer_track || total+incoming>VoiceBudget::layer_total)return LayerReason::Cost;
 if(free<unsigned(__builtin_popcount(unsigned(selected))))return LayerReason::Voices;
 for(unsigned i=0;i<layer.count;++i){if(!(selected&(1u<<i)))continue;const auto& child=layer.parts[i].sound;if(!engine_rate_supported(child.instrument,rate_))return LayerReason::Unsupported;if(engine_asset_mode(child.instrument)==EngineAssetMode::Sample && !samples_[child.sample_slot].frames)return LayerReason::Assets;if(child.instrument==Instrument::SoundFont && (!soundfont_resident(samples_[child.sample_slot].soundfont) || !soundfont_name(samples_[child.sample_slot].soundfont,unsigned(child.soundfont.values[0]),unsigned(child.soundfont.values[1]))))return LayerReason::Assets;auto resource=engine_voice_resource(unsigned(child.instrument));if(resource!=VoiceResource::None){unsigned r=unsigned(resource);++need_shared[r];need_shared_cost[r]+=engine_voice_cost(child.instrument,child.model);}}
 for(unsigned resource=1;resource<resources;++resource)if(need_shared[resource]){bool ready=resource==unsigned(VoiceResource::Plaits)?plaits_ && poly_plaits_:granular_!=nullptr;const auto& policy=engine_descriptor(unsigned(resource==unsigned(VoiceResource::Plaits)?Instrument::PlaitsMacro:Instrument::Granular))->polyphony;if(!ready || shared[resource]+need_shared[resource]>policy.shared_slots || shared_cost[resource]+need_shared_cost[resource]>policy.shared_budget)return LayerReason::Voices;}
 return LayerReason::Ready;
}
Engine::Voice* Engine::trigger(unsigned t,unsigned velocity,int note,const StepState* step,const TrackView* source,int accumulator,bool degrees,bool live_poly,int slice_override,int kit_pad,const KitStepState* kit_step,int kit_layer,Voice* protected_voice,const LayerPart* layer_part,const float* layer_mix,bool chromatic) {
 va_candidates_dirty_=true;
 RenderEventScope audit(render_audit_,event_work_,RenderAudit::TriggerWork);
 if(audio_lane_mask_&(1u<<t))return nullptr;
 if(t>=16) return nullptr;
 runtime_[t].last_velocity=velocity>127?1.0f:float(velocity)/127.0f;ModRuntime mod=mod_runtime(t);
 const int original_note=note;const unsigned original_velocity=velocity;
 const ConstTrackView base=source?*source:track(t);const bool step_sound=step && step->sound<kPresetSlots;const ConstTrackView state=layer_part?ConstTrackView(base,layer_part->sound):step_sound?ConstTrackView(base,project_->sounds[step->sound]):base;int kit_input_note=note;if(state.instrument==Instrument::DrumKit && kit_pad<0){if(note>=int(state.root_note) && note<int(state.root_note)+16)kit_pad=note-state.root_note;else if(note>=36 && note<52)kit_pad=note-36;else kit_pad=0;}
 const KitPadState* kit=kit_pad>=0 && kit_pad<16 && state.instrument==Instrument::DrumKit?&state.kit.pads[kit_pad]:nullptr;
 unsigned watch_lane=255;if(kit && kit_step && kit_layer<0 && __atomic_load_n(&kit_watch_active_,__ATOMIC_ACQUIRE)){for(unsigned i=0;i<2;++i){uint32_t spec=__atomic_load_n(&kit_watch_spec_[i],__ATOMIC_RELAXED);if((spec&15)==t && ((spec>>4)&15)==runtime_[t].pattern && ((spec>>8)&15)==unsigned(kit_pad) && kit_step==&state.kit.steps[kit_pad][(spec>>12)&255]){__atomic_add_fetch(&kit_watch_notes_[i],1,__ATOMIC_RELEASE);if(((spec>>20)&127)==original_velocity){watch_lane=i;__atomic_add_fetch(&kit_watch_velocity_[i],1,__ATOMIC_RELEASE);}}}}
 unsigned kit_slot=255;if(kit){if(kit->muted || (state.kit.solo_mask && !(state.kit.solo_mask & (1u<<kit_pad))))return nullptr;if(kit_layer<0)kit_midi_on(t,unsigned(kit_pad),velocity,kit_step,state);unsigned layer=kit_layer>=0?unsigned(kit_layer):kit->layer_mode==1 && velocity>=kit->velocity_split?1:kit->layer_mode==2?kit_round_robin_[t][kit_pad]++%2:0;kit_slot=kit->sample[layer];if(kit_slot==255 && kit_layer<0)kit_slot=kit->sample[0];if(kit_slot==255 && kit_layer<0 && kit->layer_mode==3)kit_slot=kit->sample[1];if(kit_layer<0 && kit_step && kit_step->sample_lock<kSampleAssets)kit_slot=kit_step->sample_lock;if(kit_slot>=kSampleAssets)return nullptr;}
 Instrument actual=kit?Instrument::Sampler:step && step->sound<kPresetSlots?project_->sounds[step->sound].instrument:state.instrument;bool suppressed_external=actual==Instrument::ExternalMidi && (audio_muted(t) || (audio_has_solo() && !audio_solo_track(t)));bool poly=actual!=Instrument::Texture && actual!=Instrument::Resonator && (live_poly || kit) && !suppressed_external && (live_poly_capable(actual) || layer_part || actual==Instrument::ExternalMidi);Voice* chosen=&voices_[t];
 if(actual==Instrument::Layer){
  uint8_t selected=0;LayerReason reason=layer_admission(t,state.layer,unsigned(note<0?state.root_note:note),velocity>127?127:velocity,selected);layer_failure_[t]=uint8_t(reason);if(reason!=LayerReason::Ready)return nullptr;
  Voice* first=nullptr;Voice* last=nullptr;StepState child_step;if(step){child_step=*step;child_step.sound=kNoPreset;}
  layer_launching_=true;
  for(unsigned i=0;i<state.layer.count;++i){if(!(selected&(1u<<i)))continue;float mix[3];const auto& part=state.layer.parts[i];const float bases[]={part.level,part.pan,part.detune};auto& rt=runtime_[t];for(unsigned j=0;j<3;++j)mix[j]=resolve_target_value(*project_,state,t,engine_target(Instrument::Layer,i*3+j),bases[j],step?step->locks:rt.active_locks,unsigned(rt.automation_cell),control_all_,control_all_id_,control_all_delta_,rt.automation_live,&gesture_overlay_,rt.pattern,nullptr,&mod).value;auto* child=trigger(t,velocity,note,step?&child_step:nullptr,source,accumulator,degrees,true,-1,-1,nullptr,-1,nullptr,&state.layer.parts[i],mix,chromatic);if(!child){for(auto& p:poly_voices_)for(auto* v=first;v;v=v->layer_peer)if(&p.voice==v)release_poly(p,true);layer_launching_=false;layer_failure_[t]=uint8_t(LayerReason::Voices);return nullptr;}child->sound_locked=true;if(last)last->layer_peer=child;else first=child;last=child;}
  layer_launching_=false;return first;
 }
 if(step && !live_poly && !kit && !layer_part && step->slide() && slide_voice(t,velocity,note,*step,state,accumulator,degrees))return &voices_[t];
 if(actual==Instrument::Granular)poly=true;
 if(actual==Instrument::Empty || actual==Instrument::Unknown)return nullptr;
 if(actual==Instrument::SoundFont && !soundfont_resident(samples_[state.sample_slot].soundfont)){layer_failure_[t]=uint8_t(LayerReason::Assets);return nullptr;}
 if(actual==Instrument::Granular && (!granular_ || !samples_[state.sample_slot].frames)){layer_failure_[t]=uint8_t(LayerReason::Assets);return nullptr;}
 if(actual==Instrument::Texture && !textures_)return nullptr;
 if(actual==Instrument::Resonator && !resonators_)return nullptr;
 if(actual==Instrument::Texture || actual==Instrument::Resonator){float values[15]{};resolve_voice_bank(t,actual,actual==Instrument::Texture?state.texture.values:state.resonator.values,values);if(!admit_native(t,actual,values))return nullptr;}
 if(actual==Instrument::ExternalMidi && panic_channels_)return nullptr;
 unsigned resolved_model=step && step->sound<kPresetSlots?project_->sounds[step->sound].model:state.plaits_model;
 int model_local=engine_model_parameter(state.instrument);if(!layer_part && actual==state.instrument && model_local>=0){const auto& r=runtime_[t];resolved_model=unsigned(resolve_target_value(*project_,state,t,engine_target(actual,unsigned(model_local)),float(resolved_model),step?step->locks:r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value);}
 uint8_t shared_slot=255;if(engine_voice_resource(unsigned(actual))!=VoiceResource::None){if(actual==Instrument::PlaitsMacro && (!plaits_ || !poly_plaits_))return nullptr;shared_slot=reserve_shared(t,actual,resolved_model,poly,protected_voice);if(poly && shared_slot==255)return nullptr;}
 if(poly){PolyVoice* slot=nullptr;for(auto& p:poly_voices_)if(p.owner==255){slot=&p;break;}if(!slot){for(auto& p:poly_voices_)if(!p.pending && !p.voice.composite && !layer_launching_ && &p.voice!=protected_voice && (!slot || (p.voice.released && !slot->voice.released) || (p.voice.released==slot->voice.released && p.age<slot->age)))slot=&p;}if(!slot){++poly_steals_.refused;return nullptr;}bool occupied=slot->owner<16,released=slot->voice.released;if(occupied && !release_poly(*slot,true)){++poly_steals_.refused;return nullptr;}++poly_steals_.triggered;if(occupied){if(released)++poly_steals_.released;else ++poly_steals_.held;}slot->voice.sampler.stop();slot->voice.daisy.note_off();set_poly_owner(*slot,t);slot->age=sample_count_;slot->sequenced=step!=nullptr || kit_step!=nullptr;slot->pending=false;chosen=&slot->voice;chosen->pitch_bend=voices_[t].pitch_bend;}else if(!note_off_main(t))return nullptr;
 auto& v=*chosen;v.kit_watch_lane=uint8_t(watch_lane);v.layer_peer=nullptr;v.composite=layer_part!=nullptr;v.layer_cost=layer_part?uint8_t(layer_part_cost(*layer_part)):0;v.layer_gain=layer_mix?layer_mix[0]:1;v.layer_pan=layer_mix?layer_mix[1]:0;v.layer_detune=layer_mix?layer_mix[2]/100:0;v.pitch_bend+=v.layer_detune;v.shared_slot=shared_slot;if(layer_part){v.leaf->fx=layer_part->fx;for(auto& insert:v.leaf->inserts)insert.reset();}
 if(velocity>127) velocity=127;
 v.instrument=uint8_t(actual);v.kit_pad=kit?uint8_t(kit_pad):255;if(kit){v.kit_insert.reset();v.kit_fx_locked=false;for(unsigned i=0;i<4;++i){auto& lock=v.kit_fx_locks[i];lock.enabled=kit_step && kit_step->fx_lock[i]!=255;lock.id=uint16_t(320+kit_pad*4+i);lock.value=lock.enabled?(i?float(kit_step->fx_lock[i])/127:float(kit_step->fx_lock[i])):0;v.kit_fx_locked|=lock.enabled;}auto& rt=runtime_[t];for(unsigned i=0;i<4;++i)v.kit_insert_target[i]=resolve_parameter(*project_,state,t,uint16_t(320+kit_pad*4+i),rt.active_locks,unsigned(rt.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,rt.automation_live,&gesture_overlay_,rt.pattern,nullptr,v.kit_fx_locks,&mod).value;for(unsigned i=0;i<3;++i)v.kit_insert_current[i]=v.kit_insert_target[i+1];}if(kit){const unsigned fields[]={0,2,3,4};const uint8_t values[]={uint8_t(kit_step?kit_step->level_lock:255),uint8_t(kit_step?kit_step->filter_lock:255),uint8_t(kit_step?kit_step->delay_lock:255),uint8_t(kit_step?kit_step->reverb_lock:255)};v.kit_mix_locked=false;for(unsigned i=0;i<4;++i){auto& lock=v.kit_mix_locks[i];lock.id=kit_mix_parameter_id(unsigned(kit_pad),fields[i]);lock.enabled=values[i]<128;lock.value=lock.enabled?float(values[i])/127:0;v.kit_mix_locked|=lock.enabled;}auto& rt=runtime_[t];for(unsigned i=0;i<5;++i)v.kit_mix_target[i]=resolve_parameter(*project_,state,t,kit_mix_parameter_id(unsigned(kit_pad),i),rt.active_locks,unsigned(rt.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,rt.automation_live,&gesture_overlay_,rt.pattern,nullptr,v.kit_mix_locks,&mod).value;v.kit_mix.reset(v.kit_mix_target[2],v.kit_mix_target[0],v.kit_mix_target[1],v.kit_mix_target[3],v.kit_mix_target[4]);} unsigned model=state.plaits_model; float params[5]; for(unsigned i=0;i<5;++i) params[i]=state.engine_parameters[i];
 if(!kit && step && step->sound<kPresetSlots) { const auto& sound=project_->sounds[step->sound]; v.instrument=uint8_t(sound.instrument); model=sound.model; for(unsigned i=0;i<5;++i) params[i]=sound.parameters[i]; }
 if(!kit){v.creative_state=(step && step->sound<kPresetSlots)?project_->sounds[step->sound].creative:state.creative;if(v.instrument>=unsigned(Instrument::WaveSequence) && v.instrument<=unsigned(Instrument::SpectralFreeze)){if(layer_part)v.leaf->creative.trigger_leaf(v.creative_state,v.instrument);else creative_[t].trigger(v.creative_state,v.instrument);}}
 v.sampler_source_epoch=sampler_source_epoch_[t];
 model=resolved_model;v.model=uint8_t(model);for(unsigned p=0;p<kPatterns;++p)if(&track_views_.at(p,t).steps==&state.steps){v.source_pattern=uint8_t(p);break;}
 v.texture_state=state.texture;v.texture_state.values[12]=base.texture.values[12];
 v.soundfont.stop();v.soundfont_base=step && step->sound<kPresetSlots?project_->sounds[step->sound].soundfont:state.soundfont;
 v.additive_base=state.additive;
 v.phase_dist_base=state.phase_dist;v.formant_base=state.formant;v.granular_base=state.granular;
 v.fm6_base=step && step->sound<kPresetSlots?project_->sounds[step->sound].fm6:state.fm6;
 v.va_base=step && step->sound<kPresetSlots?project_->sounds[step->sound].virtual_analog:state.virtual_analog;
 v.wavetable_base=step && step->sound<kPresetSlots?project_->sounds[step->sound].wavetable:state.wavetable;
 v.resonator_state=step && step->sound<kPresetSlots?project_->sounds[step->sound].resonator:state.resonator;
 v.sound_locked=layer_part || (step && step->sound<kPresetSlots);if(!kit)for(unsigned i=0;i<5;++i)v.base_parameters[i]=params[i];
 if(note<0) note=kit?project_->assets[kit_slot].metadata.root:state.root_note;
 if(kit_step)note+=kit_step->pitch_lock;
 if(note<0) note=0;
 // Resolve the complete sampler source once. A step sound supplies playback
 // flags and slice selection as well as SampleEdit; track flags must not leak
 // into a different sound selected by the phrase.
 const SoundState* sampler_sound=!kit && step && step->sound<kPresetSlots?&project_->sounds[step->sound]:nullptr;
 const bool source_loop=sampler_sound?sampler_sound->sample_loop:state.sample_loop;
 const bool source_reverse=sampler_sound?sampler_sound->sample_reverse:state.sample_reverse;
 const unsigned source_slice=sampler_sound?sampler_sound->sample_slice:state.sample_slice;
 const unsigned source_slices=sampler_sound?sampler_sound->sample_slices:state.sample_slices;
 v.looping=source_loop;v.received_channel=65535;v.input_note=uint8_t(kit && kit_input_note>=0?kit_input_note:note);note=twin_isolated_ && t==freeze_track_?bounded(note,0,127):(chromatic || (step && step->chromatic))?bounded(note+int(state.transpose),0,127):state.instrument==Instrument::SoundFont && state.soundfont.values[0]==128?bounded(note,0,127):transform_note(note,state);
 note=degrees?scale_move(note,accumulator,state):bounded(note+accumulator,0,127);
 const bool twin_capture=twin_isolated_ && t==freeze_track_;
 if(!twin_capture){velocity=unsigned(float(velocity)*state.velocity_scale);}
 if(velocity>127)velocity=127;
 if(twin_capture){v.pitch_bend=v.layer_detune;}
 const float tuning=twin_capture?0:project_->tuning[note%12]/100;
 v.note=uint8_t(note);v.channel=state.midi_channel;
 if(!kit){v.frequency=440.0f*powf(2.0f,(float(note)+tuning+v.pitch_bend-69.0f)/12.0f);
  float hz=v.frequency<float(rate_)*0.45f?v.frequency:float(rate_)*0.45f;v.saw.set_frequency(hz);v.daisy.set_note_float(float(v.note)+tuning+v.pitch_bend);
  for(unsigned i=0;i<5;++i)v.parameters[i]=params[i];
  for(unsigned i=0;i<5;++i){v.parameters[i]=(i<macro_controls(v.instrument)?resolve_target_value(*project_,state,t,engine_target(Instrument(v.instrument),i),v.base_parameters[i],runtime_[t].active_locks,unsigned(runtime_[t].automation_cell),control_all_,control_all_id_,control_all_delta_,runtime_[t].automation_live,&gesture_overlay_,runtime_[t].pattern).value:v.base_parameters[i]);}const auto* descriptor=engine_descriptor(v.instrument);unsigned declared=descriptor?descriptor->instrument.macro_controls:0;if(declared)descriptor->bind(descriptor->parameters,v.parameters,declared,{v.targets});for(unsigned i=declared;i<5;++i)v.targets[i]=v.parameters[i];v.saw.reset();
 }
 if(actual==Instrument::SoundFont){auto sf=v.soundfont_base;resolve_voice_bank(t,actual,v.soundfont_base.values,sf.values);if(!v.soundfont.trigger(samples_[state.sample_slot].soundfont,v.frequency,velocity,sf,!(incoming_midi_ || (step && step->native_drum)),v.note)){v.env=0;layer_failure_[t]=uint8_t(LayerReason::Assets);if(poly)for(auto& p:poly_voices_)if(&p.voice==&v)set_poly_owner(p,255);return nullptr;}layer_failure_[t]=0;}
 if(actual==Instrument::Additive){float values[25];resolve_voice_bank(t,actual,v.additive_base.values,values);v.additive.trigger(float(velocity)/127,values);}
 if(actual==Instrument::PhaseDist){float values[9];resolve_voice_bank(t,actual,v.phase_dist_base.values,values);v.phase_dist.trigger(float(velocity)/127,values);}
 if(actual==Instrument::Granular){float values[kGranularParameterCount];resolve_voice_bank(t,actual,v.granular_base.values,values);if(v.shared_slot>=4 || !granular_[v.shared_slot].trigger(samples_[state.sample_slot],project_->assets[state.sample_slot].metadata.root,float(velocity)/127,values,project_->random_seed^uint32_t(sample_count_)^t)){v.env=0;for(auto& p:poly_voices_)if(&p.voice==&v)set_poly_owner(p,255);return nullptr;}}
 if(actual==Instrument::Formant){float values[9];resolve_voice_bank(t,actual,v.formant_base.values,values);v.formant.trigger(float(velocity)/127,values);}
 if(actual==Instrument::Fm6){auto state=v.fm6_base;resolve_voice_bank(t,actual,v.fm6_base.values,state.values);v.fm6.trigger(v.frequency,velocity,state);}
 if(actual==Instrument::VirtualAnalog){auto clock=dsp_profile_.control_clock();unsigned start=clock?clock():0;float values[kVirtualAnalogParameterCount];resolve_voice_bank(t,actual,v.va_base.values,values);v.va.trigger(float(velocity)/127,values,project_->random_seed^uint32_t(sample_count_)^t);if(clock)dsp_profile_.va_trigger(clock()-start);}
 if(actual==Instrument::Wavetable){float values[8];auto& r=runtime_[t];for(unsigned i=0;i<8;++i)values[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::Wavetable,i),v.wavetable_base.values[i],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;v.wavetable.trigger(v.frequency,float(velocity)/127,values);}
 if(actual==Instrument::Texture){textures_suspended_=false;textures_[t].trigger();}
 if(actual==Instrument::Resonator && !resonators_[t].trigger(float(v.note)+tuning)){for(auto& p:poly_voices_)if(&p.voice==&v)set_poly_owner(p,255);return nullptr;}
 v.step_pitch.reset();v.tie_pending=false;v.step_accent=step && step->accent();v.step_sound=step?step->sound:kNoPreset;v.step_mono=step && !poly && !kit && !layer_part && !state.arp_mode && step_monophonic(*step);
 v.a=float(velocity)/127.0f;v.released=false;v.env=velocity?1:0;
 if(auto* dsp=plaits_dsp(t,v)){dsp->set_model(model);dsp->set_note_float(float(v.note)+tuning+v.pitch_bend);for(unsigned i=0;i<5;++i)dsp->set_parameter(i,v.parameters[i]);dsp->trigger(uint8_t(velocity),live_poly || step!=nullptr);}
 if(actual==Instrument::DaisySynth){float adsr[4];for(unsigned i=0;i<4;++i)adsr[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::DaisySynth,i),engine_parameter_value(state,i),runtime_[t].active_locks,unsigned(runtime_[t].automation_cell),control_all_,control_all_id_,control_all_delta_,runtime_[t].automation_live,&gesture_overlay_,runtime_[t].pattern).value;v.daisy.set_adsr(adsr[0],adsr[1],adsr[2],adsr[3]);v.daisy.trigger(uint8_t(velocity));}
 if(v.instrument==unsigned(Instrument::FM))v.fm.Reset();
 if(v.instrument==unsigned(Instrument::DaisyAnalogDrums) || v.instrument==unsigned(Instrument::DaisySyntheticDrums))public_drums_[t].trigger(v.instrument-unsigned(Instrument::DaisyAnalogDrums),v.parameters,float(velocity)/127.0f,v.frequency);
 unsigned base_sample_slot=kit?kit_slot:(step && step->sound<kPresetSlots)?project_->sounds[step->sound].sample_slot:state.sample_slot;v.sample_slot=uint8_t(base_sample_slot);if(v.instrument==unsigned(Instrument::Sampler) && step && !kit)for(const auto& lock:step->locks)if(lock.enabled && lock.id==300 && valid_step_sample_lock(lock.id,lock.value))v.sample_slot=uint8_t(lock.value);v.sample=samples_[v.sample_slot];v.sample_root=project_->assets[v.sample_slot].metadata.root;
 if(v.instrument==unsigned(Instrument::Sampler)){SampleEdit edit=kit?kit->edit:step && step->sound<kPresetSlots?project_->sounds[step->sound].sample_edit:state.sample_edit;if(!kit){const auto& descriptor=*engine_descriptor(unsigned(Instrument::Sampler));float values[7];for(unsigned i=0;i<descriptor.parameters.count;++i)values[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::Sampler,i),edit.*sample_parameter_member(descriptor.parameters.declarations[i].offset),runtime_[t].active_locks,unsigned(runtime_[t].automation_cell),control_all_,control_all_id_,control_all_delta_,runtime_[t].automation_live,&gesture_overlay_,runtime_[t].pattern).value;descriptor.bind(descriptor.parameters,values,7,{nullptr,nullptr,&edit});}
  if(edit.end<=edit.start)edit.end=edit.start+0.00001f;
  if(edit.end>1){edit.end=1;edit.start=0.9999f;}
  unsigned slice=slice_override>=0?unsigned(slice_override):source_slice;bool reverse=kit?kit->reverse:source_reverse,locked_slice=false;if(kit_step && kit_step->reverse_lock<2)reverse=kit_step->reverse_lock!=0;if(step)for(const auto& lock:step->locks)if(lock.enabled && valid_step_sample_lock(lock.id,lock.value)){if(lock.id==301){slice=unsigned(lock.value);locked_slice=true;}else if(lock.id==302)reverse=lock.value>0;}if(edit.zone_count){if(v.sample_slot!=base_sample_slot){edit.zone_count=0;edit.slice_count=0;}else{if(slice_override<0 && !locked_slice)slice=select_sample_zone(edit,unsigned(note),velocity,sample_zone_round_robin_[t]++);slice%=edit.zone_count;v.sample_root=edit.zones[slice].root;}}if(edit.slice_count){slice%=edit.slice_count;float length=edit.end-edit.start,origin=edit.start;edit.start=origin+length*edit.markers[slice];edit.end=origin+length*edit.markers[slice+1];}else if(!kit && source_slices>1){slice%=source_slices;float length=edit.end-edit.start;float origin=edit.start;edit.start=origin+length*slice/source_slices;edit.end=origin+length*(slice+1)/source_slices;}
  if(kit_step && kit_step->start_lock<255){float length=edit.end-edit.start;edit.start+=length*float(kit_step->start_lock)/255.0f;}
  v.base_sample_edit=kit?kit->edit:sampler_sound?sampler_sound->sample_edit:state.sample_edit;v.sample_speed=edit.speed;v.sample_mode=!kit && source_loop?SampleMode::Loop:edit.mode;v.looping=v.sample_mode==SampleMode::Loop || v.sample_mode==SampleMode::PingPong || v.sample_mode==SampleMode::Stretch;v.sample_origin=0;v.sample_extent=1;v.choke_group=edit.choke;if(edit.choke && kit_layer<0 && !layer_part){for(auto& other:voices_)if(&other!=&v && other.instrument==unsigned(Instrument::Sampler) && other.choke_group==edit.choke)other.sampler.release();for(auto& p:poly_voices_)if(p.owner<16 && &p.voice!=&v && p.voice.instrument==unsigned(Instrument::Sampler) && p.voice.choke_group==edit.choke){p.voice.sampler.release();p.voice.released=true;}}
  float warp_factor=1;if((edit.slice_count || (!kit && source_slices>1)) && slice<64 && edit.slice_warp[slice] && base_sample_slot<kSampleAssets){unsigned asset_bpm=project_->assets[base_sample_slot].metadata.bpm;if(asset_bpm>=40 && asset_bpm<=300){warp_factor=float(bpm_)/float(asset_bpm);if(warp_factor<0.5f)warp_factor=0.5f;if(warp_factor>2.0f)warp_factor=2.0f;}}
  float stretch_ratio=1;if(edit.mode==SampleMode::Stretch && base_sample_slot<kSampleAssets){unsigned asset_bpm=project_->assets[base_sample_slot].metadata.bpm;if(asset_bpm>=40 && asset_bpm<=300){stretch_ratio=float(asset_bpm)/float(bpm_);if(stretch_ratio<0.5f)stretch_ratio=0.5f;if(stretch_ratio>2.0f)stretch_ratio=2.0f;}}float pitch=edit.mode==SampleMode::Stretch?1.0f:warp_factor*powf(2,(float(note)+tuning+v.pitch_bend-v.sample_root)/12);if(pitch<0.015625f)pitch=0.015625f;if(pitch>64)pitch=64;v.sampler.start_edit(v.sample,rate_,pitch,!kit && source_loop,reverse,edit,stretch_ratio);if(kit){v.kit_speed_bend=v.pitch_bend;v.kit_speed_tuning=project_->tuning[v.note%12];v.kit_speed_value=edit.speed;}
  if(watch_lane<2 && v.sampler.active())__atomic_add_fetch(&kit_watch_hits_[watch_lane],1,__ATOMIC_RELEASE);
 }

 // Step presets initialize the external instrument before Note On. The
 // borrowed view keeps program/CC resolution out of persistent track bases.
 if(actual==Instrument::ExternalMidi && velocity && !suppressed_external){
  auto& r=runtime_[t];if(step_sound){
   unsigned count=state.midi_program<128?1:0;for(unsigned cc=0;cc<128;++cc)if(state.midi_cc_used[cc/8]&(1u<<(cc%8)))++count;
   if(midi_events_.free_space()<count+65 || (midi_capture_active_ && !midi_capture_internal_ && midi_capture_events_->free_space()<count+2)){if(poly)for(auto& p:poly_voices_)if(&p.voice==&v)set_poly_owner(p,255);v.env=0;return nullptr;}
   if(r.cc_channel!=state.midi_channel){for(auto& sent:r.cc_sent)sent=255;r.cc_channel=state.midi_channel;}
   if(r.program_channel!=state.midi_channel){r.program_sent=255;r.program_channel=state.midi_channel;}
   auto send=[&](unsigned cc){if(!(state.midi_cc_used[cc/8]&(1u<<(cc%8))))return;auto resolved=resolve_parameter(*project_,state,t,uint16_t(128+cc),r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,nullptr,r.automation_live,&gesture_overlay_,r.pattern);uint8_t value=uint8_t(resolved.value);if(r.cc_sent[cc]!=value && midi(uint8_t(0xb0|state.midi_channel),uint8_t(cc),value)){r.cc_sent[cc]=value;if(cc==0 || cc==32)r.program_sent=255;}};
   send(0);send(32);if(state.midi_program<128 && r.program_sent!=state.midi_program && midi(uint8_t(0xc0|state.midi_channel),state.midi_program,0,2))r.program_sent=state.midi_program;
   for(unsigned cc=0;cc<128;++cc)if(cc!=0 && cc!=32)send(cc);
  }else{r.cc_dirty=true;refresh_cc(t);}
 }
 if(v.instrument==unsigned(Instrument::ExternalMidi) && velocity) { bool audible=!suppressed_external;v.midi_note=audible && midi(uint8_t(0x90|v.channel),v.note,uint8_t(velocity));if(poly && audible && !v.midi_note){for(auto& p:poly_voices_)if(&p.voice==&v)set_poly_owner(p,255);return nullptr;} }
 capture_voice_on(v,t,velocity);
 if(step){uint64_t length=((runtime_[t].retrigger_period*step->gate)/(96*runtime_[t].retrigger_count))>>32;v.off_at=sample_count_+(length?length:1);}
 else if(kit_step && kit && (v.sample_mode==SampleMode::Gate || v.looping)){uint64_t length=(step_period(t)>>32)/kit_step->ratchet;v.off_at=sample_count_+(length?length:1);}
 else v.off_at=0;
 event(EngineEventType::Note,uint8_t(t),v.note);if(kit && kit_layer<0 && kit->layer_mode==3 && kit->sample[1]<kSampleAssets && (kit->sample[0]<kSampleAssets || (kit_step && kit_step->sample_lock<kSampleAssets)))v.layer_peer=trigger(t,original_velocity,original_note,step,source,accumulator,degrees,live_poly,slice_override,kit_pad,kit_step,1,&v);return &v;
}
bool Engine::slide_voice(unsigned t,unsigned velocity,int note,const StepState& step,ConstTrackView state,int accumulator,bool degrees){
 auto& v=voices_[t];const auto& r=runtime_[t];
 if(!velocity || !v.step_mono || v.released || v.a<=0 || state.arp_mode || !step_monophonic(step) || v.step_sound!=step.sound || v.instrument!=unsigned(state.instrument) || !step_legato_engine(state.instrument) || v.source_pattern!=r.pattern)return false;
 if(note<0)note=state.root_note;
 note=step.chromatic?bounded(note+int(state.transpose),0,127):transform_note(note,state);
 note=degrees?scale_move(note,accumulator,state):bounded(note+accumulator,0,127);
 float target=440.0f*powf(2.0f,(float(note)+project_->tuning[note%12]/100+v.pitch_bend-69)/12);
 capture_voice_off(v);v.step_pitch.start(v.frequency,target,rate_);v.note=uint8_t(note);v.input_note=uint8_t(note);v.step_accent=step.accent();v.tie_pending=false;
 unsigned scaled=unsigned(float(velocity)*state.velocity_scale);v.a=float(scaled>127?127:scaled)/127;
 uint64_t length=((r.retrigger_period*step.gate)/(96*r.retrigger_count))>>32;v.off_at=sample_count_+(length?length:1);
 capture_voice_on(v,t,scaled>127?127:scaled);event(EngineEventType::Note,uint8_t(t),v.note);return true;
}
bool Engine::hold_for_slide(unsigned t,Voice& v){
 auto& r=runtime_[t];const auto state=track(t);
 if(!playing_ || !r.playing || r.follow_due || v.tie_pending || !v.step_mono || v.released || !step_legato_engine(Instrument(v.instrument)) || state.arp_mode || audio_muted(t) || (audio_has_solo() && !audio_solo_track(t)) || v.source_pattern!=r.pattern || sample_count_>=r.due)return false;
 const auto next=mutation_step(state,r.position);Instrument actual=next.sound<kPresetSlots?project_->sounds[next.sound].instrument:state.instrument;
 if(!next.slide() || !next.enabled || !next.probability || next.sound!=v.step_sound || unsigned(actual)!=v.instrument || !step_monophonic(next) || !step_condition_passes(next,r.cycle,fill_) || step_probability_draw(project_->random_seed,t,r.pattern,r.automation_tick)>=next.probability)return false;
 const auto& a=state.accumulator;uint32_t cycle=a.reset?r.cycle%a.reset:r.cycle;
 int velocity=state.lanes_enabled?state.lanes[1].values[lane_cell(state.lanes[1],r.lane_triggers)]:next.velocity;
 if(int64_t(velocity)+int64_t(cycle)*a.velocity+groove_velocity(state.groove,r.position)*state.groove.amount/100<=0)return false;
 v.tie_pending=true;v.off_at=r.due+1;return true;
}
void Engine::trigger_step_chord(unsigned t,unsigned velocity,int note,const StepState& step,int accumulator,bool degrees){if(!release_sequence(t))return;if(!trigger(t,velocity,note,&step,nullptr,accumulator,degrees))return;Instrument actual=step.sound<kPresetSlots?project_->sounds[step.sound].instrument:track(t).instrument;if(!live_poly_capable(actual) && actual!=Instrument::ExternalMidi)return;const auto& state=track(t);auto effective=[&](int input){int value=(step.chromatic?bounded(input+state.transpose,0,127):transform_note(input,state));return degrees?scale_move(value,accumulator,state):bounded(value+accumulator,0,127);};int heard[kStepNotes]={effective(note)};unsigned count=1;for(uint8_t chord_note:step.chord)if(chord_note<128 && count<instrument_voice_limit(actual,step.sound<kPresetSlots?project_->sounds[step.sound].model:state.plaits_model)){int next=bounded(int(state.root_note)+int(chord_note)-60,0,127),pitch=effective(next);bool duplicate=false;for(unsigned i=0;i<count;++i)duplicate|=heard[i]==pitch;if(!duplicate){heard[count++]=pitch;trigger(t,velocity,next,&step,nullptr,accumulator,degrees,true);}}}
// Transform and deduplicate before ordering, so scale quantization cannot generate duplicate pitches.
void Engine::trigger_step_arp(unsigned t,unsigned velocity,int note,const StepState& step,int accumulator,bool degrees,unsigned index){
 const auto& state=track(t);struct Choice{uint8_t input,pitch;};Choice choices[kStepNotes*4];unsigned count=0;
 for(unsigned oct=0;oct<state.arp_octaves;++oct)for(unsigned n=0;n<kStepNotes;++n){int source=n?step.chord[n-1]:step.note;if(source>=128)continue;int input=bounded(int(state.root_note)+source-60,0,127)+int(oct)*12;if(input>127)continue;int pitch=(step.chromatic?bounded(input+state.transpose,0,127):transform_note(input,state));pitch=degrees?scale_move(pitch,accumulator,state):bounded(pitch+accumulator,0,127);bool duplicate=false;for(unsigned i=0;i<count;++i)duplicate|=choices[i].pitch==pitch;if(!duplicate)choices[count++]={uint8_t(input),uint8_t(pitch)};}
 if(!count){return;}for(unsigned i=1;i<count;++i){Choice key=choices[i];unsigned j=i;while(j && choices[j-1].pitch>key.pitch){choices[j]=choices[j-1];--j;}choices[j]=key;}
 unsigned selected=0;switch(state.arp_mode){case 2:selected=count-1-index%count;break;case 3:if(count>1){unsigned phase=index%(2*count-2);selected=phase<count?phase:2*count-2-phase;}break;case 4:selected=next_random(runtime_[t].arp_random)%count;break;default:selected=index%count;break;}
 if(!release_sequence(t)){return;}trigger(t,velocity,choices[selected].input,&step,nullptr,accumulator,degrees);
}
}
