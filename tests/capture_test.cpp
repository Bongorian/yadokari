// Stage 4 S4-4: retrospective MIDI capture. The engine always
// retains the last two bars (32 steps) of played notes and kit
// hits in a ring, armed or not. CAPTURE (Perform gesture page,
// Shift+RECORD) replays the ring as ordinary RecordedNote /
// RecordedKitHit events, so the existing Performance conversion
// turns them into undoable step edits without disturbing
// playback; the ring is consumed by the capture. The quantise
// choice (Shift+PLAY LOOP toggles CAP Q) decides whether the
// replayed events carry their microtiming or land on the grid.
#include "performance.h"
#include <cassert>
#include <cstdio>
using namespace groove;
namespace {
unsigned enabled_steps(const ProjectState& p, unsigned track) {
 unsigned n = 0;
 for (const auto& s : track_at(p,0,track).steps) if (s.enabled) ++n;
 return n;
}
} // namespace
int main() {
 Engine engine(8000);
 ProjectState project; make_empty_project(project);project.tracks[0].instrument=Instrument::PolySaw;assert(engine.stage_project(project));
 Performance perf(engine, project);
 float buf[512];
 auto render = [&](int n) { for (int i = 0; i < n; ++i) { engine.render(buf, 256); perf.poll(); } };
 assert(perf.play());
 render(48); // transport running, ~12 steps in
 assert(enabled_steps(project, 0) == 0);
 // Play a note from the pads in Keyboard mode (never armed).
 perf.set_mode(UiMode::Keyboard);
 assert(perf.pad(5, 0x10, 3000));
 render(4);
 assert(perf.pad(5, 0x20, 0));
 render(4);
 // Nothing is written until CAPTURE is pressed.
 assert(enabled_steps(project, 0) == 0);
 assert(perf.capture_quantized());
 assert(perf.capture_retro());
 render(8);
 assert(enabled_steps(project, 0) == 1);
 const StepState* hit = nullptr;
 for (const auto& s : track_at(project,0,0).steps) if (s.enabled) hit = &s;
 assert(hit && hit->velocity > 0 && hit->note <= 127);
 assert(hit->microtiming == 0); // quantised capture lands on grid
 std::printf("capture wrote step note=%u vel=%u\n", hit->note, hit->velocity);
 // The ring is consumed: a second capture adds nothing.
 assert(perf.capture_retro());
 render(8);
 assert(enabled_steps(project, 0) == 1);
 // The capture is an ordinary undoable edit.
 assert(perf.undo());
 render(2);
 assert(enabled_steps(project, 0) == 0);
 std::printf("capture undo ok\n");
 // Unquantised capture also lands (microtiming preserved by the
 // shared conversion; exact offset depends on play timing).
 assert(perf.capture_toggle_quantize());
 assert(!perf.capture_quantized());
 assert(perf.pad(7, 0x10, 3000));
 render(3);
 assert(perf.pad(7, 0x20, 0));
 render(3);
 assert(perf.capture_retro());
 render(8);
 assert(enabled_steps(project, 0) == 1);
 std::printf("unquantised capture ok\n");
 std::printf("capture test passed\n");
 return 0;
}
