// Acceptance tests for the phase-locked external clock slave (Issue #1).
// The five conditions are the fixed acceptance bar from the design record:
//  1. 120.4 BPM master for 10 minutes: phase error bounded below 1 tick.
//  2. +/-1 ms tick jitter: lock is maintained.
//  3. Tempo ramp 120 -> 140 BPM: follows without losing lock.
//  4. 2 s clock dropout: free-runs and re-locks without a jump (no resync);
//     a tempo change across a dropout, and a >half-sixteenth phase jump,
//     take the hard-resync path and re-lock.
//  5. Master mode output is unchanged (legacy integer-accumulator oracle).
// Phase error is read through Engine::slave_clock_state(), the same
// snapshot the GBX1 probe/log path reads.
#include "engine.h"
#include "midi_clock_estimator.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;

static int failures = 0;
#define CHECK(cond, ...) do { if(!(cond)) { ++failures; std::fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } } while(0)

namespace {
constexpr unsigned kRate = 8000; // host-test rate, as in backends_test

struct Rig {
 std::unique_ptr<ProjectState> project;
 std::unique_ptr<Engine> engine;
 std::vector<float> buf;
 uint64_t rendered = 0;
 Rig(unsigned tempo, bool slave, bool play_track0 = false) : buf(8192, 0.0f) {
  project = std::make_unique<ProjectState>();
  make_empty_project(*project);
  project->tempo = uint16_t(tempo);
  project->midi_clock_slave = slave;
  if(play_track0) {
   auto track = track_at(*project, 0, 0);
   track.instrument = Instrument::ExternalMidi;
   track.steps[0].enabled = true;
   track.steps[4].enabled = true;
   project->clips[0] = {0, true};
  }
  engine = std::make_unique<Engine>(kRate);
  assert(engine->stage_project(*project));
  engine->render(buf.data(), 0); // apply the staged snapshot
 }
 void render_to(uint64_t target) {
  while(rendered < target) {
   size_t n = size_t(std::min<uint64_t>(target - rendered, 2048));
   engine->render(buf.data(), n);
   rendered += n;
  }
 }
 // Deliver a one-byte real-time message at an exact sample. Stamp 0 goes
 // through the immediate submit path, like a message arriving right now.
 void send(uint64_t stamp, uint8_t status) {
  if(stamp > rendered) render_to(stamp);
  MidiPacket packet{};
  packet.sample = stamp;
  packet.length = 1;
  packet.bytes[0] = status;
  assert(engine->receive_midi(packet, 0));
  if(stamp >= rendered) render_to(stamp + 1); // process at the stamped sample
 }
 void start() { send(0, 0xfa); }
 Engine::SlaveClockState state() const { return engine->slave_clock_state(); }
};

double tick_period(double bpm) { return double(kRate) * 60.0 / (24.0 * bpm); }
uint64_t stamp_of(uint64_t tick, double bpm) { return uint64_t(std::llround(double(tick) * tick_period(bpm))); }

// Feed constant-tempo ticks (optionally jittered) up to a musical time.
void feed_constant(Rig& rig, double bpm, double seconds, int jitter_samples = 0, uint32_t seed = 1) {
 uint64_t limit = uint64_t(seconds * kRate);
 for(uint64_t tick = 1;; ++tick) {
  uint64_t stamp = stamp_of(tick, bpm);
  if(stamp > limit) break;
  if(jitter_samples) {
   seed = seed * 1664525u + 1013904223u;
   int jitter = int((seed >> 9) % uint32_t(2 * jitter_samples + 1)) - jitter_samples;
   stamp = uint64_t(int64_t(stamp) + jitter);
  }
  rig.send(stamp, 0xf8);
 }
 rig.render_to(limit + 2 * uint64_t(tick_period(bpm)));
}

void test_estimator_fractional() {
 MidiClockEstimator estimate, legacy_estimator;
 uint32_t q16 = 0;
 unsigned legacy = 0;
 for(uint64_t n = 0; n <= 24; ++n) {
  uint64_t stamp = stamp_of(n, 120.4);
  estimate.observe_q16(stamp, kRate, q16);
  legacy_estimator.observe(stamp, kRate, legacy);
 }
 double bpm = double(q16) / 65536.0;
 std::printf("estimator: q16 bpm %.4f legacy %u\n", bpm, legacy);
 CHECK(std::fabs(bpm - 120.4) < 0.05, "fractional estimate %.4f not within 0.05 of 120.4", bpm);
 CHECK(legacy == 120, "legacy integer estimate %u != 120", legacy);
}

void test_lock_10_minutes() {
 Rig rig(120, true);
 rig.start();
 feed_constant(rig, 120.4, 600.0);
 auto s = rig.state();
 double applied = double(s.applied_tempo_q16) / 65536.0;
 std::printf("lock10min: max %d mt final %d mt resyncs %u applied %.4f bpm locked %d project tempo %u\n",
             s.max_phase_error_milliticks, s.phase_error_milliticks, s.resyncs, applied, int(s.locked),
             unsigned(rig.project->tempo));
 CHECK(s.resyncs == 0, "unexpected hard resyncs: %u", s.resyncs);
 CHECK(s.max_phase_error_milliticks < 1000, "phase error not bounded below 1 tick: max %d mt", s.max_phase_error_milliticks);
 CHECK(s.locked, "not locked at end of 10 minutes");
 CHECK(std::abs(s.phase_error_milliticks) < 250, "final phase error %d mt", s.phase_error_milliticks);
 CHECK(std::fabs(applied - 120.4) < 0.1, "applied tempo %.4f not within 0.1 of 120.4", applied);
 CHECK(rig.project->tempo == 120, "display/save tempo must stay integer 120, got %u", unsigned(rig.project->tempo));
}

void test_jitter() {
 Rig rig(120, true);
 rig.start();
 feed_constant(rig, 120.4, 60.0, 8, 12345); // +/-1 ms at 8 kHz = +/-8 samples
 auto s = rig.state();
 std::printf("jitter: max %d mt final %d mt resyncs %u locked %d\n",
             s.max_phase_error_milliticks, s.phase_error_milliticks, s.resyncs, int(s.locked));
 CHECK(s.resyncs == 0, "jitter caused resyncs: %u", s.resyncs);
 CHECK(s.max_phase_error_milliticks < 1000, "jitter: max phase error %d mt", s.max_phase_error_milliticks);
 CHECK(s.locked, "jitter: not locked at end");
}

void test_ramp() {
 Rig rig(120, true);
 rig.start();
 // Integrate tick times against a 20 s linear ramp 120 -> 140, then hold.
 double t = 0, stamp_f = 0;
 uint64_t tick = 0;
 auto bpm_at = [](double seconds) { return seconds < 20.0 ? 120.0 + seconds : 140.0; };
 while(t < 35.0) {
  stamp_f += tick_period(bpm_at(t));
  t = stamp_f / kRate;
  rig.send(uint64_t(std::llround(stamp_f)), 0xf8);
  ++tick;
 }
 rig.render_to(uint64_t(37 * kRate));
 auto s = rig.state();
 double applied = double(s.applied_tempo_q16) / 65536.0;
 std::printf("ramp: ticks %llu max %d mt final %d mt resyncs %u applied %.4f bpm locked %d\n",
             (unsigned long long)tick, s.max_phase_error_milliticks, s.phase_error_milliticks,
             s.resyncs, applied, int(s.locked));
 CHECK(s.resyncs == 0, "ramp caused resyncs: %u", s.resyncs);
 CHECK(s.max_phase_error_milliticks < 1000, "ramp: max phase error %d mt", s.max_phase_error_milliticks);
 CHECK(std::fabs(applied - 140.0) < 0.5, "ramp: applied tempo %.4f not within 0.5 of 140", applied);
 CHECK(s.locked, "ramp: not locked at end");
}

void test_dropout() {
 Rig rig(120, true);
 rig.start();
 feed_constant(rig, 120.4, 15.0);
 auto before = rig.state();
 // Two seconds of master time pass with no clock bytes at all. The master
 // keeps playing, so its tick schedule does not move; the slave free-runs.
 uint64_t gap_begin = uint64_t(15.0 * kRate), gap_end = uint64_t(17.0 * kRate);
 rig.render_to(gap_end);
 auto during = rig.state();
 for(uint64_t tick = 1;; ++tick) {
  uint64_t stamp = stamp_of(tick, 120.4);
  if(stamp <= gap_end) continue;
  if(stamp > uint64_t(22.0 * kRate)) break;
  rig.send(stamp, 0xf8);
 }
 rig.render_to(uint64_t(22.5 * kRate));
 auto s = rig.state();
 std::printf("dropout: before max %d mt, gap applied %.4f -> %.4f bpm, after max %d mt final %d mt resyncs %u locked %d\n",
             before.max_phase_error_milliticks, double(before.applied_tempo_q16) / 65536.0,
             double(during.applied_tempo_q16) / 65536.0, s.max_phase_error_milliticks,
             s.phase_error_milliticks, s.resyncs, int(s.locked));
 CHECK(during.applied_tempo_q16 == before.applied_tempo_q16, "free-run tempo moved during dropout");
 CHECK(s.resyncs == 0, "dropout re-lock jumped (resyncs %u)", s.resyncs);
 CHECK(s.max_phase_error_milliticks < 1000, "dropout: max phase error %d mt", s.max_phase_error_milliticks);
 CHECK(s.locked, "dropout: not re-locked");
 CHECK(std::abs(s.phase_error_milliticks) < 250, "dropout: final phase error %d mt", s.phase_error_milliticks);
}

void test_phase_jump_resync() {
 Rig rig(120, true);
 rig.start();
 // Feed without feed_constant's render tail, so the extras below follow
 // the last real tick within one period (no gap inference in between).
 for(uint64_t tick = 1;; ++tick) {
  uint64_t stamp = stamp_of(tick, 120.4);
  if(stamp > uint64_t(10.0 * kRate)) break;
  rig.send(stamp, 0xf8);
 }
 // Four spurious extra clock bytes: the counted master position jumps 4
 // ticks ahead of the internal position (a >half-sixteenth error), which
 // must take the hard-resync path exactly once and pull back into lock.
 uint64_t base = rig.rendered;
 for(uint64_t i = 1; i <= 4; ++i) rig.send(base + 20 * i, 0xf8);
 for(uint64_t tick = 1;; ++tick) {
  uint64_t stamp = stamp_of(tick, 120.4);
  if(stamp <= base + 80) continue; // stay ahead of the injected extras
  if(stamp > uint64_t(16.0 * kRate)) break;
  rig.send(stamp, 0xf8);
 }
 rig.render_to(uint64_t(17.0 * kRate));
 auto s = rig.state();
 std::printf("jump: resyncs %u final %d mt locked %d\n", s.resyncs, s.phase_error_milliticks, int(s.locked));
 CHECK(s.resyncs == 1, "expected exactly one hard resync, got %u", s.resyncs);
 CHECK(s.locked, "jump: not re-locked after resync");
 CHECK(std::abs(s.phase_error_milliticks) < 250, "jump: final phase error %d mt", s.phase_error_milliticks);
}

void test_dropout_tempo_change_resync() {
 Rig rig(120, true);
 rig.start();
 feed_constant(rig, 120.4, 10.0);
 // Two seconds of silence, and the master changes tempo to 150 BPM during
 // the gap. Whole-tick inference cannot absorb a tempo change: the drift
 // must take the resync path and re-lock at the new tempo.
 uint64_t gap_end = uint64_t(12.0 * kRate);
 rig.render_to(gap_end);
 for(uint64_t k = 1;; ++k) {
  uint64_t stamp = gap_end + uint64_t(std::llround(double(k) * tick_period(150.0)));
  if(stamp > uint64_t(22.0 * kRate)) break;
  rig.send(stamp, 0xf8);
 }
 rig.render_to(uint64_t(22.5 * kRate));
 auto s = rig.state();
 double applied = double(s.applied_tempo_q16) / 65536.0;
 std::printf("dropout-tempo: resyncs %u final %d mt applied %.4f bpm locked %d\n",
             s.resyncs, s.phase_error_milliticks, applied, int(s.locked));
 CHECK(s.resyncs >= 1, "tempo change across dropout caused no resync");
 CHECK(std::fabs(applied - 150.0) < 1.5, "applied tempo %.4f not within 1.5 of 150", applied);
 CHECK(s.locked, "dropout-tempo: not re-locked");
 CHECK(std::abs(s.phase_error_milliticks) < 250, "dropout-tempo: final phase error %d mt", s.phase_error_milliticks);
}

void test_spp_continue_anchor() {
 Rig rig(120, true);
 // SPP to sixteenth 5, then Continue, then a 120.4 clock: must anchor on
 // the SPP/Continue pair with no resync and lock.
 MidiPacket spp{};
 spp.sample = 100; spp.length = 3; spp.bytes[0] = 0xf2; spp.bytes[1] = 5; spp.bytes[2] = 0;
 rig.render_to(100);
 assert(rig.engine->receive_midi(spp, 0));
 rig.send(200, 0xfb);
 for(uint64_t tick = 1;; ++tick) {
  uint64_t stamp = 200 + stamp_of(tick, 120.4);
  if(stamp > uint64_t(8.0 * kRate)) break;
  rig.send(stamp, 0xf8);
 }
 rig.render_to(uint64_t(9.0 * kRate));
 auto s = rig.state();
 std::printf("spp: anchored %d resyncs %u final %d mt locked %d\n",
             int(s.anchored), s.resyncs, s.phase_error_milliticks, int(s.locked));
 CHECK(s.anchored, "SPP/Continue did not anchor the slave");
 CHECK(s.resyncs == 0, "SPP/Continue caused resyncs: %u", s.resyncs);
 CHECK(s.locked, "SPP: not locked");
}

void test_master_unchanged() {
 // Legacy oracle: the pre-change integer accumulator at 120 BPM, 8 kHz.
 std::vector<uint64_t> legacy_ticks{0}; // first_ tick at the Start sample
 {
  uint64_t phase = 0;
  for(uint64_t s = 1; s < 40000; ++s) {
   phase += 120 * 24;
   if(phase >= 480000) { phase -= 480000; legacy_ticks.push_back(s); }
  }
 }
 Rig rig(120, false, true);
 assert(rig.engine->submit({CommandType::Start, 0, 0, 0}));
 rig.render_to(40000);
 std::vector<uint64_t> ticks, notes;
 MidiEvent event{};
 while(rig.engine->pop_midi(event)) {
  if(event.bytes[0] == 0xf8) ticks.push_back(event.sample);
  if((event.bytes[0] & 0xf0) == 0x90 && event.bytes[2]) notes.push_back(event.sample);
 }
 std::printf("master: %zu clock ticks, %zu note-ons\n", ticks.size(), notes.size());
 CHECK(ticks == legacy_ticks, "master clock tick stamps differ from the legacy accumulator");
 // Step period is exactly 1000 samples at 120 BPM / 8 kHz; steps 0 and 4 of
 // a 16-step track fire at 0/4000 and repeat every 16000 samples.
 std::vector<uint64_t> legacy_notes;
 for(uint64_t base = 0; base + 4000 <= 40000; base += 16000) { legacy_notes.push_back(base); legacy_notes.push_back(base + 4000); }
 CHECK(notes == legacy_notes, "master step note stamps differ from the legacy grid");
}
} // namespace

int main() {
 test_estimator_fractional();
 test_master_unchanged();
 test_spp_continue_anchor();
 test_jitter();
 test_ramp();
 test_dropout();
 test_phase_jump_resync();
 test_dropout_tempo_change_resync();
 test_lock_10_minutes();
 if(failures) { std::fprintf(stderr, "midi_clock_phaselock_test: %d failures\n", failures); return 1; }
 std::printf("midi_clock_phaselock_test: all acceptance conditions met\n");
 return 0;
}
