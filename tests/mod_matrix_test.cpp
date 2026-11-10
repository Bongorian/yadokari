// Stage 5 S5-1: generalised modulation matrix. Macro routes gain a
// source selector: besides the parent macro value (the original
// behaviour), a route can be driven by the target track's LFO,
// pressure, voice envelope or last-note velocity. Dynamic sources are
// evaluated from a ModRuntime snapshot the engine supplies at the
// control boundary; static contexts pass no snapshot and dynamic
// routes stay inert there. The source rides in bits 4-6 of the MACR
// route byte that older files always wrote as zero, so old projects
// decode every route as Macro-sourced.
#include "performance.h"
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
bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
uint32_t kind4(const char* s) { return uint32_t(uint8_t(s[0])) | (uint32_t(uint8_t(s[1])) << 8) | (uint32_t(uint8_t(s[2])) << 16) | (uint32_t(uint8_t(s[3])) << 24); }
struct TChunk { uint32_t kind = 0; uint16_t version = 1; std::vector<uint8_t> payload; };
std::vector<TChunk> parse_chunks(const uint8_t* p, size_t n) {
 std::vector<TChunk> out; size_t at = 0;
 while (at < n) { assert(at + 12 <= n); TChunk c; c.kind = uint32_t(p[at]) | (uint32_t(p[at + 1]) << 8) | (uint32_t(p[at + 2]) << 16) | (uint32_t(p[at + 3]) << 24); c.version = uint16_t(p[at + 4] | (p[at + 5] << 8)); uint32_t len = uint32_t(p[at + 8]) | (uint32_t(p[at + 9]) << 8) | (uint32_t(p[at + 10]) << 16) | (uint32_t(p[at + 11]) << 24); assert(at + 12 + len <= n); c.payload.assign(p + at + 12, p + at + 12 + len); out.push_back(std::move(c)); at += 12 + len; }
 assert(at == n); return out;
}
void append_chunks(std::vector<uint8_t>& out, const std::vector<TChunk>& chunks) {
 for (const auto& c : chunks) { out.push_back(uint8_t(c.kind)); out.push_back(uint8_t(c.kind >> 8)); out.push_back(uint8_t(c.kind >> 16)); out.push_back(uint8_t(c.kind >> 24)); out.push_back(uint8_t(c.version)); out.push_back(uint8_t(c.version >> 8)); out.push_back(0); out.push_back(0); uint32_t len = uint32_t(c.payload.size()); out.push_back(uint8_t(len)); out.push_back(uint8_t(len >> 8)); out.push_back(uint8_t(len >> 16)); out.push_back(uint8_t(len >> 24)); out.insert(out.end(), c.payload.begin(), c.payload.end()); }
}
std::vector<uint8_t> make_file(const std::vector<TChunk>& chunks) {
 std::vector<uint8_t> stream; append_chunks(stream, chunks);
 std::vector<uint8_t> file = {'M', 'G', 'X', 'C', uint8_t(kFormatVersion), 0};
 uint32_t len = uint32_t(stream.size()); file.push_back(uint8_t(len)); file.push_back(uint8_t(len >> 8)); file.push_back(uint8_t(len >> 16)); file.push_back(uint8_t(len >> 24));
 file.insert(file.end(), stream.begin(), stream.end());
 ProjectCrc crc; for (size_t i = 4; i < file.size(); ++i) crc.add(file[i]); uint32_t sum = crc.value();
 file.push_back(uint8_t(sum)); file.push_back(uint8_t(sum >> 8)); file.push_back(uint8_t(sum >> 16)); file.push_back(uint8_t(sum >> 24));
 return file;
}
std::vector<TChunk> parse_file(const std::vector<uint8_t>& file) {
 assert(file.size() > 14 && file[0] == 'M' && file[1] == 'G' && file[2] == 'X' && file[3] == 'C');
 return parse_chunks(file.data() + 10, file.size() - 14);
}
// Mode-byte offsets of every route inside the MACR payload: per macro
// u8 active + f32 value, then routes with a variable-sized scoped target (mode byte last).
std::vector<size_t> macr_mode_offsets(const std::vector<uint8_t>& payload) {
 std::vector<size_t> out; size_t at = 0;
 for (unsigned m = 0; m < 8; ++m) { at += 5; for (unsigned r = 0; r < kMacroRoutes; ++r) { unsigned guid=payload.at(at+1);size_t size=15+guid;out.push_back(at+size-1);at+=size; } }
 assert(at <= payload.size()); return out;
}
float resolve_cutoff(const ProjectState& p, const ModRuntime* mod) {
 return resolve_parameter(p, track_at(p,0,0), 0, 3, nullptr, 0, false, 0, 0, nullptr, true, nullptr, 255, nullptr, nullptr, mod).value;
}
} // namespace
int main() {
 // ---- Resolver: dynamic sources drive the route ----
 {
  ProjectState p; make_empty_project(p);
  auto track = track_at(p,0,0); track.cutoff = 0.8f;
  p.macros[0].active = true;
  auto& route = p.macros[0].routes[0];
  route = {true, 3, 0x0001, 0.2f, 0.6f, LayerMode::Replace, MacroCurve::Linear, ModSource::Lfo};
  ModRuntime mod{};
  mod.lfo = 0; assert(near(resolve_cutoff(p, &mod), 0.2f));
  mod.lfo = 1; assert(near(resolve_cutoff(p, &mod), 0.6f));
  mod.lfo = 0.5f; assert(near(resolve_cutoff(p, &mod), 0.4f));
  // The route reports the macro source flag like a macro route.
  assert(resolve_parameter(p, track, 0, 3, nullptr, 0, false, 0, 0, nullptr, true, nullptr, 255, nullptr, nullptr, &mod).sources & FromMacro);
  // No runtime context: the dynamic route is inert.
  assert(near(resolve_cutoff(p, nullptr), 0.8f));
  // Bank not engaged: inert even with a context.
  p.macros[0].active = false; assert(near(resolve_cutoff(p, &mod), 0.8f)); p.macros[0].active = true;
  // Track mask excludes this track: inert.
  route.tracks = 0x0002; assert(near(resolve_cutoff(p, &mod), 0.8f)); route.tracks = 0x0001;
  // Each dynamic source reads its own snapshot field.
  route.source = ModSource::Pressure; mod = {}; mod.pressure = 0.25f; assert(near(resolve_cutoff(p, &mod), 0.3f));
  route.source = ModSource::Envelope; mod = {}; mod.envelope = 0.75f; assert(near(resolve_cutoff(p, &mod), 0.5f));
  route.source = ModSource::Velocity; route.curve = MacroCurve::Smooth; mod = {}; mod.velocity = 0.25f;
  assert(near(resolve_cutoff(p, &mod), 0.2f + 0.4f * 0.15625f)); // smooth(0.25)
  // Macro source keeps the original behaviour (parent macro value).
  route.source = ModSource::Macro; route.curve = MacroCurve::Linear; p.macros[0].value = 0.25f;
  assert(near(resolve_cutoff(p, &mod), 0.3f));
  assert(near(resolve_cutoff(p, nullptr), 0.3f)); // macro routes need no context
  // Add layering composes on the base value.
  route.source = ModSource::Lfo; route.mode = LayerMode::Add; route.low = 0; route.high = 0.1f; mod = {}; mod.lfo = 1;
  assert(near(resolve_cutoff(p, &mod), 0.9f));
 }
 // ---- Validation ----
 {
  ProjectState p; make_empty_project(p); assert(valid_project(p));
  p.macros[2].routes[3].source = ModSource::Velocity; assert(valid_project(p));
  p.macros[2].routes[3].source = ModSource(9); assert(!valid_project(p));
 }
 // ---- Codec: source survives a round trip; old files read as Macro ----
 {
  auto in = std::make_unique<ProjectState>(); make_empty_project(*in);
  in->macros[3].routes[5].source = ModSource::Velocity;
  in->macros[3].routes[0].source = ModSource::Lfo;
  in->macros[0].routes[15].source = ModSource::Envelope;
  Buffer sink; assert(encode_project(sink, *in));
  auto out = std::make_unique<ProjectState>(); Buffer source{sink.bytes}; assert(decode_project(source, *out));
  assert(out->macros[3].routes[5].source == ModSource::Velocity);
  assert(out->macros[3].routes[0].source == ModSource::Lfo);
  assert(out->macros[0].routes[15].source == ModSource::Envelope);
  assert(out->macros[0].routes[0].source == ModSource::Macro);
  Buffer again; assert(encode_project(again, *out)); assert(again.bytes == sink.bytes);
  // Current format with zero source bits means Macro (no legacy decoder).
  auto chunks = parse_file(sink.bytes);
  bool found = false;
  for (auto& c : chunks) if (c.kind == kind4("MACR")) { found = true; for (size_t off : macr_mode_offsets(c.payload)) c.payload[off] &= 0x0f; }
  assert(found);
  auto legacy = make_file(chunks);
  auto old = std::make_unique<ProjectState>(); Buffer legacy_source{legacy}; assert(decode_project(legacy_source, *old));
  for (const auto& macro : old->macros) for (const auto& route : macro.routes) assert(route.source == ModSource::Macro);
  // Out-of-range source bits are rejected.
  auto bad_chunks = parse_file(sink.bytes);
  for (auto& c : bad_chunks) if (c.kind == kind4("MACR")) c.payload[macr_mode_offsets(c.payload)[0]] |= 0x70;
  auto bad = make_file(bad_chunks);
  auto rejected = std::make_unique<ProjectState>(); Buffer bad_source{bad}; assert(!decode_project(bad_source, *rejected));
 }
 // ---- Engine: pressure drives a route end to end ----
 {
  ProjectState p; make_empty_project(p);
  auto track = track_at(p,0,0);
  track.instrument = Instrument::PolySaw; track.cutoff = 0.9f; track.pressure = 127;
  track.steps[0].enabled = true; track.steps[0].note = 60;
  p.macros[0].active = true;
  p.macros[0].routes[0] = {true, 3, 0x0001, 0.1f, 0.4f, LayerMode::Replace, MacroCurve::Linear, ModSource::Pressure};
  Engine engine(48000); assert(engine.stage_project(p));
  assert(engine.submit({CommandType::Start, 0, 0, 0}));
  float buf[512]; AudioVisualFrame frame; float cutoff = -1;
  for (unsigned i = 0; i < 200; ++i) { engine.render(buf, 256); while (engine.pop_visual(frame)) cutoff = frame.track_parameters[0][2]; }
  assert(cutoff > 0.34f && cutoff < 0.46f); // settled at the route high end
 }
 // ---- Engine: velocity is captured at trigger time ----
 {
  ProjectState p; make_empty_project(p);
  auto track = track_at(p,0,0);
  track.instrument = Instrument::PolySaw; track.cutoff = 0.5f;
  p.macros[0].active = true;
  p.macros[0].routes[0] = {true, 3, 0x0001, 0.0f, 1.0f, LayerMode::Replace, MacroCurve::Linear, ModSource::Velocity};
  Engine engine(48000); assert(engine.stage_project(p));
  float buf[512]; AudioVisualFrame frame; float cutoff = -1;
  auto pump = [&](unsigned n) { for (unsigned i = 0; i < n; ++i) { engine.render(buf, 256); while (engine.pop_visual(frame)) cutoff = frame.track_parameters[0][2]; } };
  pump(40); assert(cutoff >= 0 && cutoff < 0.1f); // no note yet: velocity 0
  Command trigger{CommandType::Trigger, 0, 0, 127}; trigger.parameter = 1; trigger.scalar = 60;
  assert(engine.submit(trigger));
  pump(40); assert(cutoff > 0.9f); // full-velocity hit drives the route high
 }
 // ---- Engine: LFO sweeps the target across its range ----
 {
  ProjectState p; make_empty_project(p);
  auto track = track_at(p,0,0);
  track.instrument = Instrument::PolySaw; track.cutoff = 0.5f; track.lfo_rate = 8;
  track.steps[0].enabled = true; track.steps[0].note = 60;
  p.macros[0].active = true;
  p.macros[0].routes[0] = {true, 3, 0x0001, 0.0f, 1.0f, LayerMode::Replace, MacroCurve::Linear, ModSource::Lfo};
  Engine engine(48000); assert(engine.stage_project(p));
  assert(engine.submit({CommandType::Start, 0, 0, 0}));
  float buf[512]; AudioVisualFrame frame; float lo = 1, hi = 0;
  for (unsigned i = 0; i < 200; ++i) { engine.render(buf, 256); while (engine.pop_visual(frame)) { float v = frame.track_parameters[0][2]; if (v < lo) lo = v; if (v > hi) hi = v; } }
  assert(lo < 0.2f && hi > 0.8f);
 }
 std::puts("mod matrix: generalised sources, codec compat and engine paths verified");
}
