#include "soundfont.h"
#include "soundfont_load_slot.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "soundfont_fixture.h"
#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <cassert>
#include <unistd.h>
using namespace groove;
int main(){
 auto cwd=std::filesystem::current_path();char root[]="/tmp/mk3-sf2-XXXXXX";assert(mkdtemp(root) && chdir(root)==0);DesktopFiles fs;assert(fs.write("debug.sf2",0,kSoundFontFixture,sizeof kSoundFontFixture,true));struct DeferredFonts:DesktopSampleAssets {bool incremental=false,active=false;unsigned cancellations=0,polls=0;char pending[96]{};bool supports_incremental_load()const override{return incremental;}bool begin_incremental_load(const char* path,uint32_t)override{if(active || !copy_text(pending,sizeof pending,path))return false;active=true;polls=0;return true;}SampleLoadProgress poll_incremental_load(WavView& view)override{if(!active)return SampleLoadProgress::Failed;if(polls++<2)return SampleLoadProgress::Working;active=false;return load(pending,view)?SampleLoadProgress::Ready:SampleLoadProgress::Failed;}void cancel_incremental_load()override{active=false;++cancellations;}} library;library.files(&fs);WavView view;assert(library.load("debug.sf2",view) && view.soundfont);assert(soundfont_name(view.soundfont,128,10) && !soundfont_name(view.soundfont,0,127));
 {
  SoundFontCache cache;WavView pinned;assert(cache.load(fs,"debug.sf2",pinned));SoundFontVoice sounding;sounding.init(0,48000);assert(sounding.trigger(pinned.soundfont,440,100,SoundFontState{}));
  assert(cache.evict_unused()==0 && soundfont_resident(pinned.soundfont));sounding.stop();assert(cache.evict_unused()==1 && !soundfont_resident(pinned.soundfont));
  assert(!sounding.trigger(pinned.soundfont,440,100,SoundFontState{}));assert(soundfont_name(pinned.soundfont,128,10));WavView restored;assert(cache.load(fs,"debug.sf2",restored) && restored.soundfont==pinned.soundfont);assert(sounding.trigger(restored.soundfont,440,100,SoundFontState{}));sounding.stop();
  SoundFontCache denied(1024);assert(!denied.load(fs,"debug.sf2",restored) && denied.failure()[0]);
 }
 // Decode on a worker, filesystem reads only on this producer; cancellation
 // must acknowledge outstanding requests before worker scratch can disappear.
 {
  struct OwnedFiles:DesktopFiles {std::thread::id owner=std::this_thread::get_id();unsigned reads=0;bool fail=false;SoundFontLoadSlot* cancel=nullptr;bool read(const char* p,uint64_t at,void* data,unsigned n,unsigned& got)override{assert(std::this_thread::get_id()==owner && n<=65536);++reads;if(cancel){cancel->cancel();cancel=nullptr;}if(fail){got=0;return false;}return DesktopFiles::read(p,at,data,n,got);}} owned;
  SoundFontCache cache;SoundFontLoadSlot job;uint32_t crc=0;assert(file_checksum(fs,"debug.sf2",crc));
  for(unsigned run=0;run<4;++run){
   owned.fail=run==3;owned.cancel=run==1?&job:nullptr;assert(job.submit(cache,"debug.sf2",owned.size("debug.sf2"),run==2?crc^1:crc));
   assert(!job.submit(cache,"debug.sf2",owned.size("debug.sf2"),crc));
   std::thread worker([&]{assert(job.process_one());});WavView mounted;bool ok=false;auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(10);
   while(!job.collect(mounted,ok)){assert(std::chrono::steady_clock::now()<limit);job.service(owned);std::this_thread::yield();}
   worker.join();assert(ok==(run==0));if(ok)assert(mounted.soundfont && soundfont_crc(mounted.soundfont)==crc);assert(!job.busy());
  }
  assert(owned.reads>0);
 }
 SoundFontVoice voice;voice.init(0,48000);SoundFontState state;unsigned allocations=soundfont_allocation_count();voice.trigger(view.soundfont,440,100,state);double energy=0;for(unsigned i=0;i<48000;++i){float l,r;voice.next(440,i<4000,l,r);assert(__builtin_isfinite(l) && __builtin_isfinite(r));energy+=l*l+r*r;}assert(energy>.01 && !voice.active() && soundfont_allocation_count()==allocations);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto engine=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*engine,*p);SessionFiles files(fs,library,*engine,*p,*ui);
 library.incremental=true;ui->set_mode(UiMode::Browser);copy_text(ui->browser().directory,sizeof ui->browser().directory,"");files.refresh();assert(ui->request_file(FileAction::Open) && files.service() && library.active);assert(ui->select_track(1));assert(!files.service() && !library.active && library.cancellations==1 && p->tracks[1].instrument==Instrument::Empty);assert(ui->select_track(0));files.service();files.refresh();assert(ui->request_file(FileAction::Open) && files.service());for(unsigned i=0;i<4;++i)files.service();assert(!library.active && p->tracks[0].instrument==Instrument::SoundFont);library.incremental=false;ui->set_mode(UiMode::Pad);float synced[2]{};engine->render(synced,0);ui->poll();
 assert(files.load_soundfont("debug.sf2"));float audio[128];engine->render(audio,0);auto t=track_at(*p,0,0);assert(t.instrument==Instrument::SoundFont && p->assets[t.sample_slot].metadata.content_crc);t.soundfont.values[0]=128;t.soundfont.values[1]=10;assert(files.save_preset("mine.mgxp"));assert(files.collect("Export","mine.mgxp"));assert(files.load_preset("Export/PRESETS/preset.mgxp"));engine->render(audio,0);assert(track_at(*p,0,0).soundfont.values[0]==128);assert(p->tracks[0].preset_slot<kPresetSlots && p->sounds[p->tracks[0].preset_slot].identity.assigned());assert(fs.remove("debug.sf2"));assert(files.load_preset("Export/PRESETS/preset.mgxp"));engine->render(audio,0);assert(files.load_preset("mine.mgxp"));assert(track_at(*p,0,0).instrument==Instrument::SoundFont);
 allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(!files.load_preset("Export/PRESETS/preset.mgxp") && track_at(*p,0,0).instrument==Instrument::SoundFont);assert(!ui->apply_preset_track(track_at(*p,0,0)));allocation_admission_hook=nullptr;
 Command note{CommandType::MidiInput,0,69,0x90};note.scalar=100;assert(engine->submit(note));double total=0;for(unsigned i=0;i<100;++i){engine->render(audio,64);for(float x:audio){assert(__builtin_isfinite(x));total+=x*x;}}assert(total>.01);
 // Long imported names still fit the 96-byte relative reference and the full bundle path.
 std::string long_name(55,'x'),bundle(45,'E');long_name+=".sf2";assert(fs.write(long_name.c_str(),0,kSoundFontFixture,sizeof kSoundFontFixture,true));assert(files.load_soundfont(long_name.c_str()));engine->render(audio,0);assert(files.save_preset("long.mgxp") && files.collect(bundle.c_str(),"long.mgxp"));std::string excessive(85,'E');assert(!files.collect(excessive.c_str(),"long.mgxp") && !fs.exists(excessive.c_str()));char bounded[160];std::string title(80,'a');title+=".mgxs";assert(SessionFiles::collect_directory(bounded,sizeof bounded,"PROJECTS",title.c_str()) && std::strlen(bounded)<=55);auto portable=std::make_unique<ProjectState>();std::string packed=bundle+"/PRESETS/preset.mgxp";assert(read_project_file(fs,packed.c_str(),*portable) && project_paths(*portable,packed.c_str(),true));assert(fs.exists(portable->assets[portable->tracks[0].sample_slot].path));
 std::filesystem::current_path(cwd);std::filesystem::remove_all(root);printf("SoundFont: render/release, bank/program, atomic mount, save/Collect/CRC/missing asset passed %.6f %.6f\n",energy,total);
}
