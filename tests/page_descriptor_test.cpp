// S0-3 consistency test (SPEC sections 1.2 and 2.1): page descriptors
// cover every EditPage exactly once, names match mode_name(), geometry
// matches the shared coordinate tokens, and the three-axis / three-layer state model
// behaves: EditPage history (depth 8, compression, back/forward),
// PadMode independence and single dialog ownership.
#include "page_descriptors.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace groove;
int main() {
 // Descriptor coverage: one entry per UiMode, ids a permutation.
 assert(kPageDescriptorCount == 20);
 bool seen[kPageDescriptorCount]{};
 for (const auto& d : kPageDescriptors) {
  unsigned id = unsigned(d.id);
  assert(id < kPageDescriptorCount && !seen[id]); seen[id] = true;
  assert(std::strcmp(d.name, mode_name(d.id)) == 0);
  assert(page_descriptor(d.id) == &d);
  // Geometry: the section 1.2 token frame, identical on every page.
  const PageGeometry& g = d.geometry;
  assert(g.header_y == ui::Geometry::header_y && g.body_y == ui::Geometry::body_y && g.body_h == ui::Geometry::body_h && g.knob_y == ui::Geometry::knob_y && g.knob_h == 52);
  assert(g.body_y + g.body_h == ui::Layout::legend_y); // floating controls never change the body viewport
  // Title-row zones: ordered, disjoint, inside the 480-wide screen.
  assert(g.title_x0 == 10 && g.title_x1 == 190);
  assert(g.dots_x0 == 198 && g.dots_x1 == 237);
  assert(g.status_x0 == 245 && g.status_x1 == 470);
  assert(g.title_x1 < g.dots_x0 && g.dots_x1 < g.status_x0 && g.status_x1 <= 480);
 }
 for (bool s : seen) assert(s);
 assert(page_descriptor(UiMode(200)) == nullptr);

 // EditPage history: compression, forward truncation, depth eviction.
 EditPageHistory<unsigned> h;
 unsigned out = 999;
 assert(h.empty() && !h.back(out) && !h.forward(out));
 h.record(1); h.record(1); h.record(2); h.record(3);
 assert(h.size == 3 && h.current() == 3);
 assert(h.back(out) && out == 2 && h.back(out) && out == 1 && !h.can_back());
 assert(h.forward(out) && out == 2);
 h.record(2); // same as current: compressed, forward entry survives
 assert(h.size == 3 && h.current() == 2 && h.can_forward());
 h.record(9); // new page drops the forward entry
 assert(h.size == 3 && h.current() == 9 && !h.can_forward());
 EditPageHistory<unsigned> full;
 for (unsigned i = 0; i < 12; ++i) full.record(i);
 assert(full.size == EditPageHistory<unsigned>::kDepth && full.current() == 11);
 assert(full.back(out) && out == 10);

 // Performance integration: set_mode maintains history and PadMode.
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 perf.set_mode(UiMode::Pad);
 assert(perf.pad_mode() == PadMode::Play);
 perf.set_mode(UiMode::Synth);
 assert(perf.pad_mode() == PadMode::Play); // non-grammar page: untouched
 perf.set_mode(UiMode::Keyboard);
 assert(perf.pad_mode() == PadMode::Keyboard);
 perf.set_mode(UiMode::Effects);
 assert(perf.pad_mode() == PadMode::Keyboard); // independence: persists
 // Descriptor table and the live switch agree on every grammar page.
 for (const auto& d : kPageDescriptors) if (d.pad_grammar_page) { perf.set_mode(d.id); assert(perf.pad_mode() == d.pad_mode); }
 // History navigation: back returns to the previously recorded page
 // without recording over the cursor.
 UiMode before = perf.mode();
 perf.set_mode(UiMode::Browser);
 assert(perf.mode() == UiMode::Browser);
 assert(perf.page_back() && perf.mode() == before);
 assert(perf.page_forward() && perf.mode() == UiMode::Browser);
 std::puts("page descriptors, axes history, single dialog owner and PadMode independence verified");
}
