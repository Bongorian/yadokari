// Deterministic rendering of one unchanged authored measurement score per process.
#include "engine.h"
#include "project_files.h"
#include "update/sha256.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstdio>
#include <cmath>
#include <thread>
#include <atomic>
#include <cstring>
using namespace groove;
static void finish(update::Sha256& hash,char (&out)[65]){uint8_t d[32];hash.finish(d);for(unsigned i=0;i<32;++i)std::snprintf(out+2*i,3,"%02x",d[i]);}
struct HelperThread {
 VaSampleSlot& helper;std::atomic<bool> stop{false};std::thread worker;
 HelperThread(VaSampleSlot& slot,bool enabled):helper(slot){if(enabled)worker=std::thread([&]{helper.worker_ready();while(!stop.load(std::memory_order_acquire)){if(!helper.process_one())std::this_thread::yield();}});}
 void close(){stop.store(true,std::memory_order_release);if(worker.joinable())worker.join();}
 ~HelperThread(){close();}
};
int main(int argc,char** argv){if(argc!=4 && argc!=5)return 2;const bool use_helper=argc==5 && !std::strcmp(argv[4],"--helper");if(argc==5 && !use_helper)return 2;std::filesystem::current_path(argv[1]);const std::string path=std::string("PROJECTS/DSP-EFF-T")+argv[2]+".mgxs";DesktopFiles files;auto p=std::make_unique<ProjectState>();if(!read_project_file(files,path.c_str(),*p))return 3;
 // CRC-first real-asset binding; original container bytes and musical values stay fixed.
 std::ifstream index("assets.tsv");std::string crc,name;while(index>>crc){std::getline(index,name);if(!name.empty() && name[0]=='\t')name.erase(0,1);uint32_t value=uint32_t(std::stoul(crc,nullptr,16));for(auto& a:p->assets)if(a.path[0] && a.metadata.content_crc==value && !copy_text(a.path,sizeof a.path,name.c_str()))return 4;}
 DesktopSampleAssets assets;assets.files(&files);WavView views[kSampleAssets]{};if(!assets.prepare(*p,views)){std::fprintf(stderr,"Real asset preparation failed: %s\n",assets.failure_reason());return 5;}
 stmlib::Random::Seed(p->random_seed);auto e=std::make_unique<Engine>();auto helper=std::make_unique<VaSampleSlot>();HelperThread helper_thread(*helper,use_helper);if(use_helper)e->set_va_helper(helper.get());if(!e->stage_project(*p,views) || !e->submit({CommandType::Start,0,0,0}))return 6;
 const uint64_t frames=uint64_t(48000)*60*4*48/p->tempo;float main[48],cue[48],input[48]{};update::Sha256 mh,ch;double energy=0;unsigned active=0,refused=0;unsigned native_seen=0,refused_seen=0;const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);std::ofstream blocks(output/"blocks.sha256");
 for(uint64_t at=0;at<frames;at+=24){const unsigned n=unsigned(frames-at<24?frames-at:24);e->render_ports(main,cue,input,n);mh.add(reinterpret_cast<uint8_t*>(main),n*2*sizeof(float));ch.add(reinterpret_cast<uint8_t*>(cue),n*2*sizeof(float));for(unsigned i=0;i<2*n;++i){if(!std::isfinite(main[i]) || !std::isfinite(cue[i]))return 7;energy+=double(main[i])*main[i];}active=e->native_active_mask();refused=e->native_refused_mask();native_seen|=active;refused_seen|=refused;if(at%48000==0){update::Sha256 h;h.add(reinterpret_cast<uint8_t*>(main),n*2*sizeof(float));char hex[65];finish(h,hex);blocks<<at<<' '<<hex<<'\n';}EngineEvent event;while(e->pop_event(event)){}ClockEvent clock;while(e->pop_clock(clock)){}MidiEvent midi;while(e->pop_midi(midi)){}AudioVisualFrame visual;while(e->pop_visual(visual)){}assets.service();}
 helper_thread.close();auto help=helper->counts();std::fprintf(stderr,"VA helper submitted %llu used %llu late %llu busy %llu checked %llu mismatch %llu\n",(unsigned long long)help.submitted,(unsigned long long)help.used,(unsigned long long)help.late,(unsigned long long)help.busy,(unsigned long long)help.checked,(unsigned long long)help.mismatch);if(help.mismatch)return 8;
 char main_hash[65],cue_hash[65];finish(mh,main_hash);finish(ch,cue_hash);std::printf("{\"track\":%s,\"frames\":%llu,\"rate\":48000,\"block\":24,\"input\":\"digital-zero\",\"main_sha256\":\"%s\",\"cue_sha256\":\"%s\",\"energy\":%.17g,\"native_active_seen\":%u,\"native_refused_seen\":%u,\"event_dropped\":%u}\n",argv[2],(unsigned long long)frames,main_hash,cue_hash,energy,native_seen,refused_seen,e->event_dropped());
}
