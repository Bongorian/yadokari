// Unit 19: Browser stabilization. Opening a WAV from the Browser must
// go through the library's incremental loader (one poll quantum per
// service call) instead of the blocking load(), and a second OPEN
// while a load is in flight must never re-enter begin/load.
#include "session_files.h"
#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace groove;
namespace {
struct FakeFs : IFileAccess {
 static constexpr unsigned file_count=72;
 unsigned folder_reads=0;
 unsigned list(const char*, FileEntry* entries, unsigned capacity) override {
  unsigned n = 0;
  auto add = [&](const char* name, bool dir, uint64_t bytes) {
   if (n < capacity) { copy_text(entries[n].name, sizeof entries[n].name, name); entries[n].directory = dir; entries[n].bytes = bytes; ++n; }
  };
  add("KICK.WAV", false, 1000); add("SNARE.WAV", false, 2000);
  for(unsigned i=0;i<file_count-2;++i){
   char name[16];
   std::snprintf(name,sizeof name,"TAKE%02u.WAV",i);
   add(name,false,1000);
  }
  return n;
 }
 bool list_browser(const char* path, FileEntry* entries, unsigned capacity, unsigned& count) override {
  if(std::strcmp(path,"assets")==0)++folder_reads;
  count = list(path, entries, capacity); return capacity >= file_count;
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
 char load_path[160]{};
 bool load(const char*, WavView& view) override { ++blocking_loads; fill(view); return true; }
 WavView lookup(const char*) const override { return {}; }
 bool prepare(const ProjectState&, WavView*) override { return true; }
 bool supports_incremental_load() const override { return true; }
 bool begin_incremental_load(const char* path, uint32_t) override {
  if (in_flight) ++reentries;
  copy_text(load_path,sizeof load_path,path);
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
 auto position=[&]{return perf.browser().folder_offset+perf.browser().cursor;};
 auto push=[&](uint8_t direction){assert(perf.control(1,direction));assert(perf.control(1,0));};
 // A cold FILE press accepts input before the listing has any rows.
 assert(perf.control(8,0x10) && perf.control(8,0));
 assert(perf.mode()==UiMode::Browser && perf.browser().count==0);
 assert(perf.encoder(4));push(0x10);
 auto renderer=std::make_unique<UiRenderer>();
 bool processing=false;
 renderer->text_observer=[](const SoftwareCanvas::TextPlacement& t,void* context){
  if(!std::strcmp(t.text,"PROCESSING"))*static_cast<bool*>(context)=true;
 };
 renderer->text_observer_context=&processing;
 renderer->draw(project,perf);
 assert(processing && fs.folder_reads==0);
 assert(files.service() && perf.browser().folder_total==FakeFs::file_count && position()==5);
 // Refreshing a cached page leaves its rows navigable, with no reads
 // caused by the input. A crossing waits for the single action slot.
 assert(perf.request_file(FileAction::Refresh));
 const unsigned reads=fs.folder_reads;
 assert(perf.encoder(2));push(0x04);
 assert(position()==6 && fs.folder_reads==reads);
 assert(perf.move_browser(56) && position()==62);
 assert(perf.encoder(3));push(0x10);
 assert(position()==62 && perf.pending_file_action()==FileAction::Refresh);
 assert(files.service() && perf.pending_file_action()==FileAction::FolderMove);
 assert(perf.encoder(-4) && position()==62 && !perf.file_action_pending());
 assert(perf.encoder(3));push(0x10);
 assert(files.service() && position()==66);
 assert(perf.move_browser(-66) && files.service() && position()==0);
 // First OPEN starts exactly one incremental load; the blocking path
 // is never used and service() returns after one poll quantum.
 assert(perf.request_file(FileAction::Open));
 // Navigation before OPEN is taken cannot change the file it will open.
 assert(perf.encoder(1) && position()==0);
 files.service();
 assert(lib.begin_count == 1 && lib.blocking_loads == 0 && perf.browser().processing);
 assert(position()==1 && !std::strcmp(lib.load_path,"assets/KICK.WAV"));
 const unsigned reads_during_load=fs.folder_reads;
 assert(perf.encoder(1));push(0x04);push(0x04);push(0x10);
 assert(position()==1 && fs.folder_reads==reads_during_load && lib.in_flight);
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
 assert(!perf.browser().previewing);
 assert(std::strcmp(perf.browser().preview_path, "assets/SNARE.WAV") == 0);
 assert(!perf.browser().processing);
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
 assert(!perf.browser().previewing);
 assert(perf.browser().preview_path[0]);
 assert(perf.request_file(FileAction::Preview));
 for(int i=0;i<200 && !perf.browser().previewing;++i)files.service();
 assert(perf.browser().previewing);
 std::printf("browser open stress: %d incremental loads, 0 blocking, 0 re-entries\n", lib.begin_count);
 std::printf("browser-open-stress test passed\n");
 return 0;
}
