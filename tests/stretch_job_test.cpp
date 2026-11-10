// Stage 3 S3-2b: the offline stretch job. The Sampler page-3 STRETCH
// soft key opens the confirm modal; knob 1 sets the ratio; START runs
// the job in SessionFiles service quanta (playback state untouched),
// streaming PCM to <name>_x<pct>.wav, which is then loaded and bound
// to a free asset slot on the originating track. Cancel removes the
// partial file. Outputs projected over 256 MiB are refused before any
// allocation, and WAV data chunks over 256 MiB are refused at header
// parse, the single choke point of every load path.
#include "session_files.h"
#include "sample_stream.h"
#include <memory>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace groove;
namespace {
struct StoreFs : IFileAccess {
 std::map<std::string, std::vector<uint8_t>> files;
 bool fail_read=false;unsigned largest_read=0;
 unsigned list(const char*, FileEntry*, unsigned) override { return 0; }
 bool read(const char* path, uint64_t offset, void* out, unsigned count, unsigned& got) override {
  got = 0;if(count>largest_read)largest_read=count;if(fail_read)return false;
  auto it = files.find(path);
  if (it == files.end() || offset >= it->second.size()) return offset == 0 && count == 0;
  unsigned n = count;
  if (offset + n > it->second.size()) n = unsigned(it->second.size() - offset);
  std::memcpy(out, it->second.data() + offset, n);
  got = n;
  return true;
 }
 bool write(const char* path, uint64_t offset, const void* data, unsigned count, bool create) override {
  auto& v = files[path];
  if (create && offset == 0) v.clear();
  if (offset + count > v.size()) v.resize(offset + count);
  std::memcpy(v.data() + offset, data, count);
  return true;
 }
 bool move(const char* from, const char* to) override { files[to] = files[from]; files.erase(from); return true; }
 bool remove(const char* path) override { return files.erase(path) > 0; }
 bool directory(const char*) override { return true; }
 uint64_t size(const char* path) override { auto it = files.find(path); return it == files.end() ? 0 : it->second.size(); }
 bool exists(const char* path) override { return files.count(path) > 0; }
 bool revision(const char* path, FileRevision& r) override { r = {size(path), 7, 7, 7}; return true; }
};
struct StoreLibrary : ISampleLibrary {
 StoreFs& fs;
 mutable std::map<std::string, std::vector<uint8_t>> cache;
 mutable std::map<std::string, WavView> views;
 explicit StoreLibrary(StoreFs& f) : fs(f) {}
 bool parse_into(const char* path, WavView& view) const {
  if (!std::strcmp(path, "samples/huge.wav")) {
   view = WavView{nullptr, 100000000, 48000, 1, 16, 1, nullptr};
   return true;
  }
  auto it = fs.files.find(path);
  if (it == fs.files.end()) return false;
  auto& bytes = cache[path];
  bytes = it->second;
  return parse_wav(bytes.data(), bytes.size(), view);
 }
 bool load(const char* path, WavView& view) override {
  if (!parse_into(path, view)) return false;
  views[path] = view;
  return true;
 }
 WavView lookup(const char* path) const override {
  auto it = views.find(path);
  if (it != views.end()) return it->second;
  WavView view;
  if (!parse_into(path, view)) return {};
  views[path] = view;
  return view;
 }
 bool prepare(const ProjectState&, WavView*) override { return true; }
};
void put_wav(StoreFs& fs, const char* path, uint32_t frames) {
 std::vector<uint8_t> bytes(44 + frames * 2);
 wav_header_ch(bytes.data(), frames, 48000, 1);
 for (uint32_t i = 0; i < frames; ++i) {
  float s = 0.5f * std::sin(6.283185307179586f * 220.0f * float(i) / 48000.0f);
  uint16_t v = uint16_t(pcm16(s));
  bytes[44 + i * 2] = uint8_t(v);
  bytes[44 + i * 2 + 1] = uint8_t(v >> 8);
 }
 fs.files[path] = bytes;
}
struct StreamLibrary:StoreLibrary {
 SampleStream stream;WavView source;
 explicit StreamLibrary(StoreFs& fs):StoreLibrary(fs),stream(fs){assert(stream.open("samples/sine.wav",source));}
 WavView lookup(const char* path)const override{return !std::strcmp(path,"samples/sine.wav")?source:StoreLibrary::lookup(path);}
 void service()override{stream.service();}
};
void open_stretch(Performance& perf){perf.set_mode(UiMode::Sampler);for(unsigned i=0;i<3;++i)perf.encoder_direction(0x08);assert(perf.soft_key(4));assert(perf.soft_key(0));}
void finish_stretch(Performance& perf,SessionFiles& files){for(unsigned i=0;i<20000 && perf.stretch_modal();++i)files.service();assert(perf.stretch_modal()==0);}
void regression_cases(){
 // An offline job must read the file, never enqueue requests as a second
 // producer on the audio stream's resident-page reader.
 {
  Engine engine(48000);auto project=std::make_unique<ProjectState>();make_empty_project(*project);track_at(*project,0,0).instrument=Instrument::Sampler;copy_text(project->assets[0].path,96,"samples/sine.wav");
  StoreFs fs;put_wav(fs,"samples/sine.wav",48000);StreamLibrary lib(fs);Performance perf(engine,*project);SessionFiles files(fs,lib,engine,*project,perf);
  open_stretch(perf);finish_stretch(perf,files);WavView out;assert(lib.load("samples/sine_x150.wav",out) && out.frames==72000);
  double late=0;for(unsigned i=30000;i<60000;++i){double value=wav_sample(out,i,0);late+=value*value;}
  assert(std::sqrt(late/30000)>0.1 && lib.stream.underruns()==0 && fs.largest_read<=kFileScanBufferBytes);
  assert(!fs.exists("samples/sine_x150.wav.part"));
 }
 // Exhaustion must not truncate or delete any existing output, even when
 // CANCEL follows the refused request.
 {
  Engine engine(48000);auto project=std::make_unique<ProjectState>();make_empty_project(*project);track_at(*project,0,0).instrument=Instrument::Sampler;copy_text(project->assets[0].path,96,"samples/sine.wav");
  StoreFs fs;put_wav(fs,"samples/sine.wav",4800);StoreLibrary lib(fs);fs.files["samples/sine_x150.wav"]={1,2,3};
  for(unsigned i=2;i<=9;++i){char path[64];std::snprintf(path,sizeof path,"samples/sine_x150-%u.wav",i);fs.files[path]={uint8_t(i),2,3};}
  auto original=fs.files;Performance perf(engine,*project);SessionFiles files(fs,lib,engine,*project,perf);open_stretch(perf);finish_stretch(perf,files);
  assert(std::strstr(perf.browser().status,"names full") && fs.files==original && track_at(*project,0,0).sample_slot==0);
 }
 // A short/failed read cannot be converted to a successful silent render.
 {
  Engine engine(48000);auto project=std::make_unique<ProjectState>();make_empty_project(*project);track_at(*project,0,0).instrument=Instrument::Sampler;copy_text(project->assets[0].path,96,"samples/sine.wav");
  StoreFs fs;put_wav(fs,"samples/sine.wav",48000);StreamLibrary lib(fs);Performance perf(engine,*project);SessionFiles files(fs,lib,engine,*project,perf);
  open_stretch(perf);files.service();assert(perf.stretch_modal()==2);fs.fail_read=true;files.service();
  assert(perf.stretch_modal()==0 && std::strstr(perf.browser().status,"read error") && fs.files.size()==1 && track_at(*project,0,0).sample_slot==0);
 }
 std::puts("stretch regressions: streamed tail, bounded file reads, output exhaustion and failed read passed");
}
} // namespace
int main() {
 regression_cases();
 // Header-parse refusal: a data chunk claiming 300 MiB is rejected.
 {
  uint8_t header[64]{};
  std::memcpy(header, "RIFF", 4); std::memcpy(header + 8, "WAVE", 4);
  std::memcpy(header + 12, "fmt ", 4); header[16] = 16;
  header[20] = 1; header[22] = 1;
  header[24] = 0x80; header[25] = 0xbb; // 48000
  header[28] = 0x00; header[29] = 0x77; header[30] = 0x01; // byte rate 96000
  header[32] = 2; header[34] = 16;
  std::memcpy(header + 36, "data", 4);
  uint32_t big = 300u * 1024u * 1024u;
  for (unsigned b = 0; b < 4; ++b) header[40 + b] = uint8_t(big >> (b * 8));
  WavView view; uint64_t offset = 0;
  assert(!parse_wav_header(header, sizeof header, 400ull * 1024 * 1024, view, offset));
  std::printf("256 MiB header refusal ok\n");
 }
 Engine engine(48000);
 ProjectState project;
 make_empty_project(project);
 track_at(project,0,0).instrument = Instrument::Sampler;
 track_at(project,0,0).sample_slot = 0;
 copy_text(project.assets[0].path, sizeof project.assets[0].path, "samples/sine.wav");
 copy_text(project.assets[1].path, sizeof project.assets[1].path, "samples/huge.wav");
 Performance perf(engine, project);
 StoreFs fs;
 StoreLibrary lib(fs);
 put_wav(fs, "samples/sine.wav", 4800);
 SessionFiles files(fs, lib, engine, project, perf);
 perf.set_mode(UiMode::Sampler);
 perf.encoder_direction(0x08); perf.encoder_direction(0x08); perf.encoder_direction(0x08);
 assert(perf.sampler_page() == 3);
 assert(!std::strcmp(perf.soft_label(4), "STRETCH"));
 // Open the modal and set the ratio with knob 1.
 assert(perf.soft_key(4));
 assert(perf.stretch_modal() == 1);
 assert(perf.stretch_ratio_pct() == 150);
 assert(perf.knob(0, 10000));
 assert(perf.stretch_ratio_pct() == 200);
 assert(perf.knob(0, -10000));
 assert(perf.stretch_ratio_pct() == 150);
 // START: the job runs over service quanta and closes the modal.
 assert(perf.soft_key(0));
 for (int i = 0; i < 20000 && perf.stretch_modal() != 0; ++i) files.service();
 assert(perf.stretch_modal() == 0);
 assert(fs.exists("samples/sine_x150.wav"));
 unsigned bound_slot = 64;
 for (unsigned i = 0; i < 64; ++i) if (!std::strcmp(project.assets[i].path, "samples/sine_x150.wav")) bound_slot = i;
 assert(bound_slot < 64);
 assert(track_at(project,0,0).sample_slot == bound_slot);
 WavView out;
 assert(lib.load("samples/sine_x150.wav", out));
 assert(out.frames == 7200 && out.channels == 1 && out.rate == 48000);
 std::printf("stretch job completed: slot %u, %u frames\n", bound_slot, out.frames);
 // Cancel: start on the stretched asset, cancel mid-flight.
 assert(perf.soft_key(4));
 assert(perf.stretch_modal() == 1);
 assert(perf.soft_key(0));
 for (int i = 0; i < 2; ++i) files.service();
 assert(perf.stretch_modal() == 2);
 assert(perf.soft_key(0)); // CANCEL while running
 for (int i = 0; i < 10 && perf.stretch_modal() != 0; ++i) files.service();
 assert(perf.stretch_modal() == 0);
 assert(!fs.exists("samples/sine_x150_x150.wav"));
 std::printf("stretch cancel ok\n");
 // Refusal: the huge asset's projected output exceeds 256 MiB.
 track_at(project,0,0).sample_slot = 1;
 assert(perf.soft_key(4));
 assert(perf.soft_key(0));
 for (int i = 0; i < 10; ++i) files.service();
 assert(perf.stretch_modal() == 0);
 assert(std::strstr(perf.browser().status, "refused"));
 std::printf("stretch refusal ok: %s\n", perf.browser().status);
 std::printf("stretch job test passed\n");
 return 0;
}
