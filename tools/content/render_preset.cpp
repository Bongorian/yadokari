// Deterministic real-DSP preset render; one process per file resets global DSP state.
#include "engine.h"
#include "project_files.h"
#include "update/sha256.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cmath>
using namespace groove;
static void finish(update::Sha256& hash,char (&out)[65]){uint8_t d[32];hash.finish(d);for(unsigned i=0;i<32;++i)std::snprintf(out+2*i,3,"%02x",d[i]);}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: render-preset DATA_ROOT PRESET_RELATIVE_PATH\n");return 2;}
 std::filesystem::current_path(argv[1]);DesktopFiles files;auto p=std::make_unique<ProjectState>();
 if(!read_project_file(files,argv[2],*p) || p->content_kind!=ContentKind::Preset || !project_paths(*p,argv[2],true))return 3;
 if(!verify_project_assets(files,*p))return 4;
 DesktopSampleAssets library;library.files(&files);WavView views[kSampleAssets]{};
 if(!library.prepare(*p,views)){std::fprintf(stderr,"Assets: %s\n",library.failure_reason());return 5;}
 stmlib::Random::Seed(p->random_seed);auto e=std::make_unique<Engine>();if(!e->stage_project(*p,views))return 6;
 float main[48],cue[48],input[48];e->render_ports(main,cue,input,0);
 Command on{CommandType::Trigger,0,0,100};on.parameter=1;on.scalar=60;if(!e->submit(on))return 6;
 constexpr unsigned frames=60000;update::Sha256 mh,ch;double energy=0;float peak=0;unsigned nonzero=0;
 for(unsigned at=0;at<frames;at+=24){
  if(at==48000 && !e->submit({CommandType::NoteOff,0,0,0}))return 6;
  for(unsigned i=0;i<24;++i){input[2*i]=.03f*std::sin(float(at+i)*.0575958653f);input[2*i+1]=.02f*std::sin(float(at+i)*.0863937979f);}
  e->render_ports(main,cue,input,24);mh.add(reinterpret_cast<uint8_t*>(main),sizeof main);ch.add(reinterpret_cast<uint8_t*>(cue),sizeof cue);
  for(float v:main){if(!std::isfinite(v))return 7;energy+=double(v)*v;peak=std::fmax(peak,std::fabs(v));nonzero+=v!=0;}
  EngineEvent event;while(e->pop_event(event)){}ClockEvent clock;while(e->pop_clock(clock)){}MidiEvent midi;while(e->pop_midi(midi)){}AudioVisualFrame visual;while(e->pop_visual(visual)){}library.service();
 }
 char a[65],b[65];finish(mh,a);finish(ch,b);
 std::printf("{\"frames\":%u,\"main_sha256\":\"%s\",\"cue_sha256\":\"%s\",\"rms\":%.17g,\"peak\":%.9g,\"nonzero\":%u}\n",frames,a,b,std::sqrt(energy/(frames*2)),peak,nonzero);
}
