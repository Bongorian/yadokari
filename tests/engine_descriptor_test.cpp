// SPEC 2.8 descriptor consistency, extended per the Stage 1
// acceptance list:
//  - every selectable engine has a complete EngineDescriptor
//    (name, family, cpu class, envelope kind) and a GUID entry
//    marked implemented, with unique GUID strings;
//  - the Unknown engine has no descriptor (the UI must fall back
//    to the UNKNOWN ENGINE display, never to editable controls);
//  - macro ids stay inside the scope each engine declares (S-02);
//  - every registered ParameterDescriptor is actually consumed:
//    the resolver resolves it (finite base) and a step lock on it
//    takes effect through resolve_parameter - a descriptor the
//    engine cannot consume would be a dead control (rule 1/2);
//  - apply_timing is honest for the sampler region (W-01):
//    START/END are NEXT_TRIGGER, SPEED stays LIVE.
#include "project.h"
#include "parameters.h"
#include "parameter_resolver.h"
#include "instrument_registry.h"
#include "engine_guids.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace groove;
int main() {
 // Engine descriptors + GUIDs.
 const char* guids[unsigned(Instrument::Unknown)];
 for (unsigned i = 0; i < instrument_count(); ++i) {
  auto instrument = Instrument(i);
  const auto* d = instrument_descriptor(i);
  assert(d != nullptr);
  assert(d->name && d->name[0]);
  assert(d->family && d->family[0]);
  assert((instrument==Instrument::Empty?d->cpu_class==0:d->cpu_class>=1) && d->cpu_class <= 3);
  assert(d->envelope <= EnvelopeEditable);
  assert(d->macro_controls <= 5);
  const auto* g = engine_guid_info(instrument);
  assert(g != nullptr && g->implemented && g->guid && g->guid[0]);
  const auto* engine=engine_descriptor(i);
  assert(&engine->instrument==d && &engine->identity==g);
  for(unsigned local=0;local<engine->parameters.count;++local){
   const auto& p=engine->parameters.declarations[local];
   assert(p.value.id==local && p.value.name && p.value.name[0]);
   assert(p.value.minimum<=p.value.initial && p.value.initial<=p.value.maximum);
   if(p.legacy_id)assert(engine_local_parameter(instrument,p.legacy_id)==int(local));
   else assert(p.target!=EngineValueTarget::None);
   // MODEL has no pseudo global parameter; the remaining bridge is retired with target references.
   if(p.legacy_id){const auto* legacy=parameter_descriptor(p.legacy_id);assert(legacy);assert(p.value.minimum==legacy->minimum && p.value.maximum==legacy->maximum && p.value.initial==legacy->initial && p.value.interpolation==legacy->interpolation && p.value.apply_timing==legacy->apply_timing);}
  }
  assert(engine_parameter(instrument,engine->parameters.count)==nullptr);
  for(unsigned slot=0;slot<8;++slot){const auto ref=engine_core_reference(instrument,slot);if(ref.scope==EngineParameterScope::Engine)assert(engine_parameter(instrument,ref.index));else if(ref.scope==EngineParameterScope::Track)assert(parameter_descriptor(ref.index));}
  assert(engine->bind);
  float resolved[kEngineMorphParameters]{};assert(engine->parameters.count<=kEngineMorphParameters);for(unsigned n=0;n<engine->parameters.count;++n)resolved[n]=engine->parameters.declarations[n].value.initial;
  float macros[5]={-1,-1,-1,-1,-1},envelope[4]{},native[kEngineMorphParameters]{};native[12]=42;uint8_t model=255;SampleEdit sample;
  engine->bind(engine->parameters,resolved,engine->parameters.count,{macros,&model,&sample,envelope,native});
  if(instrument==Instrument::PlaitsMacro){assert(model==0);for(float value:macros)assert(value==.5f);}
  if(instrument==Instrument::FM)assert(macros[0]==.5f && macros[1]==.5f && macros[2]==-1);
  if(instrument==Instrument::Sampler)assert(sample.speed==1 && sample.start==0 && sample.end==1 && sample.attack_ms==0);
  if(instrument==Instrument::Texture)assert(native[0]==0 && native[1]==.5f && native[12]==42); // Track INPUT was not overwritten.

  for(unsigned n=0;n<engine->state.count;++n){const auto& c=engine->state.chunks[n];assert(c.fourcc && c.version && c.emit && c.encode && c.decode && c.defaults);}
  if(instrument==Instrument::Texture)assert(engine_local_parameter(instrument,632)==-1); // INPUT belongs to the track.
  if(instrument==Instrument::PlaitsMacro){const auto* model=engine_parameter(instrument,5);assert(model && model->choice_count==24);for(unsigned m=0;m<24;++m)assert(model->choices[m][0]);}
  guids[i] = g->guid;
  for (unsigned j = 0; j < i; ++j) assert(std::strcmp(guids[j], guids[i]) != 0);
 }
 assert(instrument_descriptor(unsigned(Instrument::Unknown)) == nullptr);
 Instrument old;assert(!engine_from_guid("mk3.plaits-osc",14,old));assert(!engine_from_guid("mk3.plaits-macro",16,old));assert(engine_from_guid("mk3.plaits",10,old) && old==Instrument::PlaitsMacro);
 assert(engine_voice_cost(Instrument::PlaitsMacro,2)==2 && instrument_voice_limit(Instrument::PlaitsMacro,2)==2);
 // Parameter descriptors are consumed by the resolver.
 ProjectState project; make_empty_project(project);
 ConstTrackView track = track_at(project,0,0);
 unsigned registered = 0;
 for (uint32_t id = 1; id < 1024; ++id) {
  const auto* d = parameter_descriptor(uint16_t(id));
  if (!d || !morph_common_parameter(uint16_t(id))) continue;
  ++registered;
  assert(d->name && d->name[0]);
  assert(d->minimum <= d->initial && d->initial <= d->maximum);
  float base = parameter_base(project, track, uint16_t(id));
  assert(std::isfinite(base));
  // A lock on this id must change the resolved value (unless the
  // base already equals the lock probe value).
  ParameterLock locks[kLocks]{};
  float probe = d->initial + (d->maximum - d->minimum) * 0.25f;
  if (probe > d->maximum) probe = d->maximum;
  locks[0] = ParameterLock{uint16_t(id), probe, true};
  auto resolved = resolve_parameter(project, track, 0, uint16_t(id), locks, 0);
  assert(resolved.sources & FromLock);
  assert(std::fabs(resolved.value - clamp_parameter(uint16_t(id), probe)) < 1e-6f);
 }
 for(unsigned e=0;e<instrument_count();++e){project.tracks[0].instrument=Instrument(e);const auto* descriptor=engine_descriptor(e);for(unsigned local=0;local<descriptor->parameters.count;++local){auto target=engine_target(Instrument(e),local);const auto* d=target_descriptor(target);float probe=clamp_target(target,d->initial+(d->maximum-d->minimum)*.25f);ParameterLock locks[kLocks]={{target,probe,true}};auto resolved=resolve_target_value(project,track,0,target,engine_parameter_value(track,local),locks,0);assert((resolved.sources&FromLock) && resolved.value==probe);}}
 // Direct declaration storage round trips without the global numeric adapter.
 for(unsigned e=0;e<instrument_count();++e){auto edit=track_at(project,0,0);edit.instrument=Instrument(e);const auto* descriptor=engine_descriptor(e);
  for(unsigned local=0;local<descriptor->parameters.count;++local){const auto& d=descriptor->parameters.declarations[local];float probe=clamp_target(engine_target(Instrument(e),local),d.value.initial+(d.value.maximum-d.value.minimum)*.25f);
   if(d.target==EngineValueTarget::Sample){edit.sample_edit.start=0;edit.sample_edit.end=1;}
   assert(set_engine_parameter(edit,local,probe));assert(engine_parameter_value(edit,local)==probe);
   assert(!set_engine_parameter(edit,local,d.value.maximum+1));assert(engine_parameter_value(edit,local)==probe);
  }
 }
 auto sampler=track_at(project,0,0);sampler.instrument=Instrument::Sampler;sampler.sample_edit.start=.2f;sampler.sample_edit.end=.8f;
 assert(!set_engine_parameter(sampler,1,.8f) && !set_engine_parameter(sampler,2,.2f));
 assert(registered > 100);
 // apply_timing honesty for the sampler region.
 assert(parameter_descriptor(80)->apply_timing == ApplyLive);        // SPEED
 assert(parameter_descriptor(81)->apply_timing == ApplyNextTrigger); // START
 assert(parameter_descriptor(82)->apply_timing == ApplyNextTrigger); // END
 assert(parameter_descriptor(85)->apply_timing == ApplyLive);        // ATTACK
 std::printf("engine descriptors (%u engines, %u parameters consumed) passed\n", instrument_count(), registered);
 return 0;
}
