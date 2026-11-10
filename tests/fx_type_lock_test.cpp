// Stage 5 S5-2: per-step FX type-switch locks for track inserts.
// The extension is deliberately narrow: a step lock on an insert type
// parameter (ids 94 / 98) switches that slot's effect type for the
// step's duration through the existing lock machinery, and the audio
// slot's fade-down / reset / fade-in switching keeps it clickless.
// No arbitrary FX graphs. This test pins the whole chain: UI creation
// on the Inserts page, engine application at the step boundary,
// return to the base type on the next step, and click-free switching
// inside TrackInsertSlot.
#include "performance.h"
#include "track_insert.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
using namespace groove;
int main() {
 // ---- Slot level: type changes never click ----
 {
  static float storage[TrackInsertSlot::kModFloats];
  TrackInsertSlot slot; slot.bind_modulation(storage); slot.init(48000);
  float worst = 0, prev_l = 0, prev_r = 0;
  auto run = [&](unsigned type, unsigned samples) {
   for (unsigned i = 0; i < samples; ++i) {
    float l = 0.5f, r = 0.5f;
    slot.process(l, r, type, 0.5f, 0.3f, 1.0f);
    float d = std::fmax(std::fabs(l - prev_l), std::fabs(r - prev_r));
    if (i && d > worst) worst = d;
    prev_l = l; prev_r = r;
   }
  };
  run(1, 4000);            // LOW PASS settles on DC
  run(5, 4000);            // switch to OVERDRIVE under signal
  run(9, 4000);            // switch to RING MOD under signal
  assert(worst < 0.05f);   // a hard switch would jump by ~0.5
  run(0, 4000);            // back to bypass: exactly dry again
  assert(std::fabs(prev_l - 0.5f) < 1e-4f && std::fabs(prev_r - 0.5f) < 1e-4f);
 }
 // ---- UI: Shift + TYPE knob on the Inserts page writes a type lock ----
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p);
  auto t = track_at(*p,0,0); t.insert_fx[0] = 1; // LOW PASS base
  Engine engine(8000); assert(engine.stage_project(*p));
  Performance perf(engine, *p);
  perf.set_mode(UiMode::Perform); assert(perf.soft_key(6)); // LIVE FX page: step chooser
  assert(perf.knob(5, 2000)); assert(perf.selected_step() == 2);
  perf.set_mode(UiMode::Effects); assert(perf.soft_key(3)); // Inserts page
  perf.set_shift(true);
  assert(perf.knob(0, 1000)); // FX1 TYPE 1 -> 2 as a step lock
  bool found = false;
  for (const auto& lock : t.steps[2].locks) if (lock.enabled && lock.id == 94) { found = true; assert(lock.value == 2); }
  assert(found && t.steps[2].lock_only && !t.steps[2].enabled);
  assert(t.insert_fx[0] == 1); // base value untouched by the lock edit
  perf.set_shift(false);
  assert(perf.clear_step_locks());
  for (const auto& lock : t.steps[2].locks) assert(!lock.enabled);
  assert(!t.steps[2].lock_only);
  assert(perf.undo()); // clearing is undoable, lock returns
  found = false;
  for (const auto& lock : t.steps[2].locks) if (lock.enabled && lock.id == 94 && lock.value == 2) found = true;
  assert(found && t.steps[2].lock_only);
 }
 // ---- Engine: the type switches for exactly the locked step ----
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p);
  auto t = track_at(*p,0,0);
  t.instrument = Instrument::PolySaw;
  t.insert_fx[0] = 1; t.insert_fx[1] = 0.7f; t.insert_fx[2] = 0.3f; t.insert_fx[3] = 1;
  t.steps[0].enabled = true; t.steps[0].note = 60;
  t.steps[4].lock_only = true; t.steps[4].locks[0] = {94, 5, true}; // OVERDRIVE at step 4 only
  Engine engine(48000); assert(engine.stage_project(*p));
  assert(engine.submit({CommandType::Start, 0, 0, 0}));
  float buf[512]; AudioVisualFrame frame;
  float first5 = -1, last5 = -1; bool saw_base_early = false, saw_base_late = false;
  for (unsigned i = 0; i < 400; ++i) {
   engine.render(buf, 256);
   while (engine.pop_visual(frame)) {
    float type = frame.track_insert[0][0];
    double at = double(frame.sample);
    if (type == 5) { if (first5 < 0) first5 = float(at); last5 = float(at); }
    if (type == 1 && at < 20000) saw_base_early = true;
    if (type == 1 && at > 42000) saw_base_late = true;
   }
  }
  // Step 4 spans samples 24000..30000 at 120 BPM / 48 kHz; frames land
  // every 2400 samples, so allow one frame of slack on both sides.
  assert(first5 >= 20000 && first5 <= 31000);
  assert(last5 >= 23000 && last5 <= 34000 && last5 - first5 < 12000);
  assert(saw_base_early && saw_base_late); // base type before and after
 }
 std::puts("fx type locks: UI creation, per-step engine switching and clickless slot verified");
}
