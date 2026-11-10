#include "wav_sampler.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

int main() {
  std::vector<uint8_t> pcm(8192 * 2);
  for (unsigned i = 0; i < 8192; ++i) {
    pcm[2 * i] = 0xff;
    pcm[2 * i + 1] = 0x7f;
  }
  groove::WavView sample{pcm.data(), 8192, 48000, 1, 16, 1, nullptr};
  groove::SampleEdit edit;
  edit.fade_ms = 10;
  edit.attack_ms = 0;
  for (float pitch : {0.5f, 1.0f, 2.0f}) {
    groove::WavVoice voice;
    assert(voice.start_edit(sample, 48000, pitch, false, false, edit));
    float left = 0, right = 0;
    for (unsigned i = 0; i <= 240; ++i) voice.next(left, right);
    assert(std::fabs(left - 0.5f) < 0.03f);
    assert(std::fabs(right - left) < 0.00001f);
  }
}
