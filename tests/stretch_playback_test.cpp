// Stage 3 second stage: Stretch playback mode (machine choice).
// A sampler sound whose mode is Stretch plays its trimmed region
// pitch-preserved and tempo-locked: the engine derives the duration
// ratio from the asset BPM metadata and the project tempo, and the
// voice runs a WSOLA grain engine instead of the rate resampler, so
// neither the note nor the tempo changes the pitch. These tests pin
// the voice-level contract: pitch is preserved at ratio 2 and 0.5,
// playback loops past the source length, the stored mode survives the
// project codec, and validation accepts Stretch but not beyond.
#include "wav_sampler.h"
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
// Zero crossings per second over [begin, frames) of a mono series.
double crossing_rate(const std::vector<float>& out, unsigned begin, unsigned frames) {
 unsigned n = 0;
 for (unsigned i = begin + 1; i < frames; ++i) if ((out[i] < 0) != (out[i - 1] < 0)) ++n;
 return double(n) * 48000.0 / double(frames - begin) / 2.0;
}
std::vector<float> render(WavVoice& voice, unsigned frames) {
 std::vector<float> out; out.reserve(frames);
 for (unsigned i = 0; i < frames; ++i) { float l, r; voice.next(l, r); out.push_back(l); }
 return out;
}
} // namespace
int main() {
 // Validation bounds: Stretch (4) is valid, 5 is not.
 { SampleEdit e; e.mode = SampleMode::Stretch; assert(valid_sample_edit(e));
   e.mode = SampleMode(5); assert(!valid_sample_edit(e)); }

 // Source: 1 s of a 220 Hz sine at 48 kHz, 16-bit mono.
 const unsigned kFrames = 48000;
 std::vector<uint8_t> pcm(kFrames * 2);
 for (unsigned i = 0; i < kFrames; ++i) {
   int16_t v = int16_t(0.8f * 32767.0f * sinf(6.2831853f * 220.0f * float(i) / 48000.0f));
   pcm[i * 2] = uint8_t(v & 0xff); pcm[i * 2 + 1] = uint8_t((v >> 8) & 0xff);
 }
 WavView view{}; view.data = pcm.data(); view.frames = kFrames; view.rate = 48000; view.channels = 1; view.bits = 16; view.format = 1;
 SampleEdit edit; edit.mode = SampleMode::Stretch; edit.fade_ms = 0; edit.crossfade_ms = 0; edit.attack_ms = 0; edit.release_ms = 0;

 // Ratio 2 (asset slower than project): 2 s of output still sounds
 // 220 Hz late in the render, i.e. time doubled with pitch kept, and
 // the voice is still looping after the source length has passed.
 { WavVoice voice; assert(voice.start_edit(view, 48000, 3.0f, false, false, edit, 2.0f));
   auto out = render(voice, 96000);
   double f = crossing_rate(out, 72000, 96000);
   std::printf("stretch x2 late frequency %.1f Hz\n", f);
   assert(f > 205.0 && f < 235.0);
   assert(voice.active()); }

 // Ratio 0.5: half a second of output covers the whole source and
 // still loops at the same pitch.
 { WavVoice voice; assert(voice.start_edit(view, 48000, 0.25f, false, false, edit, 0.5f));
   auto out = render(voice, 48000);
   double f = crossing_rate(out, 24000, 48000);
   std::printf("stretch x0.5 late frequency %.1f Hz\n", f);
   assert(f > 205.0 && f < 235.0);
   assert(voice.active()); }

 // A ratio outside the WSOLA design range is refused (falls back to
 // the plain path by not engaging stretch): the call itself still
 // succeeds as a normal start, matching start_edit's contract.
 { WavVoice voice; assert(voice.start_edit(view, 48000, 1.0f, false, false, edit, 3.0f)); voice.stop(); }

 // The mode persists through the project codec byte-exactly.
 { auto project = std::make_unique<ProjectState>(); make_empty_project(*project);
   track_at(*project,0,0).sample_edit.mode = SampleMode::Stretch;
   Buffer buffer; assert(encode_project(buffer, *project));
   auto restored = std::make_unique<ProjectState>(); assert(decode_project(buffer, *restored));
   assert(track_at(*restored,0,0).sample_edit.mode == SampleMode::Stretch); }

 std::printf("stretch playback tests passed\n");
 return 0;
}
