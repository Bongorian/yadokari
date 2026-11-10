#include "benchmark_project.h"
#include "engine.h"
#include "session_files.h"
#include "wav_writer.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
int main(int argc,char** argv){
 if(argc!=2 && argc!=3){std::fprintf(stderr,"usage: render-showcase OUTPUT.wav [PROJECT.mgxs]\n");return 2;}
 groove::DesktopFiles files;
 auto project=std::make_unique<groove::ProjectState>();
 if(argc==3){if(!groove::read_project_file(files,argv[2],*project) || !groove::project_paths(*project,argv[2],true)){std::fprintf(stderr,"showcase project decode failed: %s\n",argv[2]);return 1;}}
 else groove::make_benchmark_project(*project);
 groove::DesktopSampleAssets library;library.files(&files);
 groove::WavView assets[groove::kSampleAssets]{};if(!library.prepare(*project,assets)){for(unsigned i=0;i<groove::kSampleAssets;++i)if(project->assets[i].path[0])std::fprintf(stderr,"sample slot %u: %s\n",i,project->assets[i].path);return 1;}
 auto engine=std::make_unique<groove::Engine>();if(!engine->stage_project(*project,assets) || !engine->submit({groove::CommandType::Start,0,0,0}))return 1;
 const unsigned frames=uint32_t((uint64_t(16)*4*60*48000+project->tempo/2)/project->tempo);
 FILE* output=std::fopen(argv[1],"wb");if(!output)return 1;uint8_t header[44];if(!groove::wav_header(header,frames) || std::fwrite(header,1,44,output)!=44)return 1;
 auto start=std::chrono::steady_clock::now();double energy=0;float peak=0;unsigned clipped=0,max_samples=0;float audio[256];uint8_t pcm[512];
 for(unsigned at=0;at<frames;){unsigned count=std::min(128u,frames-at);engine->render(audio,count);max_samples=std::max(max_samples,engine->active_sample_voices());for(unsigned i=0;i<count*2;++i){float v=audio[i];if(!std::isfinite(v))return 1;peak=std::max(peak,std::fabs(v));energy+=double(v)*v;if(std::fabs(v)>1)++clipped;uint16_t s=uint16_t(groove::pcm16(v));pcm[i*2]=uint8_t(s);pcm[i*2+1]=uint8_t(s>>8);}if(std::fwrite(pcm,1,count*4,output)!=count*4)return 1;at+=count;}
 if(std::fclose(output))return 1;
 auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::printf("showcase frames=%u seconds=%.2f peak=%.3f rms=%.3f clipped=%u max_sample_voices=%u host_render_s=%.2f\n",frames,double(frames)/48000,peak,std::sqrt(energy/(frames*2)),clipped,max_samples,elapsed);
 return peak>0.01f && std::sqrt(energy/(frames*2))>0.001 && clipped==0?0:1;
}
