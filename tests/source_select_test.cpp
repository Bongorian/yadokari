// SCR-EDT-01 SOURCE SELECT (SPEC 2.8, S-04): re-pressing the Synth
// page button inside Synth mode opens the family-grouped engine list,
// the ENGINE/FAMILY knobs walk it, LOAD ENGINE (Top5)
// confirms through the single SetInstrument command path and lands on
// the CORE view, and replacing an Unknown engine needs a second
// confirm because it discards the preserved payload (SPEC 2.9).
#include "performance.h"
#include <cassert>
#include <cstdio>
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
 // ENGINE knob walks the registry in enum order.
 perf.knob(1, 1000);
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 perf.knob(1, -1000);
 assert(perf.source_cursor() == unsigned(Instrument::PolySaw));
 // FAMILY knob jumps to the first engine of the next family
 // (PolySaw is SYNTH; next is SAMPLER, whose only engine is Sampler).
 perf.knob(0, 1000);
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 // Main encoder scrolls too.
 perf.encoder(1);
 assert(perf.source_cursor() == unsigned(Instrument::DaisySynth));
 perf.encoder(-1);
 assert(perf.source_cursor() == unsigned(Instrument::Sampler));
 // Confirm with LOAD ENGINE: single SetInstrument, selector closes,
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
 assert(perf.source_cursor() == unsigned(Instrument::PolySaw));
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
 assert(instrument_count()==27);
 assert(perf.toggle_source_select());assert(perf.select_source(unsigned(Instrument::ExternalMidi)));assert(perf.encoder(1));assert(perf.source_cursor()==unsigned(Instrument::Acid));assert(perf.encoder(-1));assert(perf.source_cursor()==unsigned(Instrument::ExternalMidi));
 assert(perf.select_source(unsigned(Instrument::PolySaw)));assert(perf.knob(0,-1000));assert(perf.source_cursor()==unsigned(Instrument::DaisyAnalogDrums));
 for(unsigned id=0;id<instrument_count();++id){assert(perf.select_source(id));if(id+1<instrument_count()){assert(perf.encoder(1));assert(perf.source_cursor()==id+1);}}
 assert(perf.toggle_source_select());
 // Leaving the mode closes the selector.
 perf.control(7, 0x20); perf.control(7, 0x00);
 assert(perf.source_select());
 perf.set_mode(UiMode::Pad);
 assert(!perf.source_select());
 std::printf("source select (SCR-EDT-01 list selection, unknown-arm) passed\n");
 return 0;
}
