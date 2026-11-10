// O-01 test (ENGINE_OPTIMIZATION): the Plaits silence gate engages
// after consecutive silent render quanta, returns exact zero while
// gated without rendering, ignores unchanged parameter re-sends
// (the engine refresh pattern), and wakes on real changes and on
// trigger, producing audible output again.
#include "plaits_voice.h"
#include <cassert>
#include <cstdio>
using namespace groove;
static void run_peak(PlaitsVoice& v, unsigned n, float& peak) {
 peak = 0;
 for (unsigned i = 0; i < n; ++i) { float s = v.next(); float a = s < 0 ? -s : s; if (a > peak) peak = a; }
}
static void wait_gated(PlaitsVoice& v, unsigned max_samples) {
 unsigned waited = 0;
 while (!v.gated() && waited < max_samples) { v.next(); ++waited; }
 assert(v.gated());
}
int main() {
 PlaitsVoice v; v.init(); v.seed(12345);
 assert(v.set_model(0));
 v.set_parameter(3, 0.0f); // shortest decay
 // Untriggered: the voice must fall silent and gate.
 wait_gated(v, 48000 * 3);
 // While gated, output is exactly zero.
 float peak = 1; run_peak(v, 4800, peak); assert(peak == 0.0f);
 // An unchanged parameter re-send must not wake it (refresh pattern).
 v.set_parameter(1, 0.5f); assert(v.gated());
 // A real parameter change wakes it; it re-gates once silent again.
 v.set_parameter(1, 0.75f); assert(!v.gated());
 wait_gated(v, 48000);
 // A note change wakes it too.
 v.set_note(64); assert(!v.gated());
 wait_gated(v, 48000);
 // Trigger wakes it and audible output comes back.
 v.trigger(100); assert(!v.gated());
 run_peak(v, 2400, peak); assert(peak > 0.01f);
 std::printf("plaits silence gate (O-01) passed\n");
 return 0;
}
