#include "parameter_resolver.h"
#include <cassert>
#include <memory>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,2);ParameterLock locks[kLocks]{};GestureOverlay g;float out[15];ModRuntime mod{};
 for(unsigned bank=0;bank<2;++bank){t.instrument=bank?Instrument::Texture:Instrument::Resonator;unsigned first=bank?620:600,count=bank?15:8;const float* base=bank?t.texture.values:t.resonator.values;
  auto check=[&](bool expected,bool running=true,bool all=false,uint16_t control=0){bool fast=resolve_unlayered_engine(*p,t,2,t.instrument,locks,running,&g,0,all,control,base,out,bank?632:0,12);assert(fast==expected);if(fast)for(unsigned i=0;i<count;++i)assert(out[i]==resolve_parameter(*p,t,2,first+i,locks,0,all,control,0,nullptr,running,&g,0,nullptr,nullptr,&mod).value);};
  check(true);
  // References owned by another engine (including a retained unavailable one)
  // must not force this native bank through the layered resolver.
  locks[0]={engine_target(Instrument::FM,0),.5f,true};check(true);
  locks[0].id=ParameterTarget(0xc000);check(true);locks[0].enabled=false;
  if(bank){locks[0]={632,1,true};check(false);locks[0].enabled=false;}
  p->morph_enabled=true;check(false);p->morph_enabled=false;g.morph_active=true;check(false);g.morph_active=false;
  t.automation[0].enabled=true;t.automation[0].parameter=bind_parameter_target(t.instrument,uint16_t(first+2));check(false);check(true,false);t.automation[0].parameter=3;check(true);t.automation[0].enabled=false;
  g.count=1;g.points[0].parameter=bind_parameter_target(t.instrument,uint16_t(first+2));g.points[0].track=2;g.points[0].pattern=0;check(false);g.points[0].pattern=1;check(true);g.clear();
  auto& macro=p->macros[0];auto saved=macro;macro.active=true;auto& route=macro.routes[0];route.enabled=true;route.parameter=bind_parameter_target(t.instrument,uint16_t(first));route.tracks=1<<2;check(false);route.tracks=1<<1;check(true);macro.active=false;route.tracks=1<<2;g.macro_active[0]=true;check(false);g.clear();macro=saved;
  locks[0]={bind_parameter_target(t.instrument,uint16_t(first+count-1)),.5f,true};check(false);locks[0].id=3;check(true);locks[0].enabled=false;
  check(false,true,true,bind_parameter_target(t.instrument,uint16_t(first)));check(true,true,true,3);
  // A Sound snapshot is used as the base when that sound is locked.
  float sound[15];for(unsigned i=0;i<count;++i)sound[i]=parameter_descriptor(first+i)->maximum;
  assert(resolve_unlayered_engine(*p,t,2,t.instrument,locks,true,&g,0,false,0,sound,out,bank?632:0,12));for(unsigned i=0;i<count;++i)assert(out[i]==sound[i]);
 }
}
