// Stage 4 S4-2: four morph sets. morph_a/morph_b always hold the
// active set; the other three sets live in morph_extra_* and rotate
// through morph_swap_sets. Each set keeps independent A/B snapshots,
// selection is mirrored to the engine (SetMorphSet), persisted in
// the MACR chunk tail, and Shift+STORE A on Perform cycles sets.
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace groove;
namespace {
struct Buffer {
 std::vector<uint8_t> bytes; size_t at = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (at + n > bytes.size()) return false; __builtin_memcpy(p, bytes.data() + at, n); at += n; return true; }
 bool at_end() const { return at == bytes.size(); }
};
bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }
} // namespace
int main() {
 // A fresh project must satisfy valid_project (the morph extras
 // start at descriptor initials, never zero-filled).
 {
  ProjectState fresh; make_empty_project(fresh);
  assert(valid_project(fresh));
 }
 Engine engine(8000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 auto pump = [&] { float buf[128]; for (int i = 0; i < 4; ++i) engine.render(buf, 64); };
 pump();
 unsigned level = parameter_index(1);
 const float want[4] = {0.11f, 0.22f, 0.33f, 0.44f};
 // Store a distinct A snapshot in each of the four sets.
 for (unsigned s = 0; s < 4; ++s) {
  if (s) assert(perf.morph_select_set(s));
  assert(project.morph_set == s);
  assert(perf.set_parameter(1, want[s]));
  assert(perf.store_morph(0));
  pump();
  assert(near(project.morph_a[0].values[level], want[s]));
 }
 // Every set recalls its own snapshot, in a scrambled order.
 const unsigned order[4] = {2, 0, 3, 1};
 for (unsigned s : order) {
  assert(perf.morph_select_set(s));
  assert(project.morph_set == s);
  assert(near(project.morph_a[0].values[level], want[s]));
 }
 // A full cycle returns to identical state (rotation is lossless).
 assert(perf.morph_select_set(0));
 auto before = project.morph_a[0].values[level];
 for (unsigned s = 1; s <= 4; ++s) assert(perf.morph_select_set(s & 3));
 assert(project.morph_set == 0 && near(project.morph_a[0].values[level], before));
 std::printf("morph set rotation ok\n");
 // Codec round trip keeps the active set and all stored sets.
 Buffer b; assert(encode_project(b, project));
 ProjectState back; Buffer r; r.bytes = b.bytes;
 assert(decode_project(r, back));
 assert(back.morph_set == project.morph_set);
 for (unsigned s = 0; s < 4; ++s) {
  morph_swap_sets(back, s);
  assert(near(back.morph_a[0].values[level], want[s]));
 }
 std::printf("morph sets codec ok\n");
 std::printf("morph sets test passed\n");
 return 0;
}
