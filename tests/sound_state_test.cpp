// Sound snapshots carry complete engine state; root note stays on the track.
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
namespace {
struct Buffer {
 std::vector<uint8_t> bytes; size_t at = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (at + n > bytes.size()) return false; __builtin_memcpy(p, bytes.data() + at, n); at += n; return true; }
 bool at_end() const { return at == bytes.size(); }
};
} // namespace
int main() {
 // Codec: full slice state on one sound, slice-state-only on
 // another (default edit must not suppress the SMPL block).
 {
  auto project = std::make_unique<ProjectState>(); make_empty_project(*project);
  auto& s3 = project->sounds[3];
  s3.instrument = Instrument::Sampler; s3.sample_slot = 5;
  s3.sample_edit.slice_count = 4; s3.sample_edit.markers[1] = 0.25f; s3.sample_edit.markers[2] = 0.5f; s3.sample_edit.markers[3] = 0.75f; s3.sample_edit.markers[4] = 1.0f;
  s3.sample_slice = 2; s3.sample_slices = 4; s3.sample_loop = true; s3.sample_reverse = true;
  auto& s5 = project->sounds[5];
  s5.instrument = Instrument::Sampler; s5.sample_slot = 6;
  s5.sample_slice = 1; s5.sample_slices = 4;
  auto& kit=project->sounds[255];kit.instrument=Instrument::DrumKit;kit.kit.pads[7].sample[0]=11;kit.kit.pads[7].edit.speed=1.5f;kit.kit.pads[7].insert_fx[0]=3;kit.kit.pads[7].level=.38f;
  auto& midi=project->sounds[254];midi.instrument=Instrument::ExternalMidi;midi.midi_program=42;midi.midi_cc[32]=5;midi.midi_cc_used[4]=1;
  auto& adsr=project->sounds[253];adsr.instrument=Instrument::DaisySynth;adsr.daisy_adsr[0]=.08f;adsr.daisy_adsr[3]=.7f;
  // Dormant kit phrases survive even with a non-kit instrument attached.
  project->patterns[15].tracks[2].kit.steps[7][255].enabled=true;
  project->patterns[15].tracks[2].kit.steps[7][255].fx_lock[0]=4;
  assert(valid_project(*project));
  Buffer buffer; assert(encode_project(buffer, *project));
  auto restored = std::make_unique<ProjectState>(); assert(decode_project(buffer, *restored));
  const auto& r3 = restored->sounds[3];
  assert(r3.sample_slice == 2 && r3.sample_slices == 4 && r3.sample_loop && r3.sample_reverse);
  const auto& r5 = restored->sounds[5];
  assert(r5.sample_slice == 1 && r5.sample_slices == 4 && !r5.sample_loop);
  assert(restored->sounds[255].kit.pads[7].sample[0]==11 && restored->sounds[255].kit.pads[7].edit.speed==1.5f && restored->sounds[255].kit.pads[7].insert_fx[0]==3 && restored->sounds[255].kit.pads[7].level==.38f);
  assert(restored->sounds[254].midi_program==42 && restored->sounds[254].midi_cc[32]==5 && restored->sounds[254].midi_cc_used[4]==1);
  assert(restored->sounds[253].daisy_adsr[0]==.08f && restored->sounds[253].daisy_adsr[3]==.7f);
  assert(restored->patterns[15].tracks[2].kit.steps[7][255].enabled && restored->patterns[15].tracks[2].kit.steps[7][255].fx_lock[0]==4);
  Buffer again; assert(encode_project(again, *restored) && again.bytes == buffer.bytes);
  std::printf("sound codec round-trip ok\n");
 }
 // Store / recall through Performance, with undo restoring the
 // exact prior playback state.
 {
  Engine engine(8000);
  ProjectState project; make_empty_project(project);
  auto track = track_at(project,0,0);
  track.instrument = Instrument::Sampler;
  track.sample_slice = 2; track.sample_slices = 4; track.sample_loop = true; track.sample_reverse = true; track.root_note = 48;
  Performance perf(engine, project);
  assert(perf.store_sound(3));
  const auto& stored = project.sounds[3];
  assert(stored.sample_slice == 2 && stored.sample_slices == 4 && stored.sample_loop && stored.sample_reverse);
  // Move the track somewhere else, then recall.
  track.sample_slice = 0; track.sample_slices = 1; track.sample_loop = false; track.sample_reverse = false; track.root_note = 60;
  // (direct writes desync the engine mirror; re-stage via a fresh
  // Performance for the recall half)
  Performance perf2(engine, project);
  assert(!perf2.recall_sound(3)); // Missing PCM keeps the current instance intact.
  float pcm[2]={0,0};Command bind{CommandType::BindSample,0,0,0};bind.sample={reinterpret_cast<uint8_t*>(pcm),2,8000,1,32,3};assert(engine.submit(bind));engine.render(pcm,0);
  assert(perf2.recall_sound(3));
  const auto after = track_at(project,0,0);
  assert(after.sample_slice == 2 && after.sample_slices == 4 && after.sample_loop && after.sample_reverse && after.root_note == 60);
  assert(perf2.undo());
  const auto undone = track_at(project,0,0);
  assert(undone.sample_slice == 0 && undone.sample_slices == 1 && !undone.sample_loop && !undone.sample_reverse && undone.root_note == 60);
  std::printf("sound store/recall/undo ok\n");
 }
 std::printf("sound state test passed\n");
 return 0;
}
