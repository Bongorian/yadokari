#include "dsp_probe_workload.h"
#include <cstdio>

int main() {
 using namespace groove;
 std::printf("{\"revision\":%u,\"stress_start\":%u,\"fx_count\":%u,\"cases\":[",
             kWave4ProbeRevision,dsp_stress_start(),unsigned(kFxCount));
 for(unsigned i=0;i<dsp_case_count();++i){
  const auto c=dsp_condition(i);
  const auto* d=engine_descriptor(unsigned(c.engine));
  unsigned cost=engine_voice_cost(c.engine,c.variant);
  if(c.kind==DspCondition::Choir)cost=c.variant*engine_voice_cost(c.engine,0);
  if(c.kind==DspCondition::Insert)cost=fx_descriptor(c.variant)->cost;
  unsigned native_avg=0,native_burst=0;
  if(d->runtime_cost.values && c.variant<d->runtime_cost.count){
   const auto v=d->runtime_cost.values[c.variant];
   native_avg=v.average_us;native_burst=v.burst_us;
  }
  std::printf("%s{\"case\":%u,\"kind\":%u,\"engine\":%u,\"name\":\"%s\","
              "\"variant\":%u,\"tracks\":%u,\"notes\":%u,\"second_inserts\":%u,\"declared_cost\":%u,"
              "\"independent\":%s,\"native_avg_us\":%u,\"native_burst_us\":%u}",
              i?",":"",i,unsigned(c.kind),unsigned(c.engine),d->instrument.name,
              c.variant,c.tracks,c.notes,c.second_inserts,cost,d->polyphony.independent_voices?"true":"false",
              native_avg,native_burst);
 }
 std::puts("]}");
}
