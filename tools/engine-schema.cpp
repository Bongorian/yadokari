#include "engine_schema.h"
#include "project_format_version.h"
#include <cstdio>
using namespace groove;
static void string(const char* s){std::putchar('"');for(;*s;++s){if(*s=='"' || *s=='\\')std::putchar('\\');std::putchar(*s);}std::putchar('"');}
int main(){
 std::printf("{\"schema_version\":2,\"container_version\":%u,\"layer_chunk_version\":2,\"layer_max_parts\":4,\"layer_note_cost_max\":12,\"track_lfo\":{\"sync_id\":11,\"division_id\":12,\"hz_id\":8,\"depth_id\":7,\"divisions\":[\"2/1\",\"1/1\",\"1/2.\",\"1/2\",\"1/2T\",\"1/4.\",\"1/4\",\"1/4T\",\"1/8.\",\"1/8\",\"1/8T\",\"1/16\"]},\"engines\":[\n",kFormatVersion);
 for(unsigned e=0;e<instrument_count();++e){const auto& d=*engine_descriptor(e);if(e)std::putchar(',');std::printf("{\"guid\":");string(d.identity.guid);std::printf(",\"name\":");string(d.instrument.name);std::printf(",\"cpu_class\":%u,\"voice_cost\":%u,\"layer_leaf\":%s,\"layer_exclusion\":",d.instrument.cpu_class,engine_voice_cost(Instrument(e),0),layer_leaf_supported(Instrument(e))?"true":"false");const char* reason=layer_leaf_exclusion(Instrument(e));if(reason)string(reason);else std::printf("null");std::printf(",\"parameters\":[");
  for(unsigned n=0;n<engine_schema_count(Instrument(e));++n){auto p=engine_schema_parameter(Instrument(e),n);if(n)std::putchar(',');std::printf("{\"scope\":\"%s\",\"index\":%u,\"name\":",p.reference.scope==EngineParameterScope::Engine?"engine":"track",p.reference.index);string(p.value->name);std::printf(",\"tier\":\"%s\",\"type\":\"%s\",\"group\":",p.ui.tier==EngineEditTier::Performance?"performance":"detail",p.ui.type==EngineParameterType::Float?"float":p.ui.type==EngineParameterType::Integer?"integer":p.ui.type==EngineParameterType::Boolean?"boolean":"choice");string(p.ui.group);std::printf(",\"minimum\":%.9g,\"maximum\":%.9g,\"default\":%.9g,\"step\":%.9g,\"unit\":",p.value->minimum,p.value->maximum,p.value->initial,p.value->step);string(p.value->unit);std::printf(",\"interpolation\":%u,\"display\":%u,\"apply_timing\":%u,\"choices\":[",unsigned(p.value->interpolation),unsigned(p.display),unsigned(p.value->apply_timing));for(unsigned c=0;c<p.choice_count;++c){if(c)std::putchar(',');string(p.choices[c]);}std::printf("]}");}
  std::printf("]}");
 }
 std::puts("]}");
}
