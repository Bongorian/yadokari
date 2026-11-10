#include "engine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
int main(){
 using namespace groove;using Clock=std::chrono::steady_clock;
 for(unsigned n:{1u,4u,8u,16u}){
  auto project=std::make_unique<ProjectState>();make_empty_project(*project);
  for(unsigned t=0;t<n;++t){auto track=track_at(*project,0,t);track.instrument=Instrument::PlaitsMacro;track.plaits_model=uint8_t(t%24);track.level=0.03f;}
  auto engine=std::make_unique<Engine>();if(!engine->stage_project(*project))return 1;float out[12]{};engine->render(out,0);
  for(unsigned t=0;t<n;++t)if(!engine->submit({CommandType::Trigger,uint8_t(t),0,100}))return 2;
  std::vector<unsigned> times;times.reserve(8000);
  for(unsigned i=0;i<8000;++i){auto start=Clock::now();engine->render(out,6);auto end=Clock::now();times.push_back(unsigned(std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()));}
  std::sort(times.begin(),times.end());unsigned over125=0;for(auto t:times)if(t>125000)++over125;
  std::printf("tracks=%u ns_p50=%u ns_p99=%u ns_max=%u over125us=%u/8000\n",n,times[4000],times[7920],times.back(),over125);
 }
}
