#include "Synthesis/fm2.h"
#include <cassert>
#include <cmath>

int main() {
  daisysp::Fm2 low, high;
  low.Init(48000);
  high.Init(48000);
  low.SetFrequency(220);
  high.SetFrequency(220);
  low.SetRatio(2);
  high.SetRatio(2);
  low.SetIndex(0.25f);
  high.SetIndex(3.0f);
  double difference = 0;
  double energy = 0;
  for (unsigned i = 0; i < 48000; ++i) {
    float a = low.Process();
    float b = high.Process();
    assert(std::isfinite(a) && std::isfinite(b));
    assert(std::fabs(a) <= 1.001f && std::fabs(b) <= 1.001f);
    difference += std::fabs(double(a) - b);
    energy += double(b) * b;
  }
  assert(difference > 100 && energy > 100);
  high.Reset();
  float first = high.Process();
  high.Reset();
  assert(first == high.Process());
}
