// Stage 3 S3-2a: offline time-stretch fixture (SPEC 5 Stage 3
// acceptance: objective stretch quality metrics). WSOLA must preserve
// pitch (zero-crossing rate of a steady tone), preserve transients
// (onset count and scaled positions), hit the target length, and the
// incremental/streaming path must assemble the identical render.
#include "time_stretch.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace groove;

static std::vector<float> render_all(const std::vector<float>& in, uint16_t channels, float ratio) {
  TimeStretch ts;
  assert(ts.init(in.data(), uint32_t(in.size() / channels), channels, ratio));
  while (ts.step(64)) {
  }
  assert(ts.done());
  std::vector<float> out(ts.ready_data(), ts.ready_data() + ts.ready_frames() * channels);
  return out;
}

static double zero_crossing_rate(const float* x, uint32_t begin, uint32_t end) {
  uint32_t crossings = 0;
  for (uint32_t i = begin + 1; i < end; ++i)
    if ((x[i - 1] < 0 && x[i] >= 0) || (x[i - 1] >= 0 && x[i] < 0)) ++crossings;
  return double(crossings) / double(end - begin);  // crossings per frame
}

static std::vector<uint32_t> onsets(const std::vector<float>& x, uint32_t hop = 256) {
  std::vector<uint32_t> found;
  float prev = 0;
  for (uint32_t i = 0; i + hop <= x.size(); i += hop) {
    float peak = 0;
    for (uint32_t j = i; j < i + hop; ++j) peak = std::fmax(peak, std::fabs(x[j]));
    if (peak > 0.35f && prev <= 0.35f) found.push_back(i);
    prev = peak;
  }
  return found;
}

int main() {
  const uint32_t rate = 48000;
  // Pitch fixture: steady 220 Hz sine.
  std::vector<float> sine(rate);
  for (uint32_t i = 0; i < rate; ++i) sine[i] = 0.5f * std::sin(6.283185307179586 * 220.0 * i / rate);
  double base_rate = zero_crossing_rate(sine.data(), 4000, 44000);
  for (float ratio : {1.5f, 0.75f, 2.0f}) {
    auto out = render_all(sine, 1, ratio);
    uint32_t want = uint32_t(std::llround(rate * ratio));
    std::printf("ratio %.2f: frames %u want %u\n", ratio, unsigned(out.size()), want);
    assert(std::fabs(double(out.size()) - double(want)) <= double(want) * 0.01);
    // Pitch: measure inside the steady region (skip edges).
    uint32_t b = uint32_t(out.size() * 0.2), e = uint32_t(out.size() * 0.8);
    double zcr = zero_crossing_rate(out.data(), b, e);
    double err = std::fabs(zcr - base_rate) / base_rate;
    std::printf("  pitch zcr %.6f vs %.6f (err %.3f%%)\n", zcr, base_rate, err * 100);
    assert(err < 0.03);
    // Level: steady-state amplitude stays near the source's.
    float peak = 0;
    for (uint32_t i = b; i < e; ++i) peak = std::fmax(peak, std::fabs(out[i]));
    assert(peak > 0.4f && peak < 0.65f);
  }

  // Transient fixture: four clicks in silence.
  std::vector<float> clicks(rate, 0.0f);
  const uint32_t click_at[] = {8000, 16000, 24000, 32000};
  uint32_t seed = 12345;
  for (uint32_t c : click_at) {
    for (uint32_t i = 0; i < 240 && c + i < rate; ++i) {
      seed = seed * 1664525u + 1013904223u;
      float noise = float(int32_t(seed >> 9) % 2000 - 1000) / 1000.0f;
      clicks[c + i] = 0.9f * noise * std::exp(-float(i) / 40.0f);
    }
  }
  {
    auto out = render_all(clicks, 1, 1.5f);
    auto found = onsets(out);
    std::printf("transients: %u onsets\n", unsigned(found.size()));
    assert(found.size() == 4);
    for (unsigned i = 0; i < 4; ++i) {
      double want = double(click_at[i]) * 1.5;
      std::printf("  onset %u at %u want %.0f\n", i, found[i], want);
      assert(std::fabs(double(found[i]) - want) < 1600);
    }
  }

  // Streaming assembly equals the one-shot render, bit for bit.
  {
    TimeStretch ts;
    assert(ts.init(sine.data(), rate, 1, 1.5f));
    std::vector<float> assembled;
    while (!ts.done()) {
      ts.step(2);
      uint32_t ready = ts.ready_frames();
      if (ready) {
        assembled.insert(assembled.end(), ts.ready_data(), ts.ready_data() + ready);
        ts.consume(ready);
      }
    }
    uint32_t ready = ts.ready_frames();
    assembled.insert(assembled.end(), ts.ready_data(), ts.ready_data() + ready);
    auto whole = render_all(sine, 1, 1.5f);
    assert(assembled.size() == whole.size());
    for (size_t i = 0; i < whole.size(); ++i) assert(assembled[i] == whole[i]);
  }

  // Stereo keeps both channels and the ratio bounds are enforced.
  {
    std::vector<float> stereo(rate * 2);
    for (uint32_t i = 0; i < rate; ++i) {
      stereo[2 * i] = sine[i];
      stereo[2 * i + 1] = 0.25f * sine[i];
    }
    auto out = render_all(stereo, 2, 0.5f);
    assert(out.size() == rate);  // 24000 frames x 2 channels
    TimeStretch ts;
    assert(!ts.init(sine.data(), rate, 1, 0.4f));
    assert(!ts.init(sine.data(), rate, 1, 2.5f));
    assert(!ts.init(sine.data(), rate, 3, 1.5f));
  }
  std::printf("stretch fixture passed\n");
  return 0;
}
