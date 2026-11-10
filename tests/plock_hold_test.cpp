// SPEC 2.6 second lock path. In Step view the knob band normally
// edits step fields and the instrument macros are not mapped;
// while a step pad is held, knobs 2..6 present the track's macro
// parameters and turning one writes a parameter lock for the held
// (== selected) step - the same lock Shift + knob writes on the
// edit pages - without touching the global parameter value. On
// release the step-field knobs return. For DrumKit steps the
// field locks (level/delay/reverb/...) follow the same rule.
#include "performance.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace groove;
static bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 perf.set_instrument(Instrument::PlaitsMacro);
 perf.set_mode(UiMode::Step);
 // Not mapped in Step view while nothing is held.
 assert(perf.knob_parameter(2) == 0);
 // Hold step pad: hardware 9 is logical pad 5 on page 0, and
 // pressing it selects step 5.
 assert(perf.pad(9, 0x10, 800));
 uint16_t id = perf.knob_parameter(2);
 assert(id == 32);
 const auto target=engine_target(Instrument::PlaitsMacro,0); // first macro slot (32 + index - 2)
 assert(!perf.step_locked(target));
 float global_before = perf.editing_parameter(id);
 float kv_before = perf.knob_value(2);
 // Turn the knob: a lock appears on step 5 and the knob reads
 // the lock value back.
 perf.knob(2, 6000);
 assert(perf.step_locked(target));
 float lock_value = perf.knob_value(2);
 assert(!near(lock_value, kv_before) || !near(global_before, kv_before));
 // The global value is untouched: move the selection to step 4
 // (no lock there, pad released) and the parameter reads back
 // exactly the pre-hold global value.
 assert(perf.pad(9, 0x20, 0));
 assert(perf.pad(8, 0x10, 800));
 assert(perf.pad(8, 0x20, 0));
 assert(near(perf.editing_parameter(id), global_before));
 // Back on step 5 the lock is still there; after release the
 // macro mapping is gone again.
 assert(perf.pad(9, 0x10, 800));
 assert(perf.step_locked(target));
 assert(perf.pad(9, 0x20, 0));
 assert(perf.knob_parameter(2) == 0);
 assert(perf.step_locked(target));
 std::printf("p-lock hold path (pad-held knob writes the step lock) passed\n");
 return 0;
}
