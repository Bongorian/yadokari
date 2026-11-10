#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
using namespace groove;
int main(){
 auto project=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>(48000);
 auto reference=std::make_unique<ResonatorDsp>();float main[48]{},l[24]{},r[24]{},input[24]{};
 double maximum=0;
 for(unsigned model=0;model<3;++model)for(unsigned pitch=36;pitch<=84;pitch+=12){
  make_empty_project(*project);project->random_seed=0x12345678;
  auto track=track_at(*project,0,0);track.instrument=Instrument::Resonator;
  track.resonator.values[0]=float(model);track.resonator.values[1]=2;
  assert(engine->stage_project(*project));engine->render(main,0);
  assert(engine->start_live_record(false,2400,19)); // Track 1 dry, pre mixer/FX
  Command on{CommandType::MidiInput,0,uint8_t(pitch),0x90};on.scalar=100;assert(engine->submit(on));
  reference->init(project->random_seed);
  for(unsigned frame=0;frame<2400;frame+=24){
   rings::Patch patch{.5f,.5f,.5f,.5f};rings::PerformanceState state{};
   state.internal_exciter=true;state.tonic=12;state.note=float(pitch)-12;state.strum=frame==0;
   reference->process(model,4,patch,state,input,l,r);
   engine->render(main,24);
   for(unsigned i=0;i<24;++i){LiveRecordFrame received;assert(engine->pop_live_record(received));
    double error=fmax(fabs(double(received.l)-l[i]),fabs(double(received.r)-r[i]));if(error>maximum)maximum=error;
   }
  }
  assert(engine->live_record_done() && !engine->live_record_failed());engine->finish_live_record();
 }
 std::printf("Resonator Engine dry capture vs native adapter: 15 cases max error %.9g\n",maximum);
 assert(maximum==0);
 // All eight native parameter IDs follow the same lock/automation/macro chain.
 for(unsigned id=600;id<608;++id){auto t=track_at(*project,0,0);const auto* d=parameter_descriptor(id);assert(set_track_parameter(t,id,d->maximum));assert(track_parameter(t,id)==d->maximum);assert(parameter_at(parameter_index(id))==id);}
 Performance perf(*engine,*project);perf.set_mode(UiMode::Synth);
 assert(perf.set_instrument(Instrument::Resonator));assert(perf.core_parameter(0)==engine_target(Instrument::Resonator,0));
 assert(perf.soft_key(2));assert(perf.core_parameter(0)==engine_target(Instrument::Resonator,1) && perf.core_parameter(2)==engine_target(Instrument::Resonator,7));
 assert(perf.knob_parameter(3)==0 && !perf.knob(3,1000));
 assert(perf.knob(0,-1000));assert(track_at(*project,0,0).resonator.values[1]==1);
}
