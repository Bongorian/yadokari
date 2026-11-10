#pragma once
#include <stdint.h>
namespace groove {
// Shared navigation metadata: page history, remembered pad grammar and strip
// assignment are connected to Performance. A single modal owner gates physical
// and desktop dispatch; Browser name editing retains its payload behind that gate.

// What is being edited. Never changes on a screen transition; changes
// only on an explicit selection.
enum class Focus : uint8_t { Group, Track, Step, Slice, Scene, Fx };
// The grammar of the 16 pads. Switchable independently of EditPage.
enum class PadMode : uint8_t { Play, Step, Keyboard, Chords, Scene, Clip, Mute, Solo };
// At most one modal; it takes all input.
enum class ModalKind : uint8_t { None, Confirm, Progress, Alert, Rename };
// Smart Strip assignment. Switching it
// never causes a screen transition; its target follows Focus.
enum class StripMode : uint8_t { Perform, Pitch, Mod, Notes };

// History of the EditPage axis: the last 8 visited pages with
// consecutive duplicates compressed and a back/forward cursor.
// record() drops any forward entries, matching browser semantics.
template<class Page> struct EditPageHistory {
 static constexpr unsigned kDepth = 8;
 Page entries[kDepth]{};
 uint8_t size = 0, cursor = 0;
 bool empty() const { return size == 0; }
 Page current() const { return entries[cursor]; }
 void replace_current(Page page) { if(size) entries[cursor]=page; }
 bool can_back() const { return size && cursor > 0; }
 bool can_forward() const { return size && cursor + 1 < size; }
 void record(Page page) {
  if (size && entries[cursor] == page) return;
  unsigned keep = size ? unsigned(cursor) + 1 : 0;
  if (keep == kDepth) { // full: evict the oldest entry
   for (unsigned i = 1; i < kDepth; ++i) entries[i - 1] = entries[i];
   keep = kDepth - 1;
  }
  entries[keep] = page;
  size = uint8_t(keep + 1);
  cursor = uint8_t(keep);
 }
 bool back(Page& out) { if (!can_back()) return false; out = entries[--cursor]; return true; }
 bool forward(Page& out) { if (!can_forward()) return false; out = entries[++cursor]; return true; }
 void clear() { size = 0; cursor = 0; }
};

// Three independent axes (Focus, PadMode, StripMode), plus page/history,
// a single dialog owner and input-free visual layers (ARC/toast/help).
struct NavigationLocation {
 uint8_t mode=0;Focus focus=Focus::Track;uint32_t key=0;uint16_t value[54]{};char directory[160]{};
 bool operator==(const NavigationLocation& other)const{return key==other.key && focus==other.focus;}
};

struct UiAxes {
 EditPageHistory<NavigationLocation> history;
 PadMode pad_mode = PadMode::Play;
 Focus focus = Focus::Track;
 ModalKind modal = ModalKind::None;
 StripMode strip_mode = StripMode::Perform;
};
}
