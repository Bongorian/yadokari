// Fixed host baseline. Timings are host-only, never a Pi deadline claim.
#include "engine.h"
#include "dsp_probe_workload.h"
#include "soundfont_fixture.h"
#include "update/sha256.h"
#include "../../platform/desktop/file_access.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
using namespace groove;
static uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static uint32_t stamp(){return uint32_t(now());}
static void digest(update::Sha256& h,char out[65]){uint8_t d[32];h.finish(d);for(unsigned i=0;i<32;++i)std::snprintf(out+2*i,3,"%02x",d[i]);}
int main(int argc,char** argv){
 if(argc<2 || argc>3){return 2;}std::setvbuf(stdout,nullptr,_IONBF,0);std::filesystem::create_directories(argv[1]);
 DesktopFiles fs;const char* sf="build/optimization-refactor/fixture.sf2";if(!fs.write(sf,0,kSoundFontFixture,sizeof kSoundFontFixture,true))return 2;SoundFontCache fonts;WavView sfview;if(!fonts.load(fs,sf,sfview))return 3;
 std::printf("{\"revision\":1,\"rate\":48000,\"block_frames\":24,\"engine_count\":%u,\"engine_bytes\":%zu,\"slot_bytes\":%zu,\"engines\":[\n",instrument_count(),sizeof(Engine),sizeof(TrackInsertSlot));
 unsigned first=argc==3?unsigned(std::strtoul(argv[2],nullptr,10)):0;for(unsigned id=first;id<instrument_count();++id){auto p=std::make_unique<ProjectState>();make_dsp_probe_project(*p,{DspCondition::Leaf,Instrument(id),0,1,1});auto t=track_at(*p,0,0);t.length=7;t.root_note=60;p->clips[0].playing=true;if(Instrument(id)==Instrument::Texture)t.texture.values[12]=1;
  for(unsigned n=0;n<7;++n){auto& s=t.steps[n];s.enabled=n!=3;s.velocity=uint8_t(75+n*6);s.note=uint8_t(60+(n*5)%12);s.gate=uint16_t(n%2?48:96);}
  std::vector<uint8_t> sample(1024);for(unsigned n=0;n<512;++n){int16_t v=(n%128)<64?8192:-8192;sample[2*n]=uint8_t(v);sample[2*n+1]=uint8_t(uint16_t(v)>>8);}WavView views[kSampleAssets]{};views[0]={sample.data(),512,48000,1,16,1};views[4]=sfview;
  if(Instrument(id)==Instrument::DrumKit){t.kit.pads[0].sample[0]=0;t.kit.steps[0][0].enabled=true;t.kit.steps[0][0].velocity=100;}
  stmlib::Random::Seed(0x57313001);auto e=std::make_unique<Engine>();bool staged=e->stage_project(*p,views);if(!staged)return 4;e->dsp_profile().begin(stamp,e->staged_generation());e->submit({CommandType::Start,0,0,0});float pcm[48]{},input[48];for(unsigned n=0;n<48;++n)input[n]=float(int(n%9)-4)/32;
  update::Sha256 hash;double energy=0;uint64_t max_ns=0,total=0;for(unsigned n=0;n<2000;++n){uint64_t at=now();e->render_ports(pcm,nullptr,input,24);uint64_t elapsed=now()-at;total+=elapsed;if(elapsed>max_ns)max_ns=elapsed;hash.add(reinterpret_cast<uint8_t*>(pcm),sizeof pcm);for(float x:pcm){if(!std::isfinite(x))return 5;energy+=double(x)*x;}}
  char sum[65],path[256];digest(hash,sum);std::snprintf(path,sizeof path,"%s/engine-%02u.sha256",argv[1],id);std::ofstream(path)<<sum<<"\n";
  e->dsp_profile().stop();e->render(pcm,0);const auto& counter=e->dsp_profile().counter(DspProfile::voice(id,0));
  auto* d=engine_descriptor(id);std::printf("%s{\"id\":%u,\"guid\":\"%s\",\"cost\":%u,\"declared_voice_capacity\":%u,\"parameters\":%u,\"active_frames\":%llu,\"voice_max24_us\":%.6f,\"max_host_ns\":%llu,\"total_host_ns\":%llu,\"energy\":%.12g,\"sha256\":\"%s\"}",id!=first?",\n":"",id,d->identity.guid,engine_voice_cost(Instrument(id),0),instrument_voice_limit(Instrument(id)),d->parameters.count,(unsigned long long)counter.frames,double(counter.max_voice_q16)*24/(65536*1000),(unsigned long long)max_ns,(unsigned long long)total,energy,sum);
 }
 std::printf("\n],\"fx\":[\n");
 for(unsigned type=0;type<kFxCount;++type){auto fx=std::make_unique<TrackInsertSlot>();std::vector<float> mod(TrackInsertSlot::kModFloats);fx->bind_modulation(mod.data());fx->init(48000);update::Sha256 hash;uint64_t max_ns=0,total=0;float in[48];for(unsigned n=0;n<48;++n)in[n]=float(int(n%13)-6)/16;
  for(unsigned n=0;n<2000;++n){float block[48];__builtin_memcpy(block,in,sizeof block);uint64_t at=now();for(unsigned j=0;j<24;++j)fx->process(block[2*j],block[2*j+1],type,.7f,.4f,.75f,.12f,-.08f);uint64_t elapsed=now()-at;total+=elapsed;if(elapsed>max_ns)max_ns=elapsed;hash.add(reinterpret_cast<uint8_t*>(block),sizeof block);}char sum[65];digest(hash,sum);std::printf("%s{\"type\":%u,\"guid\":\"%s\",\"cost\":%u,\"max_host_ns\":%llu,\"total_host_ns\":%llu,\"sha256\":\"%s\"}",type?",\n":"",type,fx_descriptor(type)->guid,fx_descriptor(type)->cost,(unsigned long long)max_ns,(unsigned long long)total,sum);
 }
 std::printf("\n]}\n");return 0;
}
