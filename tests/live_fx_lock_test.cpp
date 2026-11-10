#include "performance.h"
#include <cassert>
#include <cstdio>
#include <memory>
int main(){using namespace groove;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 p->macros[0].routes[0]={true,472,0xffff,2,2,LayerMode::Replace};
 auto t=track_at(*p,0,0);t.instrument=Instrument::ExternalMidi;t.steps[0].lock_only=true;t.steps[0].locks[0]={472,1,true};t.steps[0].locks[1]={473,.8f,true};t.steps[1].enabled=true;
 auto e=std::make_unique<Engine>(8000);float audio[128]{};assert(e->stage_project(*p));e->render(audio,0);e->render(audio,64);
 assert(e->submit({CommandType::Start,0,0,0}));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Filter);MidiEvent event;while(e->pop_midi(event))assert((event.bytes[0]&0xf0)!=0x90 || event.bytes[2]==0);
 Command manual{CommandType::SetPerformanceFx,0,0,unsigned(PerformanceFx::Gate)};manual.scalar=.5f;assert(e->submit(manual));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Gate);
 manual.value=0;assert(e->submit(manual));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Filter);
 for(unsigned i=0;i<18;++i){e->render(audio,64);}assert(e->performance_fx_current()==PerformanceFx::Off);
 Command macro{CommandType::SetMacroValue,0,0,1};macro.scalar=1;assert(e->submit(macro));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Gate);manual.value=unsigned(PerformanceFx::Filter);assert(e->submit(manual));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Filter);manual.value=0;assert(e->submit(manual));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Gate);
 assert(e->submit({CommandType::Stop,0,0,0}));e->render(audio,64);assert(e->performance_fx_current()==PerformanceFx::Off);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Perform);assert(ui->soft_key(6));assert(ui->knob(5,2000));assert(ui->selected_step()==2);ui->set_shift(true);assert(ui->knob(3,2000));assert(ui->knob(4,1000));bool type=false,amount=false;for(const auto& l:t.steps[2].locks){type|=l.enabled && l.id==472 && l.value==2;amount|=l.enabled && l.id==473 && l.value>.5f;}assert(type && amount && t.steps[2].lock_only && !t.steps[2].enabled);assert(ui->undo());assert(ui->undo());assert(!t.steps[2].lock_only);assert(ui->redo());assert(ui->redo());assert(t.steps[2].lock_only);
 auto& lock_only=t.steps[3];lock_only.lock_only=true;lock_only.locks[0]={300,1,true};t.mutation.amount=100;auto mutated=mutation_step(t,3);assert(!mutated.enabled && mutated.locks[0].value==1);t.mutation.amount=0;
 assert(ui->set_control_all(true));bool group=false,send=false,parallel=false,live=false;for(unsigned i=0;i<20;++i){assert(ui->select_control_all_target(1,true));auto id=ui->control_all_parameter();group|=id==384;send|=id==448;parallel|=id==464;live|=id==472;}assert(group && send && parallel && live);
 assert(ui->set_control_all(false));ui->set_shift(false);ui->set_mode(UiMode::Modulation);assert(ui->soft_key(2));assert(ui->knob(1,15000));assert(ui->macro_route()==15);assert(ui->control(1,0x04) && ui->macro_route()==14);assert(ui->control(1,0));assert(ui->control(1,0x10) && ui->macro_route()==15);assert(ui->control(1,0));assert(ui->encoder_push());assert(ui->encoder(1) && p->macros[0].routes[15].parameter==4);assert(ui->encoder(-1) && p->macros[0].routes[15].parameter==3);assert(ui->encoder_push());ui->set_shift(true);assert(ui->knob(5,1000));assert(p->macros[0].routes[15].curve==MacroCurve::Slow);ui->set_shift(false);assert(ui->pad(14,0x10,3500));assert((p->macros[0].routes[15].tracks&(1u<<2))==0);assert(ui->pad(14,0x20,0));assert(ui->undo());assert(p->macros[0].routes[15].tracks==0xffff);ui->set_shift(true);assert(ui->knob(7,1000));assert(p->macros[0].routes[15].low==1 && p->macros[0].routes[15].high==0);ui->set_shift(false);
 auto& macro2=p->macros[1];macro2.active=true;macro2.value=.5f;macro2.routes[15]={true,3,4,0,1,LayerMode::Replace,MacroCurve::Slow};auto value=resolve_parameter(*p,t,2,3,nullptr,0);assert(value.value>.249f && value.value<.251f);macro2.routes[15].curve=MacroCurve::Fast;value=resolve_parameter(*p,t,2,3,nullptr,0);assert(value.value>.749f && value.value<.751f);ParameterLock priority[kLocks]{};priority[0]={3,.1f,true};value=resolve_parameter(*p,t,2,3,priority,0);assert(value.value==.1f);macro2.routes[15].low=1;macro2.routes[15].high=0;value=resolve_parameter(*p,t,2,3,nullptr,0);assert(value.value>.249f && value.value<.251f);
 // Runtime common Morph endpoints, including the last live-FX declaration IDs.
 auto mp=std::make_unique<ProjectState>();make_empty_project(*mp);mp->morph_enabled=true;
 mp->morph_a[0].values[morph_index(472)]=1;mp->morph_b[0].values[morph_index(472)]=2;
 mp->morph_a[0].values[morph_index(473)]=.2f;mp->morph_b[0].values[morph_index(473)]=.8f;
 auto me=std::make_unique<Engine>(8000);assert(me->stage_project(*mp));assert(me->submit({CommandType::Start,0,0,0}));me->render(audio,64);assert(me->performance_fx_current()==PerformanceFx::Filter);
 Command move{CommandType::SetMorph,0,0,0};move.scalar=.75f;assert(me->submit(move));me->render(audio,64);assert(me->performance_fx_current()==PerformanceFx::Gate);
 puts("live FX locks, macro routing, curves and UI passed");
}
