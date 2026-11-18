// SCR-EDT-01 SOURCE SELECT (SPEC 2.8, S-04): re-pressing the Synth
// page button inside Synth mode opens the family-grouped engine list,
// the ENGINE/FAMILY knobs walk it, DEFAULT SOUND (Top5)
// confirms through the single SetInstrument command path and lands on
// the CORE view, and replacing an Unknown engine needs a second
// confirm because it discards the preserved payload (SPEC 2.9).
#include "performance.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace groove;
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 auto track = [&]() -> ConstTrackView {
  return track_at(project,project.active_pattern,perf.selected_track());
 };
 perf.set_mode(UiMode::Synth);
 assert(!perf.source_select());
 assert(track().instrument == Instrument::Empty);
 // Open: Synth button re-press (byte 7, mask 0x20).
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(perf.source_select());
 assert(perf.source_cursor() == unsigned(Instrument::PolySaw)); // first public engine; Empty does not default to Kick
 assert(perf.knob_label(1)[0] == 'E'); // ENGINE
 // ENGINE knob walks the grouped display order.
 perf.knob(1, 1000);
 assert(perf.source_cursor() == unsigned(Instrument::DaisySynth));
 perf.knob(1, -1000);
 assert(perf.source_cursor() == unsigned(Instrument::PolySaw));
 // FAMILY knob selects the first engine in the next family.
 perf.knob(0, 1000);
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 perf.encoder(1);
 assert(perf.source_cursor() == unsigned(Instrument::SoundFont));
 perf.encoder(-1);
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 // Confirm with DEFAULT SOUND: single SetInstrument, selector closes,
 // landing view is CORE (instrument_view 0).
 assert(!perf.knob(7,1000) && perf.source_select());assert(perf.soft_key(4));
 assert(!perf.source_select());
 assert(track().instrument == Instrument::Sampler);
 assert(perf.instrument_view() == 0);
 // Reopen and cancel with the Synth button: no change.
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(perf.source_select());
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 perf.knob(1, -1000);
 assert(perf.source_cursor() == unsigned(Instrument::Formant));
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(!perf.source_select());
 assert(track().instrument == Instrument::Sampler);
 // Unknown engine: the first confirm only arms the payload discard.
 track_at(project,project.active_pattern,perf.selected_track()).instrument =
     Instrument::Unknown;
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(perf.source_select());
 assert(perf.source_cursor() == unsigned(Instrument::PolySaw)); // Unknown starts at the first public engine
 assert(!perf.knob(7,1000) && perf.source_select());assert(perf.soft_key(4));
 assert(perf.source_select()); // still open, only armed
 assert(perf.source_arm_unknown());
 assert(track().instrument == Instrument::Unknown);
 assert(perf.encoder_push()); // common modal confirm discards and replaces
 assert(!perf.source_select());
 assert(track().instrument == Instrument::PolySaw);
 // The complete registry is public and contiguous again after removal.
 assert(instrument_count()==28);
 assert(perf.toggle_source_select());
 bool seen[unsigned(Instrument::Unknown)]{};
 for(unsigned i=0;i<instrument_count();++i){
  unsigned id=unsigned(kEngineDisplayOrder[i]);assert(id<instrument_count() && !seen[id]);seen[id]=true;
  assert(perf.select_source(id));
  if(i+1<instrument_count()){
   assert(perf.encoder(1));assert(perf.source_cursor()==unsigned(kEngineDisplayOrder[i+1]));
  }
 }
 assert(perf.select_source(unsigned(Instrument::Resonator)));
 assert(perf.encoder(1) && perf.source_cursor()==unsigned(Instrument::Texture));
 assert(perf.encoder(1) && perf.source_cursor()==unsigned(Instrument::Chimera));
 assert(!std::strcmp(engine_family_name(Instrument::SoundFont),"SAMPLER"));
 assert(perf.toggle_source_select());
 // Leaving the mode closes the selector.
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(perf.source_select());
 perf.set_mode(UiMode::Pad);
 assert(!perf.source_select());
 std::printf("source select (SCR-EDT-01 list selection, unknown-arm) passed\n");
 return 0;
}
