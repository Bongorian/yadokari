#include "daisy_track_drive.h"
#include <cassert>
#include <cmath>

int main() {
  groove::DaisyTrackDrive drive;
  assert(drive.process(0.37f, 0) == 0.37f);
  float low = drive.process(0.3f, 0.25f);
  float high = drive.process(0.3f, 1.0f);
  assert(std::isfinite(low) && std::isfinite(high));
  assert(std::fabs(low - 0.3f) > 0.001f);
  assert(std::fabs(high - low) > 0.001f);
  for (int i = -20; i <= 20; ++i) {
    float y = drive.process(float(i) * 0.1f, 1.0f);
    assert(std::isfinite(y) && std::fabs(y) <= 1.01f);
  }
}
