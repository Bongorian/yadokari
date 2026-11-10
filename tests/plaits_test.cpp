#include "plaits_voice.h"
#include "demo_project.h"
#include <memory>
#include "engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
 for(unsigned model=0;model<24;++model) {
  groove::PlaitsVoice voice; voice.init(); assert(voice.set_model(model)); voice.set_note(48); assert(voice.set_parameter(0,0.4f)); assert(!voice.set_parameter(5,0.5f)); voice.trigger(); float peak=0;
  for(unsigned i=0;i<24000;++i) { const float x=voice.next(); assert(std::isfinite(x) && x>=-1 && x<=1); if(std::fabs(x)>peak) peak=std::fabs(x); }
  assert(voice.active_model()==int(model)); assert(peak>0); std::printf("model %02u peak %.5f\n",model,peak);
 }
 uint8_t sysex[4104]{}; sysex[0]=0xf0; sysex[1]=0x43; sysex[3]=9; sysex[4]=0x20; sysex[4103]=0xf7; groove::Dx7BankView bank;
 assert(groove::parse_dx7_bulk(sysex,sizeof sysex,bank)); assert(bank.data==sysex+6); sysex[6]=1; assert(!groove::parse_dx7_bulk(sysex,sizeof sysex,bank)); sysex[4102]=127; assert(groove::parse_dx7_bulk(sysex,sizeof sysex,bank));
 groove::PlaitsVoice fm; fm.init(); assert(fm.set_dx7_bank(0,bank)); assert(fm.set_model(2)); fm.trigger(); for(unsigned i=0;i<2400;++i) assert(std::isfinite(fm.next())); assert(fm.active_model()==2);
 for(unsigned model=0;model<24;++model){groove::PlaitsVoice left,right;left.init();right.init();left.seed(12345);right.seed(12345);left.set_model(model);right.set_model(model);left.trigger();right.trigger();for(unsigned i=0;i<4096;++i)assert(left.next()==right.next());}std::puts("all Plaits models: interleaved fixed-seed output identical");
 groove::Engine unsupported(44100); assert(!unsupported.supports_engine(groove::Instrument::PlaitsMacro));
 groove::Engine engine; groove::Command bank_cmd{groove::CommandType::SetDx7Bank,0,0,0}; bank_cmd.bank=bank; assert(engine.submit(bank_cmd)); assert(engine.submit({groove::CommandType::SetInstrument,0,0,uint16_t(groove::Instrument::PlaitsMacro)})); groove::Command model{groove::CommandType::SetEngineParameter,0,0,0};model.parameter=5;model.scalar=20;assert(engine.submit(model)); assert(engine.submit({groove::CommandType::Trigger,0,0,100}));
 float out[24000]; engine.render(out,12000); bool audible=false; for(float x:out) { assert(std::isfinite(x)); audible|=x!=0; } assert(audible);
 // A silent second Plaits track must not steal the first track's voice.
 auto project=std::make_unique<groove::ProjectState>();groove::make_empty_project(*project);
 track_at(*project,0,0).instrument=groove::Instrument::PlaitsMacro;track_at(*project,0,1).instrument=groove::Instrument::PlaitsMacro;track_at(*project,0,1).level=0;project->muted_tracks=2;
 auto a=std::make_unique<groove::Engine>();auto b=std::make_unique<groove::Engine>();assert(a->stage_project(*project)&&b->stage_project(*project));float empty[2];a->render(empty,0);b->render(empty,0);
 assert(a->submit({groove::CommandType::Trigger,0,0,100})&&b->submit({groove::CommandType::Trigger,0,0,100}));float left[512],right[512],warm[8192];a->render(warm,4096);b->render(warm,4096);
 assert(b->submit({groove::CommandType::Trigger,1,0,100}));a->render(left,256);b->render(right,256);
 bool heard=false;for(unsigned i=0;i<512;++i){assert(std::isfinite(left[i])&&std::isfinite(right[i]));heard|=left[i]!=0;assert(std::abs(left[i]-right[i])<0.00001f);}assert(heard);

}
