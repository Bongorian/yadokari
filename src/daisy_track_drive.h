#pragma once
#include "Effects/overdrive.h"

namespace groove {
// The exposed DRIVE control keeps an exact dry bypass at zero. DaisySP's
// Overdrive is the wet processor, shared by left and right (it is stateless).
class DaisyTrackDrive {
 public:
  DaisyTrackDrive() { dsp_.Init(); }
  float process(float input, float amount) {
    if (amount <= 0.0f) return input;
    if (amount - cached_ > 0.001f || cached_ - amount > 0.001f) {
      // Upstream 1.0 drives its output compensation to a loud endpoint.
      dsp_.SetDrive(0.5f + amount * 0.45f);
      cached_ = amount;
    }
    return input + (dsp_.ProcessInline(input) - input) * amount;
  }
 private:
  daisysp::Overdrive dsp_;
  float cached_ = -1.0f;
};
}
