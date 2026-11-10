#include "engine.h"
#include "dsp_probe_workload.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
using namespace groove;
static uint32_t audit_tick=100;static uint32_t audit_clock(){return ++audit_tick;}
int main(){auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();auto p=std::make_unique<ProjectState>(),q=std::make_unique<ProjectState>();auto av=std::make_unique<AudioVisualFrame>(),bv=std::make_unique<AudioVisualFrame>();float aa[48]{},bb[48]{};unsigned frames=0;
 for(unsigned mode=0;mode<3;++mode){make_dsp_probe_project(*p,{mode?DspCondition::Voices:DspCondition::Idle,mode==2?Instrument::FM:Instrument::VirtualAnalog,0,2,8});*q=*p;for(auto& route:q->macros[7].routes)route.enabled=false;q->macros[7].active=true; // make_empty_project installs a default route: remove it explicitly for the neutral full resolver path.
  assert(a->stage_project(*p) && b->stage_project(*q));a->render(aa,0);b->render(bb,0);
  if(mode)for(unsigned t=0;t<2;++t)for(unsigned n=0;n<8;++n){Command c{CommandType::Trigger,uint8_t(t),uint8_t(n),100};c.parameter=1;c.scalar=float(60+3*n);assert(a->submit(c) && b->submit(c));}
  for(unsigned block=0;block<220;++block){if(block==60){Command c{CommandType::SetParameter,0,0,0};c.parameter=1;c.scalar=.6f;assert(a->submit(c) && b->submit(c));}a->render(aa,24);b->render_audit().begin(audit_clock,audit_tick,true);b->render(bb,24);assert(!std::memcmp(aa,bb,sizeof aa));bool had=a->pop_visual(*av),other=b->pop_visual(*bv);assert(had==other);
   if(had){assert(av->sample==bv->sample && av->generation==bv->generation);assert(!std::memcmp(av->sample_values,bv->sample_values,sizeof av->sample_values));assert(!std::memcmp(av->sources,bv->sources,sizeof av->sources));assert(!std::memcmp(av->waveform,bv->waveform,sizeof av->waveform));assert(!std::memcmp(av->engine_envelope,bv->engine_envelope,sizeof av->engine_envelope));assert(!std::memcmp(av->voice_level,bv->voice_level,sizeof av->voice_level));assert(!std::memcmp(av->controls_value,bv->controls_value,sizeof av->controls_value));++frames;}
  }
 }
 assert(frames>=6);std::printf("Visual plain/full resolver: %u snapshots, same sample/generation/scalars/source flags/waveform and exact audio passed\n",frames);
}
