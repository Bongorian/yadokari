// Stage 3 S3-3: per-slice Warp. Each slice of a sampler edit carries
// a warp flag; a warped slice is tempo-locked at trigger time by
// scaling the voice pitch with engine_tempo / asset_bpm (clamped to
// x0.5..x2), re-locking every slice independently so uneven slices
// cannot drift apart. The flags persist through the project codec
// (track SMPL, sound SMPL and all 16 kit pad edits, appended after
// the zones in the TCOM-append style; pre-warp files read as Off).
#include "engine.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
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
unsigned crossings(const std::vector<float>& out, unsigned frames) {
 unsigned n = 0;
 for (unsigned i = 1; i < frames; ++i) if ((out[i * 2] < 0) != (out[(i - 1) * 2] < 0)) ++n;
 return n;
}
} // namespace
int main() {
 // Validation: warp values are boolean.
 {
  SampleEdit edit; edit.slice_warp[3] = 1; assert(valid_sample_edit(edit));
  edit.slice_warp[3] = 2; assert(!valid_sample_edit(edit));
 }
 // Codec round-trip: track edit + kit pad edit.
 {
  auto project = std::make_unique<ProjectState>(); make_empty_project(*project);
  auto& edit = track_at(*project,0,0).sample_edit;
  edit.slice_count = 4;
  for (unsigned i = 0; i <= 4; ++i) edit.markers[i] = float(i) / 4;
  edit.slice_warp[1] = 1; edit.slice_warp[3] = 1;
  track_at(*project,0,1).instrument = Instrument::DrumKit;
  auto& pad_edit = track_at(*project,0,1).kit.pads[2].edit;
  pad_edit.slice_count = 2; pad_edit.markers[1] = 0.5f; pad_edit.markers[2] = 1.0f;
  pad_edit.slice_warp[0] = 1;
  Buffer buffer; assert(encode_project(buffer, *project));
  auto restored = std::make_unique<ProjectState>(); assert(decode_project(buffer, *restored));
  const auto& saved = track_at(*restored,0,0).sample_edit;
  assert(saved.slice_warp[0] == 0 && saved.slice_warp[1] == 1 && saved.slice_warp[2] == 0 && saved.slice_warp[3] == 1);
  const auto& saved_pad = track_at(*restored,0,1).kit.pads[2].edit;
  assert(saved_pad.slice_warp[0] == 1 && saved_pad.slice_warp[1] == 0);
  Buffer again; assert(encode_project(again, *restored) && again.bytes == buffer.bytes);
  std::printf("warp codec round-trip ok\n");
 }
 // Engine: asset bpm 120, project tempo 60 -> warped slices play at
 // half rate; unwarped slices are untouched.
 {
  const uint32_t rate = 48000, frames = 48000;
  std::vector<int16_t> pcm(frames);
  for (uint32_t i = 0; i < frames; ++i) pcm[i] = int16_t(16000 * std::sin(6.283185307179586 * 440.0 * double(i) / rate));
  WavView view{reinterpret_cast<const uint8_t*>(pcm.data()), frames, rate, 1, 16, 1, nullptr};
  auto project = std::make_unique<ProjectState>(); make_empty_project(*project);
  project->tempo = 60;
  project->assets[0].metadata.bpm = 120;
  auto track = track_at(*project,0,0);
  track.instrument = Instrument::Sampler; track.sample_slot = 0; track.root_note = 60;
  auto render_slice = [&](unsigned slice, bool warp) {
   SampleEdit edit; edit.slice_count = 2; edit.markers[0] = 0; edit.markers[1] = 0.5f; edit.markers[2] = 1.0f;
   edit.fade_ms = 0; edit.slice_warp[slice] = warp ? 1 : 0;
   Engine engine(rate);
   assert(engine.stage_project(*project));
   Command install{CommandType::SetSample, 0, 0, 0}; install.sample = view;
   assert(engine.submit(install));
   Command set_edit{CommandType::SetSampleEdit, 0, 0, 0};
   set_edit.sample_edit = edit; set_edit.parameter = 1; set_edit.value = 2; set_edit.step = uint8_t(slice);
   assert(engine.submit(set_edit));
   assert(engine.submit({CommandType::Trigger, 0, 0, 127}));
   std::vector<float> out(24000 * 2, 0.0f);
   engine.render(out.data(), 24000);
   return crossings(out, 24000);
  };
  unsigned plain = render_slice(0, false);
  unsigned warped = render_slice(1, true);
  unsigned plain_sliced = render_slice(1, false);
  std::printf("crossings: plain=%u warped=%u plain-slice2=%u\n", plain, warped, plain_sliced);
  // 440 Hz over 0.5 s -> ~440 crossings; warped at x0.5 -> ~220.
  assert(plain > 400 && plain < 480);
  assert(plain_sliced > 400 && plain_sliced < 480);
  assert(warped > 180 && warped < 260);
  std::printf("warp engine factor ok\n");
 }
 std::printf("warp test passed\n");
 return 0;
}
