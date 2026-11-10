#include "wav_sampler.h"
struct ReferenceReader:groove::ISampleReader {groove::WavView source{};bool read_frame(uint32_t frame,unsigned channel,float& result)override{result=groove::wav_sample(source,frame,channel);return true;}uint32_t underruns()const override{return 0;}};
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

static void put16(std::vector<uint8_t>& wav, unsigned at, uint16_t value) {
  wav[at] = uint8_t(value);
  wav[at + 1] = uint8_t(value >> 8);
}
static void put32(std::vector<uint8_t>& wav, unsigned at, uint32_t value) {
  put16(wav, at, uint16_t(value));
  put16(wav, at + 2, uint16_t(value >> 16));
}
static std::vector<uint8_t> pcm(unsigned bits, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> wav(44 + data.size());
  const char* riff = "RIFF";
  const char* wave = "WAVEfmt ";
  const char* chunk = "data";
  for (unsigned i = 0; i < 4; ++i) wav[i] = uint8_t(riff[i]);
  put32(wav, 4, uint32_t(wav.size() - 8));
  for (unsigned i = 0; i < 8; ++i) wav[8 + i] = uint8_t(wave[i]);
  put32(wav, 16, 16);
  put16(wav, 20, 1);
  put16(wav, 22, 1);
  put32(wav, 24, 48000);
  put32(wav, 28, 48000 * bits / 8);
  put16(wav, 32, uint16_t(bits / 8));
  put16(wav, 34, uint16_t(bits));
  for (unsigned i = 0; i < 4; ++i) wav[36 + i] = uint8_t(chunk[i]);
  put32(wav, 40, uint32_t(data.size()));
  for (unsigned i = 0; i < data.size(); ++i) wav[44 + i] = data[i];
  return wav;
}
int main() {
  using namespace groove;
  for (unsigned bits : {8u, 32u}) {
    const std::vector<uint8_t> samples = bits == 8
      ? std::vector<uint8_t>{0, 128, 255, 128}
      : std::vector<uint8_t>{0, 0, 0, 128, 0, 0, 0, 0, 255, 255, 255, 127, 0, 0, 0, 0};
    auto wav = pcm(bits, samples);
    WavView view;
    assert(parse_wav(wav.data(), wav.size(), view));
    assert(view.bits == bits && view.frames == 4 && view.format == 1);
    assert(wav_sample(view, 0, 0) == -1.0f);
    assert(wav_sample(view, 1, 0) == 0.0f);
    assert(wav_sample(view, 2, 0) > 0.99f);
    WavVoice voice;
    assert(voice.start(view, 48000, 1.0f, false));
    float left, right;
    voice.next(left, right);
    assert(left == -1.0f && right == -1.0f);
    wav[32] = 2;
    assert(!parse_wav(wav.data(), wav.size(), view));
  }
  for(unsigned channels:{1u,2u})for(bool reverse:{false,true})for(unsigned mode=0;mode<3;++mode)for(float pitch:{0.5f,0.999f,1.0f}){
    std::vector<uint8_t> bytes(128*channels*2);for(unsigned i=0;i<bytes.size();++i)bytes[i]=uint8_t(i*73+19);
    WavView resident{bytes.data(),128,48000,uint16_t(channels),16,1};ReferenceReader reader;reader.source=resident;WavView streaming=resident;streaming.data=nullptr;streaming.reader=&reader;
    SampleEdit edit;edit.start=0.1f;edit.end=0.9f;edit.mode=mode==2?SampleMode::PingPong:mode==1?SampleMode::Loop:SampleMode::OneShot;edit.fade_ms=0;edit.crossfade_ms=0;
    WavVoice fast,reference;assert(fast.start_edit(resident,48000,pitch,false,reverse,edit) && reference.start_edit(streaming,48000,pitch,false,reverse,edit));
    for(unsigned i=0;i<600;++i){float l,r,a,b;fast.next(l,r);reference.next(a,b);assert(l==a && r==b && fast.active()==reference.active());}
  }
}
