// Stage 4 S4-1: Temporary Reload + LOCK snapshot base (SPEC 2.7 /
// Stage 4 acceptance: the state hash after Reload matches the
// restore point exactly). Entering Perform captures a full-project
// restore point automatically; Shift+LOCK returns to it. The LOCK
// button (byte 3 / 0x20) saves a full snapshot on any page and
// restores + releases it on the next press; LED 24 shows a point
// exists. The hash is the MGXC encoding of the whole ProjectState.
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <memory>
using namespace groove;
namespace {
struct Buffer {
 std::vector<uint8_t> bytes; size_t at = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (at + n > bytes.size()) return false; __builtin_memcpy(p, bytes.data() + at, n); at += n; return true; }
 bool at_end() const { return at == bytes.size(); }
};
std::vector<uint8_t> hash_of(const ProjectState& p) { Buffer b; assert(encode_project(b, p)); return b.bytes; }
void mutate(Performance& perf, ProjectState& project) {
 // A song section (project state) plus a tempo change.
 perf.set_mode(UiMode::Song);
 assert(perf.soft_key(6)); // ADD
 assert(project.song.length == 1);
 perf.set_mode(UiMode::Step);
 assert(perf.knob(0, 10000)); // tempo +10
 assert(project.tempo == 130);
}
} // namespace
int main() {
 Engine engine(8000);
 ProjectState project; make_empty_project(project);
 project.tempo = 120;
 Performance perf(engine, project);
 uint8_t leds[62];
 // No points yet.
 assert(!perf.reload_available() && !perf.lock_available());
 perf.button_leds(leds); assert(leds[24] == 0);
 // Entering Perform captures the restore point.
 perf.set_mode(UiMode::Perform);
 assert(perf.reload_available());
 perf.button_leds(leds); assert(leds[24] != 0);
 float scratch[2]{};
 auto point = hash_of(project);
 mutate(perf, project);
 assert(hash_of(project) != point);
 // Shift+LOCK performs the Temporary Reload.
 perf.control(1, 0x40); // shift down
 perf.control(3, 0x20); perf.control(3, 0x00);
 perf.control(1, 0x00);
 assert(project.tempo == 120 && project.song.length == 0);
 assert(hash_of(project) == point);
 engine.render(scratch,0); // consume the accepted snapshot before another restore
 assert(perf.reload_available()); // the auto point survives a reload
 std::printf("temporary reload hash identity ok\n");
 // LOCK toggles save / restore+release on any page.
 perf.set_mode(UiMode::Pad);
 assert(!perf.lock_available());
 perf.control(3, 0x20); perf.control(3, 0x00);
 assert(perf.lock_available());
 mutate(perf, project);
 assert(hash_of(project) != point);
 perf.control(3, 0x20); perf.control(3, 0x00);
 assert(hash_of(project) == point);
 assert(!perf.lock_available());
 std::printf("lock save/restore ok\n");
 // A rejected restore changes neither the UI project nor its restore point.
 {
  auto p=std::make_unique<ProjectState>();make_empty_project(*p);Engine e(8000);assert(e.stage_project(*p));e.render(scratch,0);
  Performance ui(e,*p);ui.set_mode(UiMode::Perform);assert(ui.lock_press());mutate(ui,*p);auto edited=hash_of(*p);unsigned generation=e.staged_generation();
  while(e.submit({CommandType::SetFill,0,0,0})){}
  assert(!ui.lock_press() && ui.lock_available() && hash_of(*p)==edited && e.staged_generation()==generation);
  assert(!ui.temporary_reload() && ui.reload_available() && hash_of(*p)==edited);
  e.render(scratch,0);assert(ui.lock_press() && !ui.lock_available() && p->tempo==120 && p->song.length==0);
  assert(e.staged_generation()==generation+1);e.render(scratch,0);
  assert(ui.lock_press());mutate(ui,*p);edited=hash_of(*p);
  assert(e.stage_project(*p));assert(!ui.lock_press() && ui.lock_available() && hash_of(*p)==edited);
  e.render(scratch,0);assert(ui.lock_press() && !ui.lock_available());e.render(scratch,0);
 }
 std::printf("reload failure preserves UI and restore points; retry succeeds\n");
 std::printf("reload test passed\n");
 return 0;
}
