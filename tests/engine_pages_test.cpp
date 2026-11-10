// SCR-EDT-02 page descriptors (SPEC 2.8): every engine declares a
// hero visual, a SHAPE kind consistent with its envelope kind, and
// rank-ordered core parameters - and every core parameter must be
// consumed by the resolver (a step lock on it takes effect), so the
// CORE page can never present a dead knob. The knob-band wiring is
// exercised too: on the CORE view, knobs 2..6 follow the descriptor
// rank order wherever the macro path does not already serve them.
#include "performance.h"
#include "engine_pages.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace groove;
int main() {
 ProjectState project; make_empty_project(project);
 ConstTrackView track = track_at(project,0,0);
 for (unsigned i = 0; i < instrument_count(); ++i) {
  project.tracks[0].instrument=Instrument(i);
  const auto* d = instrument_descriptor(i);
  const auto* page = engine_page_descriptor(i);
  assert(d != nullptr && page != nullptr);
  // A hero visual for every engine except DrumKit, whose editing UI
  // lives on the Pad/Sampler pages.
  assert(page->hero != HeroVisual::None || i == unsigned(Instrument::DrumKit) || i == unsigned(Instrument::Empty));
  // SHAPE kind follows the declared envelope kind.
  if (d->envelope == EnvelopeEditable) assert(page->shape == ShapeKind::Envelope);
  if (d->envelope == EnvelopeFixed) assert(page->shape != ShapeKind::None && page->shape != ShapeKind::Envelope);
  if (d->envelope == EnvelopeNone) assert(page->shape == ShapeKind::None);
  // Core parameters: registered, unique, and consumed.
  for (unsigned a = 0; a < 8; ++a) {
   uint16_t id = engine_legacy_reference(Instrument(i),page->core[a]);
   if (!id) break;
   const auto* pd = parameter_descriptor(id);
   assert(pd != nullptr);
   for (unsigned b = 0; b < a; ++b) assert(engine_legacy_reference(Instrument(i),page->core[b]) != id);
   float base = parameter_base(project, track, id);
   assert(std::isfinite(base));
   ParameterLock locks[kLocks]{};
   float probe = pd->initial + (pd->maximum - pd->minimum) * 0.25f;
   if (probe > pd->maximum) probe = pd->maximum;
   locks[0] = ParameterLock{bind_parameter_target(Instrument(i),id), probe, true};
   auto resolved = resolve_parameter(project, track, 0, id, locks, 0);
   assert(resolved.sources & FromLock);
   assert(std::fabs(resolved.value - clamp_parameter(id, probe)) < 1e-6f);
  }
  // Macro engines declare their five controls in rank order; FM
  // declares ratio/index first, then track parameters.
  if (d->macro_controls == 5) {
   for (unsigned k = 0; k < 5; ++k){unsigned at=i==unsigned(Instrument::PlaitsMacro)?k+1:k;assert(page->core[at].scope==EngineParameterScope::Engine && page->core[at].index==k);}
  }
  if (i == unsigned(Instrument::FM)) {
   assert(page->core[0].scope==EngineParameterScope::Engine && page->core[0].index==0 && page->core[1].scope==EngineParameterScope::Engine && page->core[1].index==1);
   assert(page->core[2].scope==EngineParameterScope::Track && page->core[2].index==3 && page->core[3].index==4 && page->core[4].index==1);
  }
 }
 assert(engine_page_descriptor(unsigned(Instrument::Unknown)) == nullptr);
 // Knob-band wiring on the CORE view.
 Engine engine(48000);
 ProjectState live; make_empty_project(live);
 Performance perf(engine, live);
 perf.set_mode(UiMode::Synth);
 perf.set_instrument(Instrument::PolySaw);
 assert(perf.core_parameter(0) == 3); // rank 1: CUTOFF
 assert(perf.core_parameter(2) == 1); // rank 3: LEVEL
 assert(perf.knob_parameter(0) == 3);
 float before = perf.editing_parameter(3);
 perf.knob(0, -2000); // CUTOFF starts at its maximum; turn down
 assert(std::fabs(perf.editing_parameter(3) - before) > 1e-6f);
 perf.set_instrument(Instrument::FM);
 assert(perf.core_parameter(2) == 3); // FM rank 3 lands on knob 4
 assert(perf.core_parameter(0) == engine_target(Instrument::FM,0)); // ratio uses its local declaration
 assert(perf.set_parameter(32,0.1234567f));float scratch[2];engine.render(scratch,0);
 assert(track_at(live,0,0).engine_parameters[0]==0.1234567f);
 assert(perf.undo());engine.render(scratch,0);assert(track_at(live,0,0).engine_parameters[0]==0.5f);
 assert(perf.redo());engine.render(scratch,0);assert(track_at(live,0,0).engine_parameters[0]==0.1234567f);
 perf.set_instrument(Instrument::Sampler);
 assert(perf.core_parameter(0) == engine_target(Instrument::Sampler,0)); // SPEED
 assert(perf.core_parameter(1) == engine_target(Instrument::Sampler,1)); // START (NEXT_TRIGGER)
 // Native/SHAPE controls publish scoped references and edit the same storage.
 perf.set_instrument(Instrument::Texture);assert(perf.core_parameter(0)==engine_target(Instrument::Texture,0));
 assert(perf.knob(0,1000));engine.render(scratch,0);assert(track_at(live,0,0).texture.values[0]==1);
 assert(perf.soft_key(2));assert(perf.knob_parameter(0)==engine_target(Instrument::Texture,6));
 assert(perf.soft_key(3));assert(perf.knob_parameter(0)==632 && perf.knob_parameter(1)==engine_target(Instrument::Texture,11));
 perf.set_instrument(Instrument::Resonator);assert(perf.soft_key(2));assert(perf.knob_parameter(0)==engine_target(Instrument::Resonator,1));
 perf.set_instrument(Instrument::DaisySynth);assert(perf.soft_key(2));assert(perf.knob_parameter(2)==engine_target(Instrument::DaisySynth,0));
 float attack=perf.knob_value(2);assert(perf.knob(2,1000));engine.render(scratch,0);assert(perf.knob_value(2)>attack);
 // SHAPE page existence follows the page descriptor: engines with
 // ShapeKind::None offer no ENVELOPE soft key at all.
 perf.set_instrument(Instrument::DaisyAnalogDrums);
 assert(!perf.soft_enabled(2));
 perf.set_instrument(Instrument::PolySaw);
 assert(perf.soft_enabled(2)); // FIXED shape page
 perf.set_instrument(Instrument::WaveSequence);
 assert(!perf.soft_enabled(2));
 std::printf("engine pages (heroes, shape kinds, rank cores consumed) passed\n");
 return 0;
}
