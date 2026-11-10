// Unit 19: Browser stabilization. Opening a WAV from the Browser must
// go through the library's incremental loader (one poll quantum per
// service call) instead of the blocking load(), and a second OPEN
// while a load is in flight must never re-enter begin/load.
#include "session_files.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace groove;
namespace {
struct FakeFs : IFileAccess {
 unsigned list(const char*, FileEntry* entries, unsigned capacity) override {
  unsigned n = 0;
  auto add = [&](const char* name, bool dir, uint64_t bytes) {
   if (n < capacity) { copy_text(entries[n].name, sizeof entries[n].name, name); entries[n].directory = dir; entries[n].bytes = bytes; ++n; }
  };
  add("KICK.WAV", false, 1000); add("SNARE.WAV", false, 2000);
  return n;
 }
 bool list_browser(const char* path, FileEntry* entries, unsigned capacity, unsigned& count) override {
  count = list(path, entries, capacity); return capacity >= 2;
 }
 bool read(const char*, uint64_t, void*, unsigned, unsigned& got) override { got = 0; return true; }
 bool write(const char*, uint64_t, const void*, unsigned, bool) override { return true; }
 bool move(const char*, const char*) override { return true; }
 bool remove(const char*) override { return true; }
 bool directory(const char*) override { return true; }
 uint64_t size(const char*) override { return 1000; }
 bool exists(const char*) override { return true; }
 bool revision(const char*, FileRevision& r) override { r = {1000, 7, 7, 7}; return true; }
};
struct FakeLibrary : ISampleLibrary {
 static uint8_t pcm[256];
 int polls_left = 0, begin_count = 0, blocking_loads = 0, reentries = 0;
 bool in_flight = false;
 bool load(const char*, WavView& view) override { ++blocking_loads; fill(view); return true; }
 WavView lookup(const char*) const override { return {}; }
 bool prepare(const ProjectState&, WavView*) override { return true; }
 bool supports_incremental_load() const override { return true; }
 bool begin_incremental_load(const char*, uint32_t) override {
  if (in_flight) ++reentries;
  in_flight = true; ++begin_count; polls_left = 40; return true;
 }
 SampleLoadProgress poll_incremental_load(WavView& view) override {
  if (polls_left > 0) { --polls_left; return SampleLoadProgress::Working; }
  in_flight = false; fill(view); return SampleLoadProgress::Ready;
 }
 void cancel_incremental_load() override { in_flight = false; }
 static void fill(WavView& view) {
  view.data = pcm; view.frames = 64; view.rate = 48000; view.channels = 1; view.bits = 16; view.format = 1; view.reader = nullptr;
 }
};
uint8_t FakeLibrary::pcm[256] = {};
} // namespace
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 copy_text(perf.browser().directory,sizeof perf.browser().directory,"assets");
 FakeFs fs; FakeLibrary lib;
 SessionFiles files(fs, lib, engine, project, perf);
 perf.set_mode(UiMode::Browser);
 // Entering Browser mode already queues a Refresh; service it and the
 // listing is populated through the real action path.
 for (int i = 0; i < 10 && perf.browser().count == 0; ++i) files.service();
 assert(perf.browser().count == 2);
 // First OPEN starts exactly one incremental load; the blocking path
 // is never used and service() returns after one poll quantum.
 perf.browser().cursor = 0;
 assert(perf.request_file(FileAction::Open));
 files.service();
 assert(lib.begin_count == 1 && lib.blocking_loads == 0);
 assert(!perf.browser().previewing);
 // Re-press storm while in flight: 100 OPEN requests + service calls
 // must not begin another load nor complete early.
 for (int i = 0; i < 30; ++i) {
  perf.browser().cursor = 1;
  perf.request_file(FileAction::Open);
  files.service();
  assert(lib.begin_count == 1);
 }
 assert(lib.reentries == 0);
 // The first load completes after its remaining polls; only then is
 // the queued second OPEN taken, starting load two.
 for (int i = 0; i < 200 && lib.begin_count < 2; ++i) files.service();
 assert(lib.begin_count == 2 && lib.blocking_loads == 0);
 for (int i = 0; i < 400 && std::strcmp(perf.browser().preview_path, "assets/SNARE.WAV"); ++i) files.service();
 assert(perf.browser().previewing);
 assert(std::strcmp(perf.browser().preview_path, "assets/SNARE.WAV") == 0);
 // OPEN x100 storm across both files: alternating targets defeat the
 // replay cache, so loads keep cycling - always incremental, never
 // re-entrant, and the browser ends consistent.
 int begins_before = lib.begin_count;
 for (int i = 0; i < 100; ++i) {
  perf.browser().cursor = unsigned(i % 2);
  perf.request_file(FileAction::Open);
  files.service();
  assert(lib.blocking_loads == 0 && lib.reentries == 0);
 }
 for (int i = 0; i < 400; ++i) files.service();
 assert(lib.begin_count > begins_before);
 assert(perf.browser().previewing);
 std::printf("browser open stress: %d incremental loads, 0 blocking, 0 re-entries\n", lib.begin_count);
 std::printf("browser-open-stress test passed\n");
 return 0;
}
