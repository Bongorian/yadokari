#pragma once
#include "project.h"
namespace groove {
enum ParameterSource:unsigned { FromMorph=1,FromAutomation=2,FromMacro=4,FromLock=8,FromControlAll=16,FromGesture=32 };
struct GestureOverlay {GesturePoint points[128]{};unsigned count=0;bool morph_active=false,macro_active[8]{};float morph=0,macros[8]{};
 void clear(){count=0;morph_active=false;for(auto& on:macro_active)on=false;}
 void apply(const GesturePoint& p){if(p.target==GestureTarget::Morph){morph_active=true;morph=p.value;return;}if(p.target==GestureTarget::Macro){macro_active[p.parameter]=true;macros[p.parameter]=p.value;return;}for(unsigned i=0;i<count;++i)if(points[i].track==p.track && points[i].pattern==p.pattern && points[i].parameter==p.parameter){points[i]=p;return;}if(count<128)points[count++]=p;}
};
struct ParameterValue {float value=0;unsigned sources=0;};
// Per-track dynamic modulation sources (Stage 5 S5-1). Values are unipolar
// 0..1 snapshots taken by the engine at the control boundary; a null context
// means dynamic-source routes are not evaluated (static contexts such as
// project validation and offline analysis).
struct ModRuntime {float lfo=0,pressure=0,envelope=0,velocity=0;};
inline float apply_layer(float base,float value,LayerMode mode){return mode==LayerMode::Add?base+value:mode==LayerMode::Multiply?base*value:value;}
inline ParameterValue resolve_target_value(const ProjectState& project,ConstTrackView track,unsigned track_index,ParameterTarget id,float base,const ParameterLock* locks,unsigned automation_cell,bool control_all=false,ParameterTarget control_id=0,float control_delta=0,bool automation_running=true,const GestureOverlay* gesture=nullptr,unsigned pattern=255,const ParameterLock* pad_locks=nullptr,const ModRuntime* mod=nullptr){
 ParameterValue result{base,0};if(unavailable_target(id) || !valid_target_address(id))return result;
 const bool local=engine_target(id);const unsigned index=local?target_local(id):morph_index(id);
 const bool opted=local || id<128 || id>=262 || (id<256 && (track.midi_cc_used[(id-128)/8]&(1u<<((id-128)%8)))) || (id>=256 && id<260 && track.nrpn[id-256].enabled) || (id>=260 && id<=261 && (track.expression_used&(1u<<(id-260))));
 if((project.morph_enabled || (gesture && gesture->morph_active)) && opted && (local || index<kCommonMorphParameters)){
  unsigned owner=!local && morph_project_parameter(id)?0:track_index;const auto& a=project.morph_a[owner];const auto& b=project.morph_b[owner];
  if(!local || (a.engine==target_engine(id) && b.engine==target_engine(id))){result.value=interpolate_target(id,local?a.engine_values[index]:a.values[index],local?b.engine_values[index]:b.values[index],gesture && gesture->morph_active?gesture->morph:project.morph_position);result.sources|=FromMorph;}
 }
 for(const auto& lane:track.automation)if(automation_running && lane.enabled && lane.parameter==id){result.value=apply_layer(result.value,lane.values[automation_cell%lane.length],lane.mode);result.sources|=FromAutomation;}
 if(gesture)for(unsigned i=0;i<gesture->count;++i){const auto& e=gesture->points[i];if(e.track==track_index && e.pattern==pattern && e.parameter==id){result.value=e.value;result.sources|=FromGesture;}}
 for(unsigned mi=0;mi<8;++mi){const auto& macro=project.macros[mi];bool overridden=gesture && gesture->macro_active[mi];float macro_value=overridden?gesture->macros[mi]:macro.value;if(macro.active || overridden)for(const auto& route:macro.routes)if(route.enabled && route.parameter==id && (route.tracks&(1u<<track_index))){float drive=macro_value;if(route.source!=ModSource::Macro){if(!mod)continue;drive=route.source==ModSource::Lfo?mod->lfo:route.source==ModSource::Pressure?mod->pressure:route.source==ModSource::Envelope?mod->envelope:mod->velocity;}float position=macro_curve(drive,route.curve);float v=route.mode==LayerMode::Replace?interpolate_target(id,route.low,route.high,position):route.low+(route.high-route.low)*position;result.value=apply_layer(result.value,v,route.mode);result.sources|=FromMacro;}}
 if(locks)for(unsigned i=0;i<kLocks;++i)if(locks[i].enabled && locks[i].id==id){result.value=locks[i].value;result.sources|=FromLock;}
 if(pad_locks)for(unsigned i=0;i<kLocks;++i)if(pad_locks[i].enabled && pad_locks[i].id==id){result.value=pad_locks[i].value;result.sources|=FromLock;}
 if(control_all && id==control_id){const auto* d=target_descriptor(id);result.value+=control_delta*(d->maximum-d->minimum);result.sources|=FromControlAll;}
 result.value=clamp_target(id,result.value);return result;
}
inline ParameterValue resolve_parameter(const ProjectState& project,ConstTrackView track,unsigned track_index,uint16_t id,const ParameterLock* locks,unsigned automation_cell,bool control_all=false,uint16_t control_id=0,float control_delta=0,const float* sound=nullptr,bool automation_running=true,const GestureOverlay* gesture=nullptr,unsigned pattern=255,const SampleEdit* sample_sound=nullptr,const ParameterLock* pad_locks=nullptr,const ModRuntime* mod=nullptr,const ResonatorState* resonator_sound=nullptr,const TextureState* texture_sound=nullptr,Instrument morph_engine=Instrument::Unknown){
 const float sample_values[]={sample_sound?sample_sound->speed:1,sample_sound?sample_sound->start:0,sample_sound?sample_sound->end:1,sample_sound?sample_sound->fade_ms:2,sample_sound?sample_sound->crossfade_ms:4,sample_sound?sample_sound->attack_ms:0,sample_sound?sample_sound->release_ms:8};
 ParameterValue result{texture_sound && id>=620 && id<635?texture_sound->values[id-620]:resonator_sound && id>=600 && id<608?resonator_sound->values[id-600]:sample_sound && id>=80 && id<=86?sample_values[id-80]:sound && id>=32 && id<=36?sound[id-32]:parameter_base(project,track,id),0};
 if(morph_engine==Instrument::Unknown)morph_engine=track.instrument;
 ParameterTarget target=bind_parameter_target(morph_engine,id);
 if(!valid_target_address(target)){result.value=clamp_parameter(id,result.value);return result;}
 return resolve_target_value(project,track,track_index,target,result.value,locks,automation_cell,control_all,control_id,control_delta,automation_running,gesture,pattern,pad_locks,mod);
}
// Native blocks use engine ownership and declaration offsets directly. Check
// shared layers once; when none match, resolution reduces to clamped bases.
// No cache survives a block, so edits, Sound locks and gestures stay immediate.
inline bool resolve_unlayered_engine(const ProjectState& project,ConstTrackView track,unsigned track_index,Instrument engine,const ParameterLock* locks,bool automation_running,const GestureOverlay* gesture,unsigned pattern,bool control_all,uint16_t control_id,const float* bases,float* output,ParameterTarget common=0,unsigned common_offset=0){
 const auto* descriptor=engine_descriptor(unsigned(engine));if(!descriptor)return false;
 auto inside=[&](ParameterTarget target){return (engine_target(target) && !unavailable_target(target) && target_engine(target)==engine) || (common && target==common);};
 if(project.morph_enabled || (gesture && gesture->morph_active) || (control_all && inside(control_id)))return false;
 if(automation_running)for(const auto& lane:track.automation)if(lane.enabled && inside(lane.parameter))return false;
 if(gesture)for(unsigned i=0;i<gesture->count;++i){const auto& e=gesture->points[i];if(e.track==track_index && e.pattern==pattern && inside(e.parameter))return false;}
 for(unsigned mi=0;mi<8;++mi)if(project.macros[mi].active || (gesture && gesture->macro_active[mi]))for(const auto& route:project.macros[mi].routes)if(route.enabled && inside(route.parameter) && (route.tracks&(1u<<track_index)))return false;
 if(locks)for(unsigned i=0;i<kLocks;++i)if(locks[i].enabled && inside(locks[i].id))return false;
 for(unsigned i=0;i<descriptor->parameters.count;++i){unsigned offset=descriptor->parameters.declarations[i].offset;output[offset]=clamp_target(engine_target(engine,i),bases[offset]);}
 if(common)output[common_offset]=clamp_target(common,bases[common_offset]);
 return true;
}
// Modulation is the final, sample-rate layer, after the smoothed control target.
inline float clamp_cutoff(float value){return value<0?0:value>1?1:value;}
inline float modulated_cutoff(float value,float depth,float phase,float pressure,float pressure_depth){float modulation=depth!=0?depth*(0.5f+0.5f*sinf(phase*6.2831853f)):0;return clamp_cutoff(value*(1-modulation)+pressure*pressure_depth);}
}
