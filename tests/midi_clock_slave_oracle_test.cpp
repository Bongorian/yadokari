// Independent oracle for YADOKARI's phase-locked MIDI clock slave (0.9.3).
//
// The upstream acceptance test reads the engine's own phase-error counter
// (Engine::slave_clock_state). This harness ignores that counter and judges
// what a listener / a downstream device would actually get: the sample
// stamps of the note-ons the sequencer emits, compared with the master's
// own sixteenth-note grid (every 6th clock tick, the first 0xF8 after Start
// being tick 0, as the MIDI 1.0 spec and YADOKARI's own master mode do).
//
// Output per scenario: mean / p99 / max |offset| of sixteenth notes in ms,
// plus the engine's self-reported counters for comparison.
#include "engine.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>
using namespace groove;

static int failures = 0;
#define CHECK(cond, ...) do { if(!(cond)) { ++failures; std::fprintf(stderr, "  FAIL: "); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } } while(0)

namespace {
bool g_ci = false; // --ci: 16 kHz, runs capped at 30 s (60 s for the long run)
struct Rig {
 unsigned rate;
 std::unique_ptr<ProjectState> project;
 std::unique_ptr<Engine> engine;
 std::vector<float> buf;
 uint64_t rendered = 0;
 std::vector<uint64_t> note_on;   // sample stamps of emitted note-ons
 Rig(unsigned rate_, unsigned tempo) : rate(rate_), buf(8192, 0.0f) {
  project = std::make_unique<ProjectState>();
  make_empty_project(*project);
  project->tempo = uint16_t(tempo);
  project->midi_clock_slave = true;
  auto track = track_at(*project, 0, 0);
  track.instrument = Instrument::ExternalMidi;
  for(unsigned s = 0; s < 16; ++s) track.steps[s].enabled = true; // every sixteenth
  project->clips[0] = {0, true};
  engine = std::make_unique<Engine>(rate);
  assert(engine->stage_project(*project));
  engine->render(buf.data(), 0);
 }
 void drain() {
  MidiEvent e{};
  while(engine->pop_midi(e))
   if((e.bytes[0] & 0xf0) == 0x90 && e.bytes[2]) note_on.push_back(e.sample);
 }
 void render_to(uint64_t target) {
  while(rendered < target) {
   size_t n = size_t(std::min<uint64_t>(target - rendered, 512));
   engine->render(buf.data(), n);
   rendered += n;
   drain();
  }
 }
 void send(uint64_t stamp, uint8_t status) {
  if(stamp > rendered) render_to(stamp);
  MidiPacket p{};
  p.sample = stamp; p.length = 1; p.bytes[0] = status;
  assert(engine->receive_midi(p, 0));
  if(stamp >= rendered) render_to(stamp + 1);
 }
};

struct Master {
 // Absolute sample stamp (as a double) of every tick the master *intended*
 // to send, i.e. its true grid; `sent` marks ticks that actually went out.
 std::vector<double> tick_time;
 std::vector<bool> sent;
};

struct Result { double mean_ms, p99_ms, max_ms, final_ms; size_t notes; };

Result judge(const Rig& rig, const Master& m, double from_s, double to_s) {
 // For every emitted note find the nearest master sixteenth (tick 6k).
 std::vector<double> err;
 double last = 0;
 for(uint64_t stamp : rig.note_on) {
  double t = double(stamp);
  if(t < from_s * rig.rate || t > to_s * rig.rate) continue;
  auto it = std::lower_bound(m.tick_time.begin(), m.tick_time.end(), t);
  double best = 1e18;
  for(int d = -12; d <= 12; ++d) {
   long idx = long(it - m.tick_time.begin()) + d;
   if(idx < 0 || idx >= long(m.tick_time.size()) || idx % 6) continue;
   double e = t - m.tick_time[size_t(idx)];
   if(std::fabs(e) < std::fabs(best)) best = e;
  }
  if(best > 1e17) continue;
  last = best * 1000.0 / rig.rate;
  err.push_back(std::fabs(last));
 }
 Result r{0, 0, 0, last, err.size()};
 if(err.empty()) return r;
 double sum = 0; for(double e : err) sum += e;
 r.mean_ms = sum / double(err.size());
 std::vector<double> sorted = err; std::sort(sorted.begin(), sorted.end());
 r.p99_ms = sorted[std::min(sorted.size() - 1, size_t(double(sorted.size()) * 0.99))];
 r.max_ms = sorted.back();
 return r;
}

// Run a master schedule into a fresh slave. `bpm_at(sec)` is the master's
// tempo; `jitter_ms` uniform transport jitter; `drop(sec)` true = tick lost;
// `start_to_first_tick` delay between 0xFA and the first 0xF8, in ticks.
struct Scenario {
 const char* name;
 unsigned rate = 48000;
 double seconds = 60;
 std::function<double(double)> bpm_at = [](double) { return 120.0; };
 double jitter_ms = 0;
 std::function<bool(double)> drop = [](double) { return false; };
 double start_to_first_tick = 0;
 double judge_from = 4.0;       // seconds of pull-in excluded from stats
 double limit_ms = 2.0;         // pass bar for p99 |offset|
 unsigned project_tempo = 120;  // tempo saved in the slave's project
};

Result run(const Scenario& in, Engine::SlaveClockState* out_state = nullptr) {
 Scenario sc = in;
 if(g_ci) { if(sc.rate == 48000) sc.rate = 16000; sc.seconds = std::min(sc.seconds, sc.seconds > 100 ? 60.0 : 30.0); }
 Rig rig(sc.rate, sc.project_tempo);
 Master m;
 const double t0 = 0.05 * sc.rate; // Start after 50 ms
 rig.send(uint64_t(t0), 0xfa);
 double t = t0 + sc.start_to_first_tick * double(sc.rate) * 60.0 / (24.0 * sc.bpm_at(0));
 uint32_t seed = 0x2545f491u;
 const double end = sc.seconds * sc.rate;
 while(t < end) {
  double sec = (t - t0) / sc.rate;
  bool lost = sc.drop(sec);
  m.tick_time.push_back(t);
  m.sent.push_back(!lost);
  if(!lost) {
   double jit = 0;
   if(sc.jitter_ms > 0) {
    seed = seed * 1664525u + 1013904223u;
    jit = ((double(seed >> 8) / double(1u << 24)) * 2.0 - 1.0) * sc.jitter_ms * sc.rate / 1000.0;
   }
   uint64_t stamp = uint64_t(std::llround(std::max(t + jit, double(rig.rendered))));
   rig.send(stamp, 0xf8);
  }
  t += double(sc.rate) * 60.0 / (24.0 * sc.bpm_at(sec));
 }
 rig.render_to(uint64_t(end) + sc.rate / 10);
 Result r = judge(rig, m, sc.judge_from, sc.seconds - 0.5);
 auto s = rig.engine->slave_clock_state();
 if(out_state) *out_state = s;
 std::printf("%-34s notes %5zu | mean %6.3f  p99 %6.3f  max %7.3f  last %+7.3f ms | engine: max %4d mt, resyncs %u, %s, %.3f bpm\n",
             sc.name, r.notes, r.mean_ms, r.p99_ms, r.max_ms, r.final_ms,
             s.max_phase_error_milliticks, s.resyncs, s.locked ? "locked" : "UNLOCKED",
             double(s.applied_tempo_q16) / 65536.0);
 CHECK(r.notes > 0, "%s: no notes emitted", sc.name);
 CHECK(r.p99_ms <= sc.limit_ms, "%s: p99 offset %.3f ms > %.3f ms", sc.name, r.p99_ms, sc.limit_ms);
 return r;
}
} // namespace

int main(int argc, char** argv) {
 bool quick = argc > 1 && std::string(argv[1]) == "--quick";
 g_ci = argc > 1 && std::string(argv[1]) == "--ci";
 if(argc > 1 && std::string(argv[1]) == "--sweep") {
  static char names[64][48]; int n = 0;
  for(unsigned proj : {120u, 60u}) for(double bpm : {30.0, 40.0, 45.0, 50.0, 60.0, 61.0, 70.0, 80.0, 240.0}) {
   std::snprintf(names[n], 48, "project %u, master %.0f", proj, bpm);
   Scenario s{names[n++]}; s.seconds = 24; s.judge_from = 8; s.project_tempo = proj;
   s.bpm_at = [bpm](double) { return bpm; }; run(s);
  }
  failures = 0; return 0;
 }
 double long_run = (quick || g_ci) ? 120 : 600;
 std::printf("Oracle: |note-on - master sixteenth| in ms, %s\n", g_ci ? "CI mode (16 kHz, shortened runs)" : "48 kHz unless noted");

 { Scenario s{"steady 120.0"}; run(s); }
 { Scenario s{"steady 120.4 (fractional)"}; s.seconds = long_run; s.bpm_at = [](double) { return 120.4; }; run(s); }
 { Scenario s{"steady 120.4 @ 8 kHz (upstream rate)"}; s.rate = 8000; s.bpm_at = [](double) { return 120.4; }; run(s); }
 { Scenario s{"steady 87.65"}; s.bpm_at = [](double) { return 87.65; }; run(s); }
 { Scenario s{"steady 174.3"}; s.bpm_at = [](double) { return 174.3; }; run(s); }
 { Scenario s{"slow 30 BPM"}; s.bpm_at = [](double) { return 30.0; }; s.judge_from = 10; run(s); }
 { Scenario s{"fast 299 BPM"}; s.bpm_at = [](double) { return 299.0; }; run(s); }
 // Start-up with a master far slower than the project's saved tempo: the
 // first interval runs at the project tempo and must not be mistaken for
 // lost ticks (0.9.3 locked one or more whole ticks off here).
 { Scenario s{"master 60, project 120"}; s.seconds = 30; s.bpm_at = [](double) { return 60.0; }; s.judge_from = 6; run(s); }
 { Scenario s{"master 70, project 120"}; s.seconds = 30; s.bpm_at = [](double) { return 70.0; }; s.judge_from = 6; run(s); }
 { Scenario s{"master 30, project 60"}; s.seconds = 30; s.project_tempo = 60; s.bpm_at = [](double) { return 30.0; }; s.judge_from = 10; run(s); }
 { Scenario s{"jitter +/-1 ms @120.4"}; s.bpm_at = [](double) { return 120.4; }; s.jitter_ms = 1.0; s.limit_ms = 3.0; run(s); }
 { Scenario s{"jitter +/-3 ms @120.4 (cheap USB)"}; s.bpm_at = [](double) { return 120.4; }; s.jitter_ms = 3.0; s.limit_ms = 6.0; run(s); }
 { Scenario s{"ramp 120->140 over 20 s"}; s.seconds = 40; s.bpm_at = [](double x) { return x < 20 ? 120 + x : 140.0; }; run(s); }
 { Scenario s{"ramp 140->90 over 10 s"}; s.seconds = 30; s.bpm_at = [](double x) { return x < 10 ? 140 - 5 * x : 90.0; }; s.limit_ms = 3.0; run(s); }
 { Scenario s{"step 120->128 instant"}; s.seconds = 30; s.bpm_at = [](double x) { return x < 10 ? 120.0 : 128.0; }; s.judge_from = 4; s.limit_ms = 30; run(s); }
 { Scenario s{"dropout 2 s @120.4"}; s.seconds = 30; s.bpm_at = [](double) { return 120.4; }; s.drop = [](double x) { return x > 15 && x < 17; }; run(s); }
 { Scenario s{"1 of 100 ticks lost @120.4"}; s.bpm_at = [](double) { return 120.4; };
   s.drop = [](double x) { return std::fmod(x * 48.16, 100.0) < 1.0; }; s.limit_ms = 5.0; run(s); }
 // Masters that send the first 0xF8 one tick after 0xFA (some old
 // hardware does this): the oracle shows the constant offset the
 // self-reported counter cannot see.
 { Scenario s{"first F8 one tick after FA"}; s.start_to_first_tick = 1.0; s.limit_ms = 25; run(s); }

 if(failures) { std::fprintf(stderr, "slave_oracle_test: %d failures\n", failures); return 1; }
 std::printf("slave_oracle_test: all scenarios within limits\n");
 return 0;
}
