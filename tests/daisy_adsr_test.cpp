// Unit 22 (S-05): DaisySynth ADSR promoted to descriptor parameters
// 104-107 with storage, codec persistence and Synth ENVELOPE knobs.
#include "performance.h"
#include "project_codec.h"
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace groove;
struct Sink { std::vector<uint8_t> bytes; bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; } };
struct Source { const std::vector<uint8_t>& bytes; size_t pos = 0; bool read(uint8_t* p, size_t n) { if (n > bytes.size() - pos) return false; for (size_t i = 0; i < n; ++i) p[i] = bytes[pos++]; return true; } bool at_end() const { return pos == bytes.size(); } };
int main() {
 // Registration: descriptors, defaults matching the old fixed
 // constants, morph sentinel (not a morph target).
 for (uint16_t id = 104; id <= 107; ++id) {
  const auto* d = parameter_descriptor(id);
  assert(d && d->id == id);
  assert(parameter_index(id) == kMorphParameters);
 }
 assert(std::strcmp(parameter_descriptor(104)->name, "DAISY ATTACK") == 0);
 assert(parameter_descriptor(106)->initial == 0.55f);
 // Registry / page shape now agree the envelope is editable.
 const auto* inst = instrument_descriptor(unsigned(Instrument::DaisySynth));
 assert(inst && inst->envelope == EnvelopeEditable);
 assert(engine_page_descriptor(unsigned(Instrument::DaisySynth))->shape == ShapeKind::Envelope);
 // Storage mapping + validation.
 ProjectState project; make_empty_project(project);
 auto track = track_at(project,0,0);
 track.instrument=Instrument::DaisySynth; // Only the mounted engine is serialized.
 assert(track.daisy_adsr[0] == 0.005f && track.daisy_adsr[2] == 0.55f);
 assert(set_track_parameter(track, 106, 0.25f));
 assert(track_parameter(track, 106) == 0.25f);
 assert(!set_track_parameter(track, 104, 99.0f)); // out of range
 assert(valid_track(track));
 // Codec round trip persists the values (and byte-identically).
 track.daisy_adsr[0] = 0.02f; track.daisy_adsr[1] = 0.5f; track.daisy_adsr[3] = 1.5f;
 Sink sink; assert(encode_project(sink, project));
 ProjectState out; Source source{sink.bytes}; assert(decode_project(source, out));
 assert(track_at(out,0,0).daisy_adsr[0] == 0.02f);
 assert(track_at(out,0,0).daisy_adsr[1] == 0.5f);
 assert(track_at(out,0,0).daisy_adsr[2] == 0.25f);
 assert(track_at(out,0,0).daisy_adsr[3] == 1.5f);
 Sink again; assert(encode_project(again, out)); assert(again.bytes == sink.bytes);
 // Knob wiring: Synth ENVELOPE page, knobs 3-6 (indices 2-5).
 Engine engine(48000);
 Performance perf(engine, project);
 assert(engine.stage_project(project));
 perf.select_track(0);
 assert(perf.set_instrument(Instrument::DaisySynth));
 perf.set_mode(UiMode::Synth);
 assert(perf.soft_key(2)); // ENVELOPE page
 assert(perf.instrument_view() == 2);
 assert(perf.knob_parameter(2) == engine_target(Instrument::DaisySynth,0) && perf.knob_parameter(5) == engine_target(Instrument::DaisySynth,3));
 float before = track_at(project,0,0).daisy_adsr[1];
 assert(perf.knob(3, -2000)); // DECAY down
 float after = track_at(project,0,0).daisy_adsr[1];
 assert(after < before && after >= 0.001f);
 std::printf("daisy adsr (S-05 promotion) test passed\n");
 return 0;
}
