#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>
#include <unistd.h>
using namespace groove;

class StemTestFiles : public DesktopFiles {
public:
 bool fail_payload=false;
 bool write(const char* path,uint64_t offset,const void* data,unsigned bytes,bool truncate) override{
  if(fail_payload && offset>=44 && std::strstr(path,"Group4.wav"))return false;
  return DesktopFiles::write(path,offset,data,bytes,truncate);
 }
};

int main(){
 constexpr unsigned rate=8000;
 const auto previous=std::filesystem::current_path();
 char root[]="/tmp/mk3-stems-XXXXXX";assert(mkdtemp(root) && chdir(root)==0);
 {
 StemTestFiles fs;DesktopSampleAssets lib;lib.files(&fs);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 p->tempo=120;p->song.enabled=true;p->song.loop=true;p->song.length=1;p->song.sections[0].bars=1;
 std::vector<int16_t> source(rate*3);
 for(unsigned i=0;i<source.size();++i)source[i]=i%32<16?8000:-8000;
 WavView views[kSampleAssets]{};
 views[0]={reinterpret_cast<const uint8_t*>(source.data()),unsigned(source.size()),rate,1,16,1,nullptr};
 auto& asset=p->assets[0];copy_text(asset.path,sizeof asset.path,"ASSETS/SAMPLES/Test.wav");
 asset.metadata.rate=rate;asset.metadata.frames=source.size();asset.metadata.bits=16;asset.metadata.channels=1;
 for(unsigned t=0;t<2;++t){
  auto& lane=p->audio[t];lane.kind=TrackKind::Audio;lane.clips[0].asset=0;lane.clips[0].frames=source.size();
 }
 p->routing.groups[0].members=1;
 auto engine=std::make_unique<Engine>(rate);
 auto ui=std::make_unique<Performance>(*engine,*p);
 SessionFiles files(fs,lib,*engine,*p,*ui);assert(engine->stage_project(*p,views));
 files.set_bindings(views);ui->project_loaded();ui->audio_available(true);
 float audio[128]{};engine->render(audio,0);ui->poll();files.service();
 ui->set_mode(UiMode::Project);ui->set_shift(true);
 auto start=[&]{
  assert(!std::strcmp(ui->soft_label(5),"SONG STEMS"));assert(ui->soft_key(5));files.service();
  assert(ui->song_stems_active() && !std::strcmp(ui->soft_label(5),"CANCEL STEMS"));
 };
 auto block=[&]{engine->render(audio,64);ui->poll();files.service();};
 // Cancel and rearm while the first Start is still in the DSP queue.
 start();assert(ui->soft_key(5));files.service();assert(!ui->song_stems_active());
 start();
 assert(fs.exists("RECORDINGS/Stems-001.tmp") && !fs.exists("RECORDINGS/Stems-001"));
 std::vector<int16_t> output;
 for(unsigned i=0;i<300 && ui->song_stems_active();++i){
  block();for(float sample:audio)output.push_back(pcm16(sample));
 }
 if(!fs.exists("RECORDINGS/Stems-001"))std::fprintf(stderr,"incomplete after %zu samples, playing %d, stem state %u, busy %d\n",output.size(),ui->playing(),unsigned(engine->stems().state()),ui->song_stems_active());
 assert(!ui->song_stems_active() && fs.exists("RECORDINGS/Stems-001"));
 unsigned count=0,first_length=0;
 for(const auto& entry:std::filesystem::directory_iterator("RECORDINGS/Stems-001")){
  const auto path=entry.path().string();uint8_t hdr[44]{},expected[44]{};unsigned got=0;
  unsigned frames=unsigned((fs.size(path.c_str())-44)/4);
  if(!count)first_length=frames;
  assert(frames==first_length);
  assert(frames>=2*rate-1 && frames<=2*rate+1 && wav_header(expected,frames,rate));
  assert(fs.read(path.c_str(),0,hdr,44,got) && got==44 && !std::memcmp(hdr,expected,44));
  std::vector<int16_t> wav(2*frames);assert(fs.read(path.c_str(),44,wav.data(),frames*4,got) && got==frames*4);
  bool sounding=std::any_of(wav.begin(),wav.end(),[](int16_t x){return x>100 || x< -100;});
  auto name=entry.path().filename().string();
  if(name=="Mix.wav")assert(std::equal(wav.begin(),wav.end(),output.begin()));
  assert(sounding==(name=="Direct.wav" || name=="Group1.wav" || name=="Mix.wav"));++count;
 }
 assert(count==11 && !fs.exists("RECORDINGS/Stems-001.tmp"));
 assert(ui->stop());block();start();block();assert(ui->stop());block();
 assert(!ui->song_stems_active() && !fs.exists("RECORDINGS/Stems-002.tmp") && !fs.exists("RECORDINGS/Stems-002"));
 start();fs.fail_payload=true;for(unsigned i=0;i<StemCapture::Frames/64+2 && ui->song_stems_active();++i)block();
 assert(!ui->song_stems_active() && !fs.exists("RECORDINGS/Stems-002.tmp") && !fs.exists("RECORDINGS/Stems-002"));
 assert(fs.exists("RECORDINGS/Stems-001/Mix.wav"));
 fs.fail_payload=false;assert(ui->stop());block();
 start();block();assert(ui->soft_key(5));files.service();block();
 assert(!ui->song_stems_active() && ui->playing() && !fs.exists("RECORDINGS/Stems-002.tmp"));
 assert(ui->stop());block();
 // Rearm before the DSP has consumed STOP or released the previous ring.
 start();block();assert(ui->soft_key(5));files.service();assert(ui->stop());
 start();block();assert(ui->song_stems_active() && ui->playing());
 assert(ui->stop());block();assert(!ui->song_stems_active() && !fs.exists("RECORDINGS/Stems-002.tmp"));
 // A full disk queue must terminate cleanly without publishing a short export.
 p->song.sections[0].bars=8;p->routing.groups[0].send[0]=1;p->effects.delay_mix=1;
 assert(engine->stage_project(*p,views));block();
 start();
 for(unsigned i=0;i<StemCapture::Frames*StemCapture::Blocks+128;i+=64)engine->render(audio,64);
 assert(engine->stems().state()==StemCapture::State::Overrun);
 files.service();assert(!ui->song_stems_active() && !fs.exists("RECORDINGS/Stems-002.tmp"));
 assert(ui->stop());block();
 p->song.sections[0].bars=1;assert(engine->stage_project(*p,views));block();
 start();for(unsigned n=0;n<300 && ui->song_stems_active();++n)block();
 assert(!ui->song_stems_active());
 const char* wet="RECORDINGS/Stems-002/Returns.wav";
 assert(fs.size(wet)>44);std::vector<int16_t> returns((fs.size(wet)-44)/2);unsigned got=0;assert(!returns.empty());
 assert(fs.read(wet,44,returns.data(),returns.size()*2,got) && got==returns.size()*2);
 assert(std::any_of(returns.begin(),returns.end(),[](int16_t x){return x>5 || x< -5;}));
 }
 std::filesystem::current_path(previous);std::filesystem::remove_all(root);
 std::puts("Song stems: front panel, one pass, eleven aligned WAVs, Main PCM, cancellation and media errors passed");
}
