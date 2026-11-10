#include "benchmark_project.h"
#include <cassert>
#include <memory>
using namespace groove;
int main(){
 auto project=std::make_unique<ProjectState>();make_fx_stress_project(*project);assert(valid_project(*project));
 assert(project->morph_enabled && project->tempo==140);
 for(unsigned t=0;t<16;++t){const auto track=track_at(*project,0,t);assert(track.instrument==(t<8?Instrument::DrumKit:Instrument::PlaitsMacro));assert(track.insert_fx[0]==(t<8?1.0f:float(t<13?t+2:t+3)) && track.insert_fx[4]==5);
  if(t<8)for(unsigned pad=0;pad<4;++pad){assert(track.kit.pads[pad].sample[0]==0);assert(track.kit.pads[pad].insert_fx[0]>0);assert(track.kit.steps[pad][pad].enabled && track.kit.steps[pad][pad].fx_lock[1]!=255);}}
}
