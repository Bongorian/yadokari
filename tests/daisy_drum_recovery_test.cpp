// Only this test translation unit has private access, to inject a latched
// non-finite generator state without a firmware test hook.
#include "engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
using namespace groove;
int main(){
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 for(float rate:{nan,inf,-inf,0.0f}){
  DaisyDrumBank bank{};bank.init(rate);
  float p[]={nan,0.5f,0.5f,0.6f,1};
  bank.trigger(0,p,1,65.4f);
  bank.analog_kick_.tone_lp_=inf;
  assert(bank.next(UINT64_MAX)==0 && bank.recoveries()==1);
  assert(bank.next(UINT64_MAX)==0 && bank.recoveries()==1);
  assert(std::isfinite(bank.next(0)) && bank.recoveries()==1);
 }
 auto engine=std::make_unique<Engine>();
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 project->tracks[0].instrument=Instrument::DaisyAnalogDrums;
 project->tracks[0].engine_parameters[0]=0; // inject into the selected kick, not the default snare
 assert(engine->stage_project(*project));
 float master[128];engine->render(master,0);
 assert(engine->submit({CommandType::Trigger,0,0,127}));
 engine->render(master,64);
 auto& bank=engine->public_drums_[0];
 assert(bank.kit_==0 && bank.voice_==0);
 bank.analog_kick_.tone_lp_=nan;
 engine->render(master,64);
 for(float x:master)assert(std::isfinite(x));
 assert(bank.recoveries()==1);
 assert(engine->submit({CommandType::Trigger,0,0,127}));
 double energy=0;
 for(unsigned b=0;b<64;++b){engine->render(master,64);for(float x:master){assert(std::isfinite(x));energy+=double(x)*x;}}
 assert(bank.recoveries()==1 && energy>0.001);
 std::puts("PASS Daisy latched filter recovery, finite stereo master and audible retrigger");
}
