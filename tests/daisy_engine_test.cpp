#include "engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
int main(){
 for(unsigned kit=0;kit<2;++kit)for(unsigned voice=0;voice<3;++voice){
  auto project=std::make_unique<ProjectState>();make_empty_project(*project);
  auto track=track_at(*project,0,0);track.instrument=kit?Instrument::DaisySyntheticDrums:Instrument::DaisyAnalogDrums;
  track.length=16;track.root_note=voice==0?48:60;track.engine_parameters[0]=float(uint16_t((float(voice)/3+0.05f)*16383))/16383.0f;track.steps[0].enabled=true;track.steps[0].velocity=110;
  auto engine=std::make_unique<Engine>(48000);assert(engine->stage_project(*project));assert(engine->submit({CommandType::Start,0,0,0}));
  std::vector<float> out(512);float peak=0;double energy=0;
  for(unsigned n=0;n<12;++n){engine->render(out.data(),256);for(float x:out){assert(std::isfinite(x));peak=std::fmax(peak,std::fabs(x));energy+=double(x)*x;}}
  if(peak<0.00001f||energy<0.00001)return 1;
  std::printf("engine kit=%u voice=%u peak=%.3f energy=%.3f\n",kit,voice,peak,energy);
 }
}
