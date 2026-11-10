#include "drum_gm.h"
#include "engine.h"
#include "alloc_policy.h"
#include "pcm_output.h"
#include "note_transform.h"
#include "instrument_math.h"
#include <math.h>
namespace groove {
static float sine_table(float phase){static float table[1025];static bool built=false;if(!built){for(unsigned i=0;i<=1024;++i)table[i]=sinf(6.283185307f*float(i)/1024);built=true;}float x=phase*(1024/6.283185307f);if(x<0)x=0;if(x>=1024)x=1023.9999f;unsigned i=unsigned(x);float frac=x-float(i);return table[i]+(table[i+1]-table[i])*frac;}
bool Engine::plain_visual_parameters(unsigned t){
 if(project_->morph_enabled || gesture_overlay_.morph_active || gesture_overlay_.count || control_all_)return false;
 for(unsigned i=0;i<8;++i)if(project_->macros[i].active || gesture_overlay_.macro_active[i])return false;
 const auto& r=runtime_[t];for(const auto& lock:r.active_locks)if(lock.enabled)return false;
 if(r.automation_live)for(const auto& lane:track(t).automation)if(lane.enabled)return false;
 return true;
}
void Engine::resolve_voice_bank(unsigned t,Instrument engine,const float* bases,float* values){
 auto& r=runtime_[t];auto state=track(t);if(resolve_unlayered_engine(*project_,state,t,engine,r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,bases,values))return;
 ModRuntime mod=mod_runtime(t);const auto& table=engine_descriptor(unsigned(engine))->parameters;
 for(unsigned i=0;i<table.count;++i){unsigned offset=table.declarations[i].offset;values[offset]=resolve_target_value(*project_,state,t,engine_target(engine,i),bases[offset],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;}
}
bool Engine::admit_native(unsigned t,Instrument engine,const float* values){
 if(!native_budget_.admit(t,engine_native_cost(engine,values))){native_retry_sample_[t]=sample_count_+48;layer_failure_[t]=uint8_t(LayerReason::Cost);__atomic_fetch_or(&native_refused_,1u<<t,__ATOMIC_RELEASE);return false;}
 for(unsigned i=0;i<(engine==Instrument::Texture?15u:8u);++i)native_values_[t][i]=values[i];
 layer_failure_[t]=0;__atomic_fetch_and(&native_refused_,~(1u<<t),__ATOMIC_RELEASE);__atomic_fetch_or(&native_active_,1u<<t,__ATOMIC_RELEASE);return true;
}
void Engine::process_profiled_insert(TrackInsertSlot& effect,float& l,float& r,unsigned type,float a,float b,float wet,float ext_l,float ext_r,unsigned path){DspTimingScope timer(dsp_profile_,DspProfile::fx(path,type));effect.process(l,r,type,a,b,wet,ext_l,ext_r);}
// Keep the resident/paged Sampler path out of the 1120-byte general synth
// frame. Both paths preserve the same control prelude and profiling boundary.
inline __attribute__((always_inline)) void Engine::advance_legacy_voice(Voice& v){
 if(v.step_pitch.remaining){v.frequency=v.step_pitch.next(v.frequency);float hz=fminf(v.frequency,float(rate_)*.45f);v.saw.set_frequency(hz);}
 if(v.kit_pad>=16)for(unsigned p=0;p<5;++p)v.parameters[p]+=(v.targets[p]-v.parameters[p])*0.005f;
}
__attribute__((noinline)) void Engine::render_sampler_voice(Voice& v,float& l,float& rr){
 // On unsampled blocks neither the bin nor activity metadata is needed.
 auto sample=[&]() __attribute__((always_inline)) {
  l=rr=0;advance_legacy_voice(v);
  v.sampler.next(l,rr);l*=0.5f*v.a;rr*=0.5f*v.a;
  if(v.kit_watch_lane<2 && __atomic_load_n(&kit_watch_active_,__ATOMIC_ACQUIRE) && (__builtin_fabsf(l)>1e-9f || __builtin_fabsf(rr)>1e-9f))__atomic_add_fetch(&kit_watch_frames_[v.kit_watch_lane],1,__ATOMIC_RELEASE);
 };
 if(dsp_profile_.clock()){
  DspTimingScope timer(dsp_profile_,DspProfile::voice(unsigned(Instrument::Sampler),v.model),v.env>=.00001f);sample();
 }else sample();
}
__attribute__((noinline)) void Engine::render_non_sampler_voice(unsigned t,Voice& v,float& l,float& rr,bool poly,unsigned voice_key){
 unsigned variant=0;if(dsp_profile_.clock())variant=v.instrument==unsigned(Instrument::Texture)?unsigned(v.texture_values[0])*4+unsigned(v.texture_values[11]):v.instrument==unsigned(Instrument::Resonator)?unsigned(v.resonator_values[0]):v.model;
 bool native=v.instrument==unsigned(Instrument::Texture) || v.instrument==unsigned(Instrument::Resonator);
 DspTimingScope timer(dsp_profile_,DspProfile::voice(v.instrument,variant),native?(!poly && native_budget_.held(t)):v.env>=.00001f,native?t:16);
 l=rr=0;
 // Unknown engines (unregistered GUID / newer schema, SPEC 2.9 fallback)
 // are silent; without this guard value 20 would fall into the generic
 // synthesizer branch below and produce sound.
 if(v.instrument==unsigned(Instrument::Layer) || v.instrument==unsigned(Instrument::Empty) || v.instrument>=unsigned(Instrument::Unknown))return;
 if(v.instrument==unsigned(Instrument::SoundFont)){if(v.env<.00001f){v.soundfont.stop();return;}if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::SoundFont)v.soundfont_base=track(t).soundfont;float values[5];resolve_voice_bank(t,Instrument::SoundFont,v.soundfont_base.values,values);v.soundfont.parameters(values);}v.soundfont.next(v.frequency,!v.released,l,rr);v.env=v.soundfont.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Additive)){if(v.env<.00001f)return;if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Additive)v.additive_base=track(t).additive;float values[25];resolve_voice_bank(t,Instrument::Additive,v.additive_base.values,values);v.additive.parameters(values);}l=rr=v.additive.next(v.frequency,!v.released);v.env=v.additive.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::PhaseDist)){if(v.env<.00001f)return;if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::PhaseDist)v.phase_dist_base=track(t).phase_dist;float values[9];resolve_voice_bank(t,Instrument::PhaseDist,v.phase_dist_base.values,values);v.phase_dist.parameters(values);}l=rr=v.phase_dist.next(v.frequency,!v.released);v.env=v.phase_dist.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Granular)){if(v.env<.00001f || !granular_ || v.shared_slot>=4)return;auto& dsp=granular_[v.shared_slot];if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Granular)v.granular_base=track(t).granular;float values[kGranularParameterCount];resolve_voice_bank(t,Instrument::Granular,v.granular_base.values,values);dsp.parameters(values);}dsp.next(v.frequency,!v.released,l,rr);v.env=dsp.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Formant)){if(v.env<.00001f)return;if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Formant)v.formant_base=track(t).formant;float values[9];resolve_voice_bank(t,Instrument::Formant,v.formant_base.values,values);v.formant.parameters(values);}l=rr=v.formant.next(v.frequency,!v.released);v.env=v.formant.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Fm6)){
  if(v.env<=0)return;
  if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Fm6)v.fm6_base=track(t).fm6;float values[21];resolve_voice_bank(t,Instrument::Fm6,v.fm6_base.values,values);v.fm6.parameters(values);}
  l=rr=v.fm6.next(v.frequency,!v.released);v.env=v.fm6.active()?1:0;return;
 }
 if(v.instrument==unsigned(Instrument::VirtualAnalog)){
  if(v.env<=0)return;
  if((sample_count_&31)==0){auto parameter_clock=dsp_profile_.clock();unsigned parameter_start=parameter_clock?parameter_clock():0;float values[kVirtualAnalogParameterCount];bool resolved=false;
   if(!v.sound_locked && track(t).instrument==Instrument::VirtualAnalog){v.va_base=track(t).virtual_analog;auto& cached=va_resolved_[t];
    if(cached.sample==sample_count_ && cached.generation==generation_){__builtin_memcpy(values,cached.values,sizeof values);resolved=true;}
    else{auto& r=runtime_[t];if(resolve_unlayered_engine(*project_,track(t),t,Instrument::VirtualAnalog,r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,v.va_base.values,values)){__builtin_memcpy(cached.values,values,sizeof values);cached.sample=sample_count_;cached.generation=generation_;resolved=true;}}
   }
   if(!resolved)resolve_voice_bank(t,Instrument::VirtualAnalog,v.va_base.values,values);
   v.va.parameters(values);if(parameter_clock)dsp_profile_.va_parameters(parameter_clock()-parameter_start);
  }
  if(!(va_helper_ && va_job_sample_[voice_key]==sample_count_ && va_helper_->take(&v,sample_count_,va_job_[voice_key],v.va,v.frequency,!v.released,l)))l=v.va.next(v.frequency,!v.released);
  rr=l;v.env=v.va.active()?__builtin_fmaxf(v.va.level(),.0001f):0;return;
 }
 if(v.instrument==unsigned(Instrument::Wavetable)){
  if(v.env<=0)return;
  if((sample_count_&31)==0){auto& r=runtime_[t];const auto& state=track(t);if(!v.sound_locked && state.instrument==Instrument::Wavetable)v.wavetable_base=state.wavetable;float values[8];
   if(!resolve_unlayered_engine(*project_,state,t,Instrument::Wavetable,r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,v.wavetable_base.values,values)){ModRuntime mod=mod_runtime(t);for(unsigned i=0;i<8;++i)values[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::Wavetable,i),v.wavetable_base.values[i],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;}v.wavetable.parameters(values);
  }
  l=rr=v.wavetable.next(v.frequency,!v.released);v.env=v.wavetable.active()?__builtin_fmaxf(v.wavetable.level(),.0001f):0;return;
 }
 if(v.instrument==unsigned(Instrument::Texture)){
  if(!native_budget_.held(t) && sample_count_<native_retry_sample_[t])return;
  if(!textures_ || textures_suspended_ || poly || (track(t).instrument!=Instrument::Texture && !v.sound_locked))return;
  auto& dsp=textures_[t];auto& r=runtime_[t];
  if(dsp.boundary()){const float* bases=v.sound_locked?v.texture_state.values:track(t).texture.values;
   if(!resolve_unlayered_engine(*project_,track(t),t,Instrument::Texture,r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,bases,v.texture_values,632,12)){
    ModRuntime mod=mod_runtime(t);const auto& descriptor=*engine_descriptor(unsigned(Instrument::Texture));float values[15];
    for(unsigned i=0;i<descriptor.parameters.count;++i)values[i]=resolve_target_value(*project_,track(t),t,engine_target(Instrument::Texture,i),bases[descriptor.parameters.declarations[i].offset],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;
    descriptor.bind(descriptor.parameters,values,descriptor.parameters.count,{nullptr,nullptr,nullptr,nullptr,v.texture_values});
    v.texture_values[12]=resolve_target_value(*project_,track(t),t,632,bases[12],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;
   }
  }
  if(dsp.boundary()){if(!admit_native(t,Instrument::Texture,v.texture_values) && !native_budget_.held(t))return;for(unsigned i=0;i<15;++i)v.texture_values[i]=native_values_[t][i];}
  unsigned source=unsigned(v.texture_values[12]);const auto& input=texture_previous_[source<=60?source:0];dsp.next(v.texture_values,input.l,input.r,l,rr);return;
 }
 if(v.instrument==unsigned(Instrument::Resonator)){
  if(!native_budget_.held(t) && sample_count_<native_retry_sample_[t])return;
  if(!resonators_ || poly)return;
  auto& dsp=resonators_[t];auto& r=runtime_[t];
  if(dsp.boundary()){const float* bases=v.sound_locked?v.resonator_state.values:track(t).resonator.values;
   if(!resolve_unlayered_engine(*project_,track(t),t,Instrument::Resonator,r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,bases,v.resonator_values)){
    ModRuntime mod=mod_runtime(t);const auto& descriptor=*engine_descriptor(unsigned(Instrument::Resonator));float values[8];
    for(unsigned i=0;i<descriptor.parameters.count;++i)values[i]=resolve_target_value(*project_,track(t),t,engine_target(Instrument::Resonator,i),bases[descriptor.parameters.declarations[i].offset],r.active_locks,unsigned(r.automation_cell),control_all_,control_all_id_,control_all_delta_,r.automation_live,&gesture_overlay_,r.pattern,nullptr,&mod).value;
    descriptor.bind(descriptor.parameters,values,descriptor.parameters.count,{nullptr,nullptr,nullptr,nullptr,v.resonator_values});
   }
  }
  if(dsp.boundary()){if(!admit_native(t,Instrument::Resonator,v.resonator_values) && !native_budget_.held(t))return;for(unsigned i=0;i<8;++i)v.resonator_values[i]=native_values_[t][i];}
  dsp.next(v.resonator_values,v.pitch_bend,l,rr);return;
 }
 advance_legacy_voice(v);
   if(v.instrument>=unsigned(Instrument::WaveSequence) && v.instrument<=unsigned(Instrument::SpectralFreeze) && v.env>=0.00001f){l=rr=(v.composite?v.leaf->creative:creative_[t]).next(v.instrument,v.frequency,float(rate_),float(bpm_),v.parameters,v.creative_state,samples_,v.sample_slot,v.released,feedback_input_)*v.a*v.env;v.env*=v.released?0.998f:0.999995f;}
   else if((v.instrument==unsigned(Instrument::DaisyAnalogDrums) || v.instrument==unsigned(Instrument::DaisySyntheticDrums)) && v.env>=0.00001f){l=rr=public_drums_[t].next()*0.4f*v.a*v.env;if(v.released)v.env*=0.998f;}
   else if(v.instrument==unsigned(Instrument::PlaitsMacro)){if(auto* dsp=plaits_dsp(t,v)){if(v.env>=0.00001f && !dsp->gated()){l=rr=dsp->next()*0.5f*v.env;if(v.released)v.env*=0.998f;}else{dsp->stop();v.env=0;}}}
   else if(v.instrument==unsigned(Instrument::DaisySynth)) l=rr=v.daisy.process()*0.25f;
   else if((v.instrument==unsigned(Instrument::PolySaw) || v.instrument==unsigned(Instrument::FM) || v.instrument==unsigned(Instrument::Acid)) && v.env>=0.00001f) {
    float x=0;
    if(v.instrument==unsigned(Instrument::PolySaw)) x=v.saw.process()*v.a;
    else if(v.instrument==unsigned(Instrument::FM)) { v.fm.SetFrequency(v.frequency);v.fm.SetRatio(fm_ratio(v.parameters[0]));v.fm.SetIndex(fm_index(v.parameters[1])*0.7957747f*v.env);x=v.fm.Process()*v.a; }
    else if(v.instrument==unsigned(Instrument::Acid)) {
     float saw=v.saw.process(),pulse=saw>=0?1.0f:-1.0f;float input=(saw+(pulse-saw)*v.parameters[4])*v.a;
     float cutoff=acid_cutoff(v.parameters,v.env);if(v.step_accent)cutoff+=600+2500*v.env;
     float f=2*sine_table(3.14159265f*cutoff/(float(rate_)*2));if(f>0.7f)f=0.7f;float damping=acid_damping(v.parameters[1]);
     for(unsigned oversample=0;oversample<2;++oversample){v.acid_low+=f*v.acid_band;float high=input-v.acid_low-damping*v.acid_band;v.acid_band+=f*high;v.acid_band=soft_clip(v.acid_band,0.3f);}
     x=v.acid_low;if(v.step_accent)x*=1.6f;
    }
    float gain=v.instrument==unsigned(Instrument::Acid)?0.15f:0.035f;
    l=rr=x*v.env*gain;if(v.released)v.env*=0.99f;else if(v.instrument==unsigned(Instrument::Acid))v.env*=acid_decay(v.parameters[3]);else if(!poly)v.env*=0.9994f;
   }
}
void Engine::publish_voice_visual(AudioVisualFrame& frame,unsigned t,Voice& v){
 frame.voice_level_valid[t]=true;float level=0;
 switch(Instrument(v.instrument)){
 case Instrument::Formant:level=v.formant.level();v.formant.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::Granular:if(granular_ && v.shared_slot<4){auto& dsp=granular_[v.shared_slot];level=dsp.level();dsp.visual_envelope(frame.engine_envelope[t]);}else frame.voice_level_valid[t]=false;break;
 case Instrument::Additive:level=v.additive.level();v.additive.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::PhaseDist:level=v.phase_dist.level();v.phase_dist.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::VirtualAnalog:level=v.va.level();v.va.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::Wavetable:level=v.wavetable.level();v.wavetable.visual_envelope(frame.engine_envelope[t]);break;
 default:frame.voice_level_valid[t]=false;break;
 }
 frame.voice_level[t]=v.env<.00001f?0:level;
}
}
