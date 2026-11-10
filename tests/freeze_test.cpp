// Stage 5 S5-3: Track Freeze. Freezing captures one full pattern cycle
// of a track post-insert (voices + track inserts baked; group inserts,
// sends and the master chain stay live), writes it as a WAV asset and
// flags the track frozen_asset. A frozen track ignores its steps and
// insert chain: at each cycle wrap the engine retriggers the captured
// cycle on the track's sampler voice. The original instrument, steps
// and parameters stay in the pattern untouched, so unfreezing (and
// Undo) restore the track losslessly.
#include "session_files.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace groove;
namespace {
struct Buffer {
 std::vector<uint8_t> bytes; size_t at = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (at + n > bytes.size()) return false; __builtin_memcpy(p, bytes.data() + at, n); at += n; return true; }
 bool at_end() const { return at == bytes.size(); }
};
struct StoreFs : IFileAccess {
 std::map<std::string, std::vector<uint8_t>> files;
 unsigned list(const char*, FileEntry*, unsigned) override { return 0; }
 bool read(const char* path, uint64_t offset, void* out, unsigned count, unsigned& got) override {
  got = 0;
  auto it = files.find(path);
  if (it == files.end() || offset >= it->second.size()) return offset == 0 && count == 0;
  unsigned n = count;
  if (offset + n > it->second.size()) n = unsigned(it->second.size() - offset);
  std::memcpy(out, it->second.data() + offset, n);
  got = n;
  return true;
 }
 bool write(const char* path, uint64_t offset, const void* data, unsigned count, bool create) override {
  auto& v = files[path];
  if (create && offset == 0) v.clear();
  if (offset + count > v.size()) v.resize(offset + count);
  std::memcpy(v.data() + offset, data, count);
  return true;
 }
 bool move(const char* from, const char* to) override { files[to] = files[from]; files.erase(from); return true; }
 bool remove(const char* path) override { return files.erase(path) > 0; }
 bool directory(const char*) override { return true; }
 uint64_t size(const char* path) override { auto it = files.find(path); return it == files.end() ? 0 : it->second.size(); }
 bool exists(const char* path) override { return files.count(path) > 0; }
 bool revision(const char* path, FileRevision& r) override { r = {size(path), 7, 7, 7}; return true; }
};
struct StoreLibrary : ISampleLibrary {
 StoreFs& fs;
 mutable std::map<std::string, std::vector<uint8_t>> cache;
 mutable std::map<std::string, WavView> views;
 explicit StoreLibrary(StoreFs& f) : fs(f) {}
 bool parse_into(const char* path, WavView& view) const {
  auto it = fs.files.find(path);
  if (it == fs.files.end()) return false;
  auto& bytes = cache[path];
  bytes = it->second;
  return parse_wav(bytes.data(), bytes.size(), view);
 }
 bool load(const char* path, WavView& view) override {
  if (!parse_into(path, view)) return false;
  views[path] = view;
  return true;
 }
 WavView lookup(const char* path) const override {
  auto it = views.find(path);
  if (it != views.end()) return it->second;
  WavView view;
  if (!parse_into(path, view)) return {};
  views[path] = view;
  return view;
 }
 bool prepare(const ProjectState&, WavView*) override { return true; }
};
void setup_track(ProjectState& p) {
 auto t = track_at(p,0,0);
 t.instrument = Instrument::PolySaw;
 // Overdrive at high drive: the raw saw is quiet (cycle RMS ~0.008 at
 // the post-insert tap), so the fixture boosts it into a range where
 // the frozen / silent assertions are unambiguous.
 t.insert_fx[0] = 5; t.insert_fx[1] = 0.9f; t.insert_fx[2] = 0.0f; t.insert_fx[3] = 1;
 const uint8_t notes[4] = {60, 64, 67, 71};
 for (unsigned i = 0; i < 4; ++i) { auto& st = t.steps[i * 4]; st.enabled = true; st.note = notes[i]; st.velocity = 127; st.gate = 384; }
}
double render_cycle_rms(Engine& engine, float* buf) {
 double s = 0; unsigned n = 0;
 for (unsigned i = 0; i < 375; ++i) { engine.render(buf, 256); for (unsigned j = 0; j < 512; ++j) { s += double(buf[j]) * buf[j]; ++n; } }
 return std::sqrt(s / n);
}
} // namespace
int main() {
 // ---- Codec: frozen_asset roundtrips; out-of-range values invalid ----
 {
  auto in = std::make_unique<ProjectState>(); make_empty_project(*in);
  track_at(*in,2,5).frozen_asset = 41;
  Buffer sink; assert(encode_project(sink, *in));
  auto out = std::make_unique<ProjectState>(); Buffer source{sink.bytes};
  assert(decode_project(source, *out));
  assert(track_at(*out,2,5).frozen_asset == 41);
  assert(track_at(*out,0,0).frozen_asset == 255);
  TrackState bad{}; bad.frozen_asset = 200; assert(!valid_track(bad));
 }
 // ---- Engine: capture one cycle, then frozen playback semantics ----
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p); setup_track(*p);
  Engine engine(48000); assert(engine.stage_project(*p));
  assert(engine.submit({CommandType::Start, 0, 0, 0}));
  float buf[512];
  for (unsigned i = 0; i < 40; ++i) engine.render(buf, 256);
  assert(engine.freeze_capture_begin(0));
  unsigned guard = 0;
  while (engine.freeze_capture_state() != 3 && guard++ < 3000) engine.render(buf, 256);
  assert(engine.freeze_capture_state() == 3);
  uint32_t frames = engine.freeze_capture_frames();
  assert(frames > 91200 && frames < 100800); // 16 steps at 120 BPM / 48 kHz = 96000
  const int16_t* data = engine.freeze_capture_data(); assert(data);
  double sum = 0; for (uint32_t i = 0; i < frames * 2; ++i) sum += double(data[i]) * data[i];
  assert(std::sqrt(sum / (frames * 2)) / 32768.0 > 0.05); // the cycle has real content
  std::vector<int16_t> audio(data, data + frames * 2); // owned copy: the capture buffer is released below
  WavView view{}; view.data = reinterpret_cast<const uint8_t*>(audio.data()); view.frames = frames; view.rate = 48000; view.channels = 2; view.bits = 16; view.format = 1;
  WavView bindings[64]{}; bindings[5] = view;
  SampleAssetState assets[64]{};
  copy_text(assets[5].path, sizeof assets[5].path, "RECORDINGS/Freeze-001.wav");
  assets[5].metadata.rate = 48000; assets[5].metadata.frames = frames; assets[5].metadata.channels = 2; assets[5].metadata.bits = 16;
  PatternState pat; snapshot_pattern(pat,*p,0); pat.include_settings=true; pat.tracks[0].frozen_asset = 5;
  assert(engine.replace_pattern(0, pat, 1, true, nullptr, bindings, assets));
  for (unsigned i = 0; i < 8; ++i) engine.render(buf, 256); // consume the apply
  PatternState cleared = pat;
  for (auto& st : cleared.tracks[0].steps) st.enabled = false;
  assert(engine.replace_pattern(0, cleared, 1, true, nullptr, bindings, assets));
  for (unsigned i = 0; i < 8; ++i) engine.render(buf, 256);
  engine.freeze_capture_release();
  render_cycle_rms(engine, buf); // settle: first full frozen cycle after the apply
  double r1 = render_cycle_rms(engine, buf);
  double r2 = render_cycle_rms(engine, buf);
  assert(r1 > 0.05); // frozen audio plays although every step is off
  assert(std::fabs(r1 - r2) < r1 * 0.15); // and loops deterministically
  PatternState other_insert = cleared; other_insert.tracks[0].insert_fx[0] = 11; other_insert.tracks[0].insert_fx[1] = 0.9f;
  assert(engine.replace_pattern(0, other_insert, 1, true, nullptr, bindings, assets));
  for (unsigned i = 0; i < 8; ++i) engine.render(buf, 256);
  double r3 = render_cycle_rms(engine, buf);
  assert(std::fabs(r3 - r1) < r1 * 0.15); // insert chain is baked + bypassed
  PatternState unfrozen = other_insert; unfrozen.tracks[0].frozen_asset = 255;
  assert(engine.replace_pattern(0, unfrozen, 1, true));
  for (unsigned i = 0; i < 8; ++i) engine.render(buf, 256);
  render_cycle_rms(engine, buf); // burn the ringing one-shot tail
  assert(render_cycle_rms(engine, buf) < 0.005); // silent: no steps, no freeze
 }
 // ---- Job: Mixer FREEZE key -> SessionFiles capture -> asset + flag ----
 {
  Engine engine(48000);
  ProjectState project; make_empty_project(project); setup_track(project);
  assert(engine.stage_project(project));
  Performance perf(engine, project);
  StoreFs fs; StoreLibrary lib(fs);
  SessionFiles files(fs, lib, engine, project, perf);
  assert(perf.play());
  perf.set_mode(UiMode::Mixer);
  assert(!std::strcmp(perf.soft_label(4), "FREEZE"));
  assert(perf.soft_key(4));
  float buf[512];
  bool saw_modal = false;
  for (int i = 0; i < 2500; ++i) { engine.render(buf, 256); files.service(); saw_modal = saw_modal || perf.freeze_modal() != 0; }
  assert(saw_modal); // the capture dialog opened while the job ran
  assert(perf.freeze_modal() == 0);
  unsigned slot = track_at(project,0,0).frozen_asset;
  assert(slot < 64);
  assert(fs.exists("RECORDINGS/Freeze-001.wav"));
  assert(!std::strcmp(project.assets[slot].path, "RECORDINGS/Freeze-001.wav"));
  WavView v; assert(lib.load(project.assets[slot].path, v));
  assert(v.frames > 91200 && v.frames < 100800 && v.channels == 2 && v.rate == 48000);
  assert(render_cycle_rms(engine, buf) > 0.05); // frozen track audible via the job's binding
  assert(!std::strcmp(perf.soft_label(4), "UNFREEZE"));
  assert(perf.soft_key(4));
  assert(track_at(project,0,0).frozen_asset == 255);
  for (int i = 0; i < 8; ++i) { engine.render(buf, 256); files.service(); } // consume the apply
  assert(perf.undo()); assert(track_at(project,0,0).frozen_asset == slot);
  for (int i = 0; i < 8; ++i) { engine.render(buf, 256); files.service(); }
  assert(perf.redo()); assert(track_at(project,0,0).frozen_asset == 255);
 }
 std::puts("track freeze: codec, capture, frozen playback, chain bypass and job flow verified");
}
