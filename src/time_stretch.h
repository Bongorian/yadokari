#pragma once
// Offline time-stretch (Stage 3, first stage of SPEC 5 Stage 3):
// WSOLA (waveform-similarity overlap-add). Pitch is preserved by
// construction - grains are copied, never resampled - and the
// similarity search keeps transients aligned. The processor is
// incremental: step() advances a bounded number of synthesis hops so
// the SessionFiles service loop can drive it in quanta, publish
// progress and honour cancellation, and finished frames can be
// streamed out (and to disk) before the whole render completes.
//
// Buffer model: out_/norm_ always start at the next unconsumed frame.
// end_ and normalised_ are buffer-relative frame counts. A frame
// becomes final once every hop covering it has landed; at that point
// its window weight is complete and it is normalised in place, so
// ready frames are always in final form. Buffers come from the
// project allocation policy (no std containers, like every other
// shared header) and grow geometrically.
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "alloc_policy.h"

namespace groove {

struct TimeStretch {
  static constexpr uint32_t kWindow = 2048;
  static constexpr uint32_t kHop = 512;
  static constexpr int kTolerance = 512;

  TimeStretch() = default;
  ~TimeStretch() { delete[] out_; delete[] norm_; }
  TimeStretch(const TimeStretch&) = delete;
  TimeStretch& operator=(const TimeStretch&) = delete;

  bool init(const float* input, uint32_t frames, uint16_t channels, float ratio) {
    delete[] out_; delete[] norm_;
    out_ = nullptr; norm_ = nullptr; capacity_ = 0; out_frames_ = 0;
    if (!input || !frames || (channels != 1 && channels != 2)) return false;
    if (ratio < 0.5f || ratio > 2.0f) return false;
    in_ = input;
    frames_ = frames;
    channels_ = channels;
    ratio_ = ratio;
    total_target_ = uint32_t(llround(double(frames) * double(ratio)));
    remaining_ = total_target_;
    ensure(kWindow);
    if (!out_ || !norm_) return false;
    out_frames_ = kWindow;
    end_ = 0;
    normalised_ = 0;
    analysis_ = 0.0;
    done_ = false;
    return true;
  }

  bool done() const { return done_; }
  uint32_t target_frames() const { return total_target_; }
  float progress() const {
    if (!frames_) return 1.0f;
    float p = float(analysis_ / double(frames_));
    return p > 1.0f ? 1.0f : p;
  }

  // Advance at most max_hops synthesis hops. Returns false when the
  // render has finished (done() is set at the same point).
  bool step(unsigned max_hops) {
    if (done_) return false;
    for (unsigned h = 0; h < max_hops; ++h) {
      if (!hop()) {
        finish();
        return false;
      }
    }
    // Never publish past the target length: hops near the end write
    // beyond it, and finish() trims the surplus.
    uint32_t safe = end_ > kWindow ? end_ - kWindow : 0;
    normalise_to(safe < remaining_ ? safe : remaining_);
    return true;
  }

  // Frames that are final (normalised, never touched again).
  uint32_t ready_frames() const { return normalised_; }
  const float* ready_data() const { return out_; }
  void consume(uint32_t frames) {
    if (!frames) return;
    uint32_t keep = out_frames_ - frames;
    __builtin_memmove(out_, out_ + size_t(frames) * channels_, size_t(keep) * channels_ * sizeof(float));
    __builtin_memmove(norm_, norm_ + frames, size_t(keep) * sizeof(float));
    out_frames_ = keep;
    end_ -= frames;
    normalised_ -= frames;
    remaining_ -= frames;
  }

 private:
  float in_at(uint32_t frame, unsigned channel) const {
    if (frame >= frames_) return 0.0f;
    return in_[size_t(frame) * channels_ + channel];
  }
  float out_at(uint32_t frame, unsigned channel) const {
    size_t i = size_t(frame) * channels_ + channel;
    return i < size_t(out_frames_) * channels_ ? out_[i] : 0.0f;
  }
  static float window(uint32_t i) {
    return float(0.5 - 0.5 * cos(6.283185307179586 * double(i) / double(kWindow - 1)));
  }

  bool hop() {
    uint32_t centre = uint32_t(analysis_ + 0.5);
    if (centre >= frames_ + kWindow) return false;
    int best = 0;
    if (end_ > 0) {
      uint32_t overlap = kWindow - kHop;
      double best_score = -1e30;
      for (int d = -kTolerance; d <= kTolerance; ++d) {
        int start = int(centre) + d;
        double score = 0.0, ref = 0.0, cand = 0.0;
        for (uint32_t i = 0; i < overlap; i += 4) {
          double r = 0.0, c = 0.0;
          for (unsigned ch = 0; ch < channels_; ++ch) {
            r += out_at(end_ + i, ch);
            int s = start + int(i);
            c += in_at(s < 0 ? 0 : uint32_t(s), ch);
          }
          score += r * c;
          ref += r * r;
          cand += c * c;
        }
        double denom = sqrt(ref * cand);
        double value = denom > 1e-12 ? score / denom : 0.0;
        if (value > best_score) {
          best_score = value;
          best = d;
        }
      }
    }
    int start = int(centre) + best;
    ensure(end_ + kWindow);
    if (!out_ || !norm_) return false;
    if (end_ + kWindow > out_frames_) out_frames_ = end_ + kWindow;
    for (uint32_t i = 0; i < kWindow; ++i) {
      float w = window(i);
      int s = start + int(i);
      uint32_t src = s < 0 ? 0 : uint32_t(s);
      for (unsigned ch = 0; ch < channels_; ++ch) {
        out_[size_t(end_ + i) * channels_ + ch] += in_at(src, ch) * w;
      }
      norm_[end_ + i] += w;
    }
    end_ += kHop;
    analysis_ += double(kHop) / double(ratio_);
    return true;
  }

  void ensure(uint32_t frames) {
    if (frames <= capacity_) {
      // The region beyond the logical end can hold stale data (the
      // tail copies left behind by the memmove in consume); like the
      // vector resize this replaces, newly exposed frames start at 0.
      for (size_t i = size_t(out_frames_) * channels_; i < size_t(frames) * channels_; ++i) out_[i] = 0.0f;
      for (uint32_t i = out_frames_; i < frames; ++i) norm_[i] = 0.0f;
      return;
    }
    uint32_t cap = capacity_ ? capacity_ : 4096;
    while (cap < frames) cap *= 2;
    float* next_out = alloc_array<float>(size_t(cap) * channels_);
    float* next_norm = alloc_array<float>(cap);
    if (!next_out || !next_norm) {
      delete[] next_out; delete[] next_norm;
      delete[] out_; delete[] norm_;
      out_ = nullptr; norm_ = nullptr; capacity_ = 0;
      return;
    }
    for (size_t i = 0; i < size_t(out_frames_) * channels_; ++i) next_out[i] = out_[i];
    for (uint32_t i = 0; i < out_frames_; ++i) next_norm[i] = norm_[i];
    for (size_t i = size_t(out_frames_) * channels_; i < size_t(cap) * channels_; ++i) next_out[i] = 0.0f;
    for (uint32_t i = out_frames_; i < cap; ++i) next_norm[i] = 0.0f;
    delete[] out_; delete[] norm_;
    out_ = next_out; norm_ = next_norm; capacity_ = cap;
  }

  void normalise_to(uint32_t upto) {
    if (upto > out_frames_) upto = out_frames_;
    while (normalised_ < upto) {
      float n = norm_[normalised_];
      if (n > 1e-6f) {
        for (unsigned ch = 0; ch < channels_; ++ch) out_[size_t(normalised_) * channels_ + ch] /= n;
      }
      ++normalised_;
    }
  }

  void finish() {
    if (remaining_ < out_frames_) out_frames_ = remaining_;
    normalise_to(out_frames_);
    done_ = true;
  }

  const float* in_ = nullptr;
  uint32_t frames_ = 0;
  uint16_t channels_ = 0;
  float ratio_ = 1.0f;
  uint32_t total_target_ = 0;
  uint32_t remaining_ = 0;
  float* out_ = nullptr;
  float* norm_ = nullptr;
  uint32_t capacity_ = 0;
  uint32_t out_frames_ = 0;
  uint32_t end_ = 0;
  uint32_t normalised_ = 0;
  double analysis_ = 0.0;
  bool done_ = false;
};

}  // namespace groove
