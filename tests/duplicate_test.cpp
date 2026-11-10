// Unit 20b: DUPLICATE (duplicate step/pattern), Shift+DUPLICATE
// (double pattern), ERASE+knob parameter data erase.
#include "performance.h"
#include <cassert>
#include <cstdio>
using namespace groove;
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 assert(engine.stage_project(project));
 auto settle = [&]() { float buf[1024]{}; engine.render(buf, 512); perf.poll(); };
 project.active_pattern = 0;
 auto track = track_at(project,0,0);
 track.length = 16;
 track.steps[0].enabled = 1; track.steps[0].note = 60; track.steps[0].velocity = 100;
 track.steps[3].enabled = 1; track.steps[3].note = 62;
 track.steps[3].locks[0] = {3, 0.5f, true};
 track.automation[0].enabled = true; track.automation[0].parameter = 3;
 track.automation[0].length = 16;
 for (unsigned s = 0; s < 16; ++s) track.automation[0].values[s] = float(s) / 16.0f;
 perf.set_mode(UiMode::Step);
 // duplicate_step: selection advances onto the copy.
 assert(perf.selected_step() == 0);
 assert(perf.duplicate_step());
 assert(perf.selected_step() == 1);
 const auto& copied = track_at(project,0,0).steps[1];
 assert(copied.enabled && copied.note == 60 && copied.velocity == 100);
 // duplicate_pattern: lands on the next empty pattern, selected.
 assert(perf.duplicate_pattern());
 assert(project.active_pattern == 1);
 assert(track_at(project,1,0).steps[3].enabled);
 assert(track_at(project,1,0).steps[3].note == 62);
 settle(); // the audio side consumes the staged pattern (single-flight)
 // double_pattern: length and content double, automation follows.
 assert(perf.select_pattern(0));
 assert(perf.double_pattern());
 const auto doubled = track_at(project,0,0);
 assert(doubled.length == 32);
 assert(doubled.steps[19].enabled && doubled.steps[19].note == 62);
 assert(doubled.steps[16].enabled && doubled.steps[16].note == 60);
 assert(doubled.automation[0].length == 32 && doubled.automation[0].values[19] == 3.0f / 16.0f);
 settle();
 // erase_parameter_data: locks and the lane for id 3 go, others stay.
 assert(track_at(project,0,0).steps[3].locks[0].enabled);
 assert(perf.erase_parameter_data(3));
 assert(!track_at(project,0,0).steps[3].locks[0].enabled);
 assert(!track_at(project,0,0).automation[0].enabled);
 assert(!perf.erase_parameter_data(3)); // nothing left to erase
 // DUPLICATE button end-to-end: Step mode duplicates the step.
 perf.control(5, 0x04); perf.control(5, 0x00);
 assert(perf.selected_step() == 2);
 std::printf("duplicate/double/erase test passed\n");
 return 0;
}
