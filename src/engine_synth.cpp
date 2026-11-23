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
void Engine::present_chimera(AudioVisualFrame& frame){
 frame.chimera_available=chimera_slots_!=0;
 frame.chimera_live=false;
 if(!chimera_slots_)return;
 const auto selected=frame.taps.track;
 const PolyVoice* newest=nullptr;
 for(const auto& poly:poly_voices_)if(poly.owner==selected && poly.voice.env>0 &&
     poly.voice.instrument==unsigned(Instrument::Chimera) && poly.voice.shared_slot<chimera_slots_ &&
     (!newest || poly.age>=newest->age))newest=&poly;
 if(newest){
  frame.chimera_live=true;
  chimeras_[newest->voice.shared_slot].geometry(frame.chimera);
 }
 else if(track(selected).instrument==Instrument::Chimera)
  chimeras_[0].geometry(frame.chimera,chimera_model(track(selected).chimera),false);
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
 if(v.instrument==unsigned(Instrument::Chimera)){
  auto* dsp=chimera_dsp(v);
  if(!dsp || v.env<=0)return;
  for(unsigned p=0;p<5;++p)v.parameters[p]+=(v.targets[p]-v.parameters[p])*.005f;
  const auto& runtime=runtime_[t];
  const float increment=chimera_score_increment();
  const bool early=v.score_before_step && v.score_step==runtime.step_serial+1;
  const float lead=v.voice_score?v.voice_score->lead:0;
  const float aligned=early?__builtin_fmaxf(0,lead-float(int64_t(runtime.due)-int64_t(sample_count_))*increment):-1;
  const bool update=dsp->control_due();
  ChimeraState detail;
  if(update){
   const auto state=track(t);const auto mod=mod_runtime(t);
   detail=v.sound_locked?v.chimera_base:state.chimera;
   for(unsigned i=0;i<kChimeraDetailCount;++i){
    const unsigned local=kChimeraMacroCount+i;
    if(chimera_sound_attribute(local))continue;
    detail.values[i]=resolve_target_value(*project_,state,t,engine_target(Instrument::Chimera,local),
      detail.values[i],runtime.active_locks,unsigned(runtime.automation_cell),control_all_,control_all_id_,
      control_all_delta_,runtime.automation_live,&gesture_overlay_,runtime.pattern,nullptr,&mod).value;
   }
   // Keep the program and species that were admitted for this note. An
   // active voice cannot switch to a differently priced creature in place.
   constexpr unsigned program_index=ChimeraProgram-kChimeraMacroCount;
   constexpr unsigned species_index=ChimeraSpecies-kChimeraMacroCount;
   detail.values[program_index]=v.scene_speech?float(ChimeraSpeak):v.chimera_base.values[program_index];
   detail.values[species_index]=v.chimera_base.values[species_index];
   if(chimera_voice_mode(detail)!=ChimeraLegato)detail.values[ChimeraGlide-kChimeraMacroCount]=0;
   const unsigned route=unsigned(detail.values[ChimeraTalkOutput-kChimeraMacroCount]);
   if(route && route<=kTracks && dsp->tongue(talk_pose_[route-1].xy))talk_pose_[route-1].until=sample_count_+96;
  }
  const float initial=early?aligned:(v.off_at?lead:0);
  for(unsigned n=0;n<v.tract_count;++n){
   auto& member=chimeras_[v.tract_slots[n]];
   if(update){
    member.update_model(detail);
    if(v.tract_count>1){
     const float offset=2.f*n/(v.tract_count-1)-1;
     v.tract_pitch[n]=exp2f(offset*detail.values[ChimeraSpread-kChimeraMacroCount]/1200.f);
    }else v.tract_pitch[n]=1;
   }
   l+=member.next(v.frequency*v.tract_pitch[n],v.a,!v.released,v.parameters,update?&detail:nullptr,
                  update?v.voice_score:nullptr,initial,increment,aligned);
  }
  if(update)v.model=uint8_t(dsp->model());
  rr=l=l/v.tract_count;
  if(!dsp->active())v.env=0;
  return;
 }
 if(bundle_helper_ && bundle_take(t,v,voice_key,l,rr))return;
 if(v.instrument==unsigned(Instrument::SoundFont)){if(v.env<.00001f){v.soundfont.stop();return;}if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::SoundFont)v.soundfont_base=track(t).soundfont;float values[5];resolve_voice_bank(t,Instrument::SoundFont,v.soundfont_base.values,values);v.soundfont.parameters(values);}v.soundfont.next(v.frequency,!v.released,l,rr);v.env=v.soundfont.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Additive)){if(v.env<.00001f)return;if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Additive)v.additive_base=track(t).additive;float values[25];resolve_voice_bank(t,Instrument::Additive,v.additive_base.values,values);v.additive.parameters(values);}l=rr=v.additive.next(v.frequency,!v.released);v.env=v.additive.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::PhaseDist)){if(v.env<.00001f)return;if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::PhaseDist)v.phase_dist_base=track(t).phase_dist;float values[9];resolve_voice_bank(t,Instrument::PhaseDist,v.phase_dist_base.values,values);v.phase_dist.parameters(values);}l=rr=v.phase_dist.next(v.frequency,!v.released);v.env=v.phase_dist.active()?1:0;return;}
 if(v.instrument==unsigned(Instrument::Granular)){if(v.env<.00001f || !granular_ || v.shared_slot>=kSharedVoiceCapacity)return;auto& dsp=granular_[v.shared_slot];if((sample_count_&63)==0){if(!v.sound_locked && track(t).instrument==Instrument::Granular)v.granular_base=track(t).granular;float values[kGranularParameterCount];resolve_voice_bank(t,Instrument::Granular,v.granular_base.values,values);dsp.parameters(values);}dsp.next(v.frequency,!v.released,l,rr);v.env=dsp.active()?1:0;return;}
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
  if((sample_count_&31)==0){
   const auto& state=track(t);
   if(!v.sound_locked && state.instrument==Instrument::Wavetable)v.wavetable_base=state.wavetable;
   float values[kWavetableParameterCount];resolve_voice_bank(t,Instrument::Wavetable,v.wavetable_base.values,values);
   v.wavetable.parameters(values);
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
   else if((v.instrument==unsigned(Instrument::DaisyAnalogDrums) || v.instrument==unsigned(Instrument::DaisySyntheticDrums)) && v.env>=0.00001f){l=rr=public_drums_[t].next(sample_count_)*0.4f*v.a*v.env;if(v.released)v.env*=0.998f;}
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
 case Instrument::Granular:if(granular_ && v.shared_slot<kSharedVoiceCapacity){auto& dsp=granular_[v.shared_slot];level=dsp.level();dsp.visual_envelope(frame.engine_envelope[t]);}else frame.voice_level_valid[t]=false;break;
 case Instrument::Additive:level=v.additive.level();v.additive.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::PhaseDist:level=v.phase_dist.level();v.phase_dist.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::VirtualAnalog:level=v.va.level();v.va.visual_envelope(frame.engine_envelope[t]);break;
 case Instrument::Wavetable:level=v.wavetable.level();v.wavetable.visual_envelope(frame.engine_envelope[t]);break;
 default:frame.voice_level_valid[t]=false;break;
 }
 frame.voice_level[t]=v.env<.00001f?0:level;
}
// ---- W2 render-ahead bundles -------------------------------------------
// Factored per-sample renders for the bundle-eligible voices. The canonical
// branches in render_non_sampler_voice perform the identical operations in
// the identical order: the env gate first, an optional parameters() call at
// a refresh sample, one next() call, then the branch's env rule. The bundle
// cross-check verifies this equivalence bit-exactly on real adoptions and
// disables bundles for the boot on any mismatch.
static float bundle_sample(AdditiveVoice& dsp,const float* params,float frequency,bool gate,float& env){
 if(env<0.00001f)return 0;
 if(params)dsp.parameters(params);
 float x=dsp.next(frequency,gate);env=dsp.active()?1:0;return x;
}
static float bundle_sample(PhaseDistVoice& dsp,const float* params,float frequency,bool gate,float& env){
 if(env<0.00001f)return 0;
 if(params)dsp.parameters(params);
 float x=dsp.next(frequency,gate);env=dsp.active()?1:0;return x;
}
static float bundle_sample(FormantVoice& dsp,const float* params,float frequency,bool gate,float& env){
 if(env<0.00001f)return 0;
 if(params)dsp.parameters(params);
 float x=dsp.next(frequency,gate);env=dsp.active()?1:0;return x;
}
static float bundle_sample(WavetableVoice& dsp,const float* params,float frequency,bool gate,float& env){
 if(env<=0)return 0;
 if(params)dsp.parameters(params);
 float x=dsp.next(frequency,gate);env=dsp.active()?__builtin_fmaxf(dsp.level(),0.0001f):0;return x;
}
static float bundle_sample(VirtualAnalogVoice& dsp,const float* params,float frequency,bool gate,float& env){
 if(env<=0)return 0;
 if(params)dsp.parameters(params);
 float x=dsp.next(frequency,gate);env=dsp.active()?__builtin_fmaxf(dsp.level(),.0001f):0;return x;
}
static const float* bundle_params(const RenderBundleJob& job,unsigned offset){
 for(unsigned ri=0;ri<job.refresh_count;++ri)if(job.refresh_offset[ri]==offset)return job.values;
 return nullptr;
}
static bool bundle_bits(float a,float b){uint32_t x,y;__builtin_memcpy(&x,&a,4);__builtin_memcpy(&y,&b,4);return x==y;}
// Render one sub-block from the canonical state and compare it, and a probe
// sample rendered from both end states, bit-exactly with the worker's.
template<class Dsp>
static bool bundle_cross_check(Dsp& canonical,const Dsp& checkpoint,const RenderBundleJob& job,unsigned offset){
 float env=offset?job.env[offset-1]:job.env0;
 Dsp scratch=canonical;
 for(unsigned o=offset;o<offset+RenderBundleJob::kSubFrames && o<job.frames;++o){
  float x=bundle_sample(scratch,bundle_params(job,o),job.frequency,!job.released,env);
  if(!bundle_bits(x,job.out[o]) || !bundle_bits(env,job.env[o]))return false;
 }
 Dsp probe=checkpoint;float probe_env=env;float probe_x=bundle_sample(probe,nullptr,job.frequency,!job.released,probe_env);
 float scratch_x=bundle_sample(scratch,nullptr,job.frequency,!job.released,env);
 return bundle_bits(scratch_x,probe_x) && bundle_bits(env,probe_env);
}
bool render_bundle_job(RenderBundleJob& job){
 const unsigned subs=(job.frames+RenderBundleJob::kSubFrames-1)/RenderBundleJob::kSubFrames;
 const unsigned sub=job.next_sub;
 if(sub>=subs)return false;
 const unsigned begin=sub*RenderBundleJob::kSubFrames;
 unsigned end=begin+RenderBundleJob::kSubFrames;if(end>job.frames)end=job.frames;
 for(unsigned offset=begin;offset<end;++offset){
  const float* params=bundle_params(job,offset);
  float x=0;
  switch(Instrument(job.instrument)){
   case Instrument::Additive: x=bundle_sample(job.live.additive,params,job.frequency,!job.released,job.env_live);break;
   case Instrument::PhaseDist: x=bundle_sample(job.live.phase_dist,params,job.frequency,!job.released,job.env_live);break;
   case Instrument::Formant: x=bundle_sample(job.live.formant,params,job.frequency,!job.released,job.env_live);break;
   case Instrument::Wavetable: x=bundle_sample(job.live.wavetable,params,job.frequency,!job.released,job.env_live);break;
   case Instrument::VirtualAnalog: x=bundle_sample(job.live.virtual_analog,params,job.frequency,!job.released,job.env_live);break;
   default:break;
  }
  job.out[offset]=x;job.env[offset]=job.env_live;
 }
 switch(Instrument(job.instrument)){
  case Instrument::Additive: new (&job.checkpoint[sub].additive) AdditiveVoice(job.live.additive);break;
  case Instrument::PhaseDist: new (&job.checkpoint[sub].phase_dist) PhaseDistVoice(job.live.phase_dist);break;
  case Instrument::Formant: new (&job.checkpoint[sub].formant) FormantVoice(job.live.formant);break;
  case Instrument::Wavetable: new (&job.checkpoint[sub].wavetable) WavetableVoice(job.live.wavetable);break;
  case Instrument::VirtualAnalog: new (&job.checkpoint[sub].virtual_analog) VirtualAnalogVoice(job.live.virtual_analog);break;
  default:break;
 }
 job.next_sub=sub+1;
 __atomic_store_n(&job.progress,sub+1,__ATOMIC_RELEASE);
 return sub+1<subs;
}
void Engine::build_render_bundles(){
 if(!bundle_helper_ || bundle_helper_->disabled())return;
 if(!bundle_tracking_ready_){for(unsigned k=0;k<16+kPolyVoiceCapacity;++k)bundle_job_[k]=-1;bundle_tracking_ready_=true;}
 for(unsigned k=0;k<16+kPolyVoiceCapacity;++k){
  int ji=bundle_job_[k];if(ji<0)continue;
  auto& job=bundle_helper_->at(unsigned(ji));
  if(sample_count_>=job.start_sample+job.frames)bundle_forget(k);
 }
 auto try_build=[&](unsigned t,Voice& v,unsigned key){
  if(bundle_job_[key]>=0)return;
  const unsigned instrument=v.instrument;
  const bool va=instrument==unsigned(Instrument::VirtualAnalog);
  if(instrument!=unsigned(Instrument::Additive) && instrument!=unsigned(Instrument::PhaseDist) &&
     instrument!=unsigned(Instrument::Formant) && instrument!=unsigned(Instrument::Wavetable) && !va)return;
  // VA renders the raw voice only; Layer gain/pan/detune and leaf FX stay on the
  // owner after the voice output, so Layer children (composite) may be bundled.
  if((!va && (v.composite || v.leaf)) || v.step_pitch.remaining || v.env<0.00001f)return;
  auto& r=runtime_[t];const auto& state=track(t);
  const uint64_t window_end=sample_count_+RenderBundleJob::kFrames;
  // No control event for this voice may fall inside the window: a scheduled
  // gate end, a due step (locks/automation/retune), a pending retrigger or
  // a clip follow. Anything else is caught by the per-sample validation.
  if(!v.released && v.off_at && v.off_at<window_end)return;
  if(r.playing && r.due && r.due<window_end)return;
  if(r.retriggers && r.retrigger_at<window_end)return;
  if(r.follow_due && r.grid<int64_t(window_end))return;
  const float* bases=nullptr;
  switch(Instrument(instrument)){
   case Instrument::Additive: bases=(v.sound_locked || state.instrument!=Instrument::Additive)?v.additive_base.values:state.additive.values;break;
   case Instrument::PhaseDist: bases=(v.sound_locked || state.instrument!=Instrument::PhaseDist)?v.phase_dist_base.values:state.phase_dist.values;break;
   case Instrument::Formant: bases=(v.sound_locked || state.instrument!=Instrument::Formant)?v.formant_base.values:state.formant.values;break;
   case Instrument::Wavetable: bases=(v.sound_locked || state.instrument!=Instrument::Wavetable)?v.wavetable_base.values:state.wavetable.values;break;
   case Instrument::VirtualAnalog: bases=(v.sound_locked || state.instrument!=Instrument::VirtualAnalog)?v.va_base.values:state.virtual_analog.values;break;
   default:return;
  }
  auto* job=bundle_helper_->claim();if(!job)return;
  __builtin_memset(job->values,0,sizeof job->values);
  if(!resolve_unlayered_engine(*project_,state,t,Instrument(instrument),r.active_locks,r.automation_live,&gesture_overlay_,r.pattern,control_all_,control_all_id_,bases,job->values)){
   bundle_helper_->retire(*job);return; // resolution is time-varying: canonical only
  }
  job->key=&v;job->voice_key=key;job->track=t;job->instrument=instrument;
  job->start_sample=sample_count_;job->automation_tick=r.automation_tick;
  job->generation=generation_;job->epoch=v.voice_epoch;job->instance=v.voice_instance;
  job->frequency=v.frequency;job->released=v.released;job->env0=v.env;job->env_live=v.env;
  job->frames=RenderBundleJob::kFrames;job->param_count=0;job->refresh_count=0;
  const unsigned mask=(instrument==unsigned(Instrument::Wavetable) || va)?31u:63u;
  for(unsigned off=0;off<RenderBundleJob::kFrames;++off)
   if(((sample_count_+off)&mask)==0 && job->refresh_count<RenderBundleJob::kMaxRefresh)
    job->refresh_offset[job->refresh_count++]=uint16_t(off);
  switch(Instrument(instrument)){
   case Instrument::Additive: new (&job->live.additive) AdditiveVoice(v.additive);job->param_count=25;break;
   case Instrument::PhaseDist: new (&job->live.phase_dist) PhaseDistVoice(v.phase_dist);job->param_count=9;break;
   case Instrument::Formant: new (&job->live.formant) FormantVoice(v.formant);job->param_count=9;break;
   case Instrument::Wavetable: new (&job->live.wavetable) WavetableVoice(v.wavetable);job->param_count=kWavetableParameterCount;break;
   case Instrument::VirtualAnalog: new (&job->live.virtual_analog) VirtualAnalogVoice(v.va);job->live.virtual_analog.detach_trigger_bank();job->param_count=uint8_t(kVirtualAnalogParameterCount);break;
   default:break;
  }
  bundle_helper_->publish(*job);
  bundle_job_[key]=int(bundle_helper_->index(*job));
  bundle_sub_from_job_[key]=false;bundle_prev_from_job_[key]=false;
 };
 for(unsigned t=0;t<16;++t)try_build(t,voices_[t],t);
 for(unsigned i=0;i<kPolyVoiceCapacity;++i){auto& p=poly_voices_[i];if(p.owner<16 && !p.pending)try_build(p.owner,p.voice,16+i);}
}
void Engine::bundle_forget(unsigned key){
 int ji=bundle_job_[key];if(ji<0)return;
 bundle_helper_->retire(bundle_helper_->at(unsigned(ji)));
 bundle_job_[key]=-1;bundle_sub_from_job_[key]=false;bundle_prev_from_job_[key]=false;
}
bool Engine::bundle_take(unsigned t,Voice& v,unsigned voice_key,float& l,float& r){
 if(!bundle_tracking_ready_)return false;
 int ji=bundle_job_[voice_key];if(ji<0)return false;
 auto* slot=bundle_helper_;auto& job=slot->at(unsigned(ji));
 const uint64_t s=sample_count_;
 if(job.key!=&v || job.voice_key!=voice_key || s<job.start_sample || s>=job.start_sample+job.frames){
  bundle_forget(voice_key);return false;
 }
 const unsigned offset=unsigned(s-job.start_sample);
 const unsigned sub=offset/RenderBundleJob::kSubFrames;
 const bool same_voice=job.instance==v.voice_instance && job.instrument==v.instrument && job.generation==generation_;
 bool just_adopted=false;
 if(offset && (offset%RenderBundleJob::kSubFrames)==0){
  bundle_sub_from_job_[voice_key]=false; // a new sub-block re-enters the start path (progress + cross-check)
  if(bundle_prev_from_job_[voice_key] && same_voice){
  const unsigned done=sub-1;
  switch(Instrument(job.instrument)){
   case Instrument::Additive: v.additive=job.checkpoint[done].additive;break;
   case Instrument::PhaseDist: v.phase_dist=job.checkpoint[done].phase_dist;break;
   case Instrument::Formant: v.formant=job.checkpoint[done].formant;break;
   case Instrument::Wavetable: v.wavetable=job.checkpoint[done].wavetable;break;
   case Instrument::VirtualAnalog: v.va.adopt(job.checkpoint[done].virtual_analog);break;
   default:break;
  }
  ++bundle_adopted_subblocks_;slot->note_adopted(1,RenderBundleJob::kSubFrames);
  bundle_prev_from_job_[voice_key]=false;just_adopted=true;
  }
 }
 if(!same_voice || job.epoch!=v.voice_epoch ||
    job.released!=v.released || !bundle_bits(job.frequency,v.frequency) ||
    job.automation_tick!=runtime_[t].automation_tick){
  // A control event reached this voice inside the window. If part of the
  // current sub-block was already consumed from the job and the note
  // instance is unchanged, replay that prefix on the canonical state
  // (discarding the duplicate outputs) so it reaches this sample before
  // the canonical branch renders it. A new note instance was
  // re-initialised by its trigger and needs no replay.
  if(bundle_sub_from_job_[voice_key] && same_voice){
   const unsigned begin=offset-(offset%RenderBundleJob::kSubFrames);
   float env=begin?job.env[begin-1]:job.env0;
   for(unsigned o=begin;o<offset;++o){
    const float* params=bundle_params(job,o);
    switch(Instrument(job.instrument)){
     case Instrument::Additive: bundle_sample(v.additive,params,job.frequency,!job.released,env);break;
     case Instrument::PhaseDist: bundle_sample(v.phase_dist,params,job.frequency,!job.released,env);break;
     case Instrument::Formant: bundle_sample(v.formant,params,job.frequency,!job.released,env);break;
     case Instrument::Wavetable: bundle_sample(v.wavetable,params,job.frequency,!job.released,env);break;
     case Instrument::VirtualAnalog: bundle_sample(v.va,params,job.frequency,!job.released,env);break;
     default:break;
    }
   }
   v.env=env;slot->note_replayed(offset-begin);
  }
  slot->note_invalidated();bundle_forget(voice_key);return false;
 }
 if(!bundle_sub_from_job_[voice_key]){
  // Once canonical rendering starts for a sub-block, it owns the whole sub-block.
  if(offset%RenderBundleJob::kSubFrames)return false;
  if(slot->progress(job)<=sub){slot->note_late();return false;}
  if(offset==0 || (just_adopted && (bundle_adopted_subblocks_%256)==0)){
   bool ok=false;
   switch(Instrument(job.instrument)){
    case Instrument::Additive: ok=bundle_cross_check(v.additive,job.checkpoint[sub].additive,job,offset);break;
    case Instrument::PhaseDist: ok=bundle_cross_check(v.phase_dist,job.checkpoint[sub].phase_dist,job,offset);break;
    case Instrument::Formant: ok=bundle_cross_check(v.formant,job.checkpoint[sub].formant,job,offset);break;
    case Instrument::Wavetable: ok=bundle_cross_check(v.wavetable,job.checkpoint[sub].wavetable,job,offset);break;
    case Instrument::VirtualAnalog: ok=bundle_cross_check(v.va,job.checkpoint[sub].virtual_analog,job,offset);break;
    default:break;
   }
   slot->note_checked();
   if(!ok){slot->note_mismatch();slot->disable();bundle_forget(voice_key);return false;}
  }
  bundle_sub_from_job_[voice_key]=true;
 }
 l=r=job.out[offset];v.env=job.env[offset];
 if((offset+1)%RenderBundleJob::kSubFrames==0 || offset+1>=job.frames)bundle_prev_from_job_[voice_key]=true;
 if(offset+1>=job.frames){
  const unsigned last=job.frames/RenderBundleJob::kSubFrames-1;
  switch(Instrument(job.instrument)){
   case Instrument::Additive: v.additive=job.checkpoint[last].additive;break;
   case Instrument::PhaseDist: v.phase_dist=job.checkpoint[last].phase_dist;break;
   case Instrument::Formant: v.formant=job.checkpoint[last].formant;break;
   case Instrument::Wavetable: v.wavetable=job.checkpoint[last].wavetable;break;
   case Instrument::VirtualAnalog: v.va.adopt(job.checkpoint[last].virtual_analog);break;
   default:break;
  }
  ++bundle_adopted_subblocks_;slot->note_adopted(1,RenderBundleJob::kSubFrames);
  bundle_forget(voice_key);
 }
 return true;
}
}
