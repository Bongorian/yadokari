#pragma once
#include "engine_registry.h"
namespace groove {
// Compact audio/UI handle, never persisted as an engine ordinal. Common targets
// retain their track/project IDs; engine targets hold owner + local declaration.
using ParameterTarget=uint16_t;
constexpr ParameterTarget kNoParameterTarget=0xffff;
inline bool engine_target(ParameterTarget target){return (target&0x8000)!=0 && target!=kNoParameterTarget;}
// Unavailable owners are project-local handles in a disjoint range. Their
// GUID strings never enter the audio resolver; ProjectState remains copyable.
constexpr unsigned kUnavailableTargetOwners=64;
struct UnavailableTargetOwners {
 uint8_t count=0;
 char guid[kUnavailableTargetOwners][65]{};
 int intern(const char* name,unsigned length){
  for(unsigned i=0;i<count;++i)if(!__builtin_strcmp(guid[i],name))return int(i);
  if(count>=kUnavailableTargetOwners || !length || length>64)return -1;
  unsigned index=count++;__builtin_memcpy(guid[index],name,length);guid[index][length]=0;return int(index);
 }
 bool valid()const{if(count>kUnavailableTargetOwners)return false;for(unsigned i=0;i<count;++i){unsigned n=0;while(n<65 && guid[i][n]){unsigned char c=guid[i][n++];if(c<33 || c>126)return false;}if(!n || n>64)return false;}return true;}
};
inline bool unavailable_target(ParameterTarget target){return (target&0xf800)==0xc000;}
inline unsigned unavailable_owner(ParameterTarget target){return (target>>5)&63;}
inline bool target_owner_present(ParameterTarget target,const UnavailableTargetOwners& owners){return !unavailable_target(target) || unavailable_owner(target)<owners.count;}
inline Instrument target_engine(ParameterTarget target){return unavailable_target(target)?Instrument::Unknown:Instrument((target&0x7fff)>>5);}
inline unsigned target_local(ParameterTarget target){return target&31;}
inline ParameterTarget engine_target(Instrument engine,unsigned local){return local<32?ParameterTarget(0x8000|(unsigned(engine)<<5)|local):kNoParameterTarget;}
inline const ParameterDescriptor* target_descriptor(ParameterTarget target){if(unavailable_target(target)){static const ParameterDescriptor missing={0,"UNAVAILABLE",-1000000,1000000,0,Interpolation::Linear,"",3,1};return &missing;}if(engine_target(target)){if(((target&0x7fff)>>5)>=instrument_count())return nullptr;const auto* d=engine_parameter(target_engine(target),target_local(target));return d?&d->value:nullptr;}return parameter_descriptor(target);}
inline bool valid_target_address(ParameterTarget target){return target_descriptor(target) && (engine_target(target) || morph_common_parameter(target));}
inline bool valid_target(ParameterTarget target,float value){const auto* d=target_descriptor(target);return valid_target_address(target) && value>=d->minimum && value<=d->maximum && (!engine_target(target) || d->interpolation!=Interpolation::Discrete || __builtin_floorf(value)==value);}
// Only UI/DSP adapters accept historical numeric controls; stored references do not.
inline ParameterTarget bind_parameter_target(Instrument engine,uint16_t control){if(engine_target(control) || control==kNoParameterTarget)return control;int local=engine_local_parameter(engine,control);return local>=0?engine_target(engine,unsigned(local)):morph_common_parameter(control)?control:kNoParameterTarget;}
inline float clamp_target(ParameterTarget target,float value){const auto* d=target_descriptor(target);if(!d)return 0;value=__builtin_fmaxf(d->minimum,__builtin_fminf(d->maximum,value));return d->interpolation==Interpolation::Discrete || (!engine_target(target) && target>=128 && target<=261)?__builtin_floorf(value+.5f):value;}
inline float interpolate_target(ParameterTarget target,float a,float b,float mix){const auto* d=target_descriptor(target);if(!d)return a;mix=__builtin_fmaxf(0,__builtin_fminf(1,mix));if(d->interpolation==Interpolation::Discrete)return mix<.5f?a:b;if(d->interpolation==Interpolation::Logarithmic && a>0 && b>0)return expf(logf(a)+(logf(b)-logf(a))*mix);return a+(b-a)*mix;}
inline bool valid_target_layer(ParameterTarget target,LayerMode mode,float value){const auto* d=target_descriptor(target);if(!valid_target_address(target) || unsigned(mode)>2)return false;return mode==LayerMode::Replace?valid_target(target,value):mode==LayerMode::Multiply?value>=0 && value<=2:value>=-(d->maximum-d->minimum) && value<=d->maximum-d->minimum;}
inline bool valid_automation(const AutomationLane& lane){if(!lane.length || lane.length>kAutomationSteps || unsigned(lane.mode)>2 || !valid_target_address(lane.parameter))return false;for(float v:lane.values)if(!valid_target_layer(lane.parameter,lane.mode,v))return false;return true;}
// All selectors use common declarations + the selected engine's local table.
inline unsigned target_count(Instrument engine){const auto* d=engine_descriptor(unsigned(engine));return kCommonMorphParameters+(d?d->parameters.count:0);}
inline ParameterTarget target_at(Instrument engine,unsigned index){return index<kCommonMorphParameters?morph_parameter_at(index):index<target_count(engine)?engine_target(engine,index-kCommonMorphParameters):kNoParameterTarget;}
inline unsigned target_index(Instrument engine,ParameterTarget target){if(unavailable_target(target))return target_count(engine);if(engine_target(target))return target_engine(target)==engine?kCommonMorphParameters+target_local(target):target_count(engine);return morph_index(target);}
inline ParameterTarget next_target(Instrument engine,ParameterTarget current,int change){int index=int(target_index(engine,current));int limit=int(target_count(engine))-1;index+=change;if(index<0)index=0;if(index>limit)index=limit;return target_at(engine,unsigned(index));}
inline uint16_t engine_ui_reference(Instrument engine,EngineParameterReference ref){if(ref.scope==EngineParameterScope::Track)return ref.index;if(ref.scope==EngineParameterScope::Engine){const auto* d=engine_parameter(engine,ref.index);return d?engine_target(engine,ref.index):0;}return 0;}
inline const char* target_owner_name(ParameterTarget target,const UnavailableTargetOwners& owners){if(unavailable_target(target))return target_owner_present(target,owners)?owners.guid[unavailable_owner(target)]:"Unavailable engine";const auto* d=engine_descriptor(unsigned(target_engine(target)));return d?d->instrument.name:"Unavailable engine";}
// Wire reference: zero + common u16, or GUID length + GUID bytes + local u16.
// Unknown owners/declarations remain dormant and round-trip through the table.
template<class W> bool write_parameter_target(W& w,ParameterTarget target,const UnavailableTargetOwners* owners=nullptr){
 if(!engine_target(target))return w.u8(0) && w.u16(target);
 if(!valid_target_address(target))return false;
 if(unavailable_target(target) && (!owners || !target_owner_present(target,*owners)))return false;
 const char* guid=unavailable_target(target)?owners->guid[unavailable_owner(target)]:engine_guid(target_engine(target));
 unsigned n=0;while(n<65 && guid[n])++n;if(!n || n>64 || !w.u8(uint8_t(n)))return false;
 for(unsigned i=0;i<n;++i)if(!w.u8(uint8_t(guid[i])))return false;
 return w.u16(uint16_t(target_local(target)));
}
template<class R> bool read_parameter_target(R& r,ParameterTarget& target,UnavailableTargetOwners* owners=nullptr){
 uint8_t n;if(!r.u8(n) || n>64)return false;
 if(!n)return r.u16(target) && !engine_target(target) && target!=kNoParameterTarget;
 char guid[65]{};for(unsigned i=0;i<n;++i){uint8_t c;if(!r.u8(c) || c<33 || c>126)return false;guid[i]=char(c);}
 Instrument engine;uint16_t local;if(!r.u16(local) || local>=32)return false;
 if(engine_from_guid(guid,n,engine) && engine_parameter(engine,local)){target=engine_target(engine,local);return true;}
 int owner=owners?owners->intern(guid,n):-1;if(owner<0)return false;
 target=ParameterTarget(0xc000|(unsigned(owner)<<5)|local);return true;
}
}
