// SCR-EDT-04 ENGINE LAB: one knob grammar for the five creative engines
// (EDIT_UI 1.3 / 6). Before the unification, instruments 13-16 put the
// envelope follower on knobs 1-2 and had no lane editing at all, while
// WaveSequence put lane/cell there and hid the follower behind Shift.
// The descriptor-declared tail (EnginePageDescriptor::lab_tail) is the
// only per-engine difference left on the page.
#include "performance.h"
#include "engine_pages.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>
using namespace groove;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int main() {
 // Descriptor tails for the creative engines; None elsewhere.
 assert(engine_page_descriptor(unsigned(Instrument::WaveSequence))->lab_tail == LabTail::SampleSlot); // WaveSequence
 assert(engine_page_descriptor(unsigned(Instrument::ChipClock))->lab_tail == LabTail::Tune);       // ChipClock
 assert(engine_page_descriptor(unsigned(Instrument::TapeMicrosound))->lab_tail == LabTail::SampleSlot); // TapeMicrosound
 assert(engine_page_descriptor(unsigned(Instrument::WestCoast))->lab_tail == LabTail::Fold);       // WestCoast
 assert(engine_page_descriptor(unsigned(Instrument::SpectralFreeze))->lab_tail == LabTail::SampleSlot); // SpectralFreeze
 assert(engine_page_descriptor(unsigned(Instrument::PolySaw))->lab_tail == LabTail::None);        // PolySaw
 assert(engine_page_descriptor(unsigned(Instrument::DaisyAnalogDrums))->lab_tail == LabTail::None);        // Kick

 auto project = std::make_unique<ProjectState>();
 make_empty_project(*project);
 auto engine = std::make_unique<Engine>();
 assert(engine->stage_project(*project));
 Performance ui(*engine, *project);
 std::vector<float> out(512);
 engine->render(out.data(), 256);
 auto track = track_at(*project,0,0);
 const Instrument engines[] = {Instrument::WaveSequence, Instrument::ChipClock,
                               Instrument::TapeMicrosound, Instrument::WestCoast,
                               Instrument::SpectralFreeze};
 const char* shared[] = {"LANE", "CELL", "CELL VALUE", "LOOP LENGTH",
                         "MUTATION", "ENV SOURCE", "ENV DEPTH"};
 for (Instrument inst : engines) {
  track.instrument = inst;
  engine->render(out.data(), 256);
  ui.set_shift(false);
  ui.set_mode(UiMode::Synth);
  ui.soft_key(0);
  assert(ui.soft_key(3) && ui.instrument_view() == 3);
  // Cursor reset to lane 0 / cell 0.
  assert(ui.knob(0, -5000));
  assert(ui.knob(1, -7000));
  assert(ui.creative_lane() == 0 && ui.creative_cell() == 0);
  // Knobs 1-7 mean the same thing on every creative engine.
  for (unsigned i = 0; i < 7; ++i) assert(!std::strcmp(ui.knob_label(i), shared[i]));
  // Anti-swap: knob 1 selects the lane; it must not touch the follower
  // (the pre-unification 13-16 grammar edited follower_source here).
  int source_before = track.creative.follower_source;
  assert(ui.knob(0, 2000));
  assert(ui.creative_lane() == 2);
  assert(int(track.creative.follower_source) == source_before);
  // Cell cursor, then cell value on lane 2 / cell 1.
  assert(ui.knob(1, 1000));
  assert(ui.creative_cell() == 1);
  int value_before = track.creative.value[2][1];
  assert(ui.knob(2, 5000));
  assert(int(track.creative.value[2][1]) == clampi(value_before + 5, 0, 127));
  // Loop length and mutation.
  int length_before = track.creative.length[2];
  assert(ui.knob(3, -2000));
  assert(int(track.creative.length[2]) == clampi(length_before - 2, 1, 8));
  int mutation_before = track.creative.mutation;
  assert(ui.knob(4, 3000));
  assert(int(track.creative.mutation) == clampi(mutation_before + 3, 0, 100));
  // Follower source / amount are plain knobs 6-7 now, not Shift layers.
  assert(ui.knob(5, 3000));
  assert(int(track.creative.follower_source) == clampi(source_before + 3, 0, 16));
  int amount_before = track.creative.follower_amount;
  assert(ui.knob(6, -2000));
  assert(int(track.creative.follower_amount) == clampi(amount_before - 2, -100, 100));
  // Read-backs agree with the state.
  assert(ui.knob_value(0) == 2 && ui.knob_value(1) == 2);
  assert(ui.knob_value(5) == float(track.creative.follower_source));
  assert(ui.knob_value(6) == float(track.creative.follower_amount));
  // Knob 8 is the descriptor tail.
  LabTail tail = engine_page_descriptor(unsigned(inst))->lab_tail;
  if (tail == LabTail::SampleSlot) {
   assert(!std::strcmp(ui.knob_label(7), "SAMPLE SLOT"));
   int slot_before = track.sample_slot;
   assert(ui.knob(7, 3000));
   assert(int(track.sample_slot) == clampi(slot_before + 3, 0, 63));
   assert(ui.knob_parameter(7) == 0);
  } else if (tail == LabTail::Tune) {
   assert(!std::strcmp(ui.knob_label(7), "TUNE NOTE"));
   assert(ui.knob(7, 2000));
   assert(ui.tuning_note() == 2);
   assert(ui.knob_parameter(7) == 0);
  } else if (tail == LabTail::Fold) {
   assert(!std::strcmp(ui.knob_label(7), "FOLD"));
   assert(ui.knob_parameter(7) == 32);
   float fold_before = track_parameter(track, 32);
   assert(ui.knob(7, 5000));
   assert(track_parameter(track, 32) > fold_before);
  }
  engine->render(out.data(), 256);
 }
 return 0;
}
