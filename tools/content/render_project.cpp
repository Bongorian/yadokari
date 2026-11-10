// Bounded host preview of a current project, using the real DSP and asset loader.
#include "engine.h"
#include "project_files.h"
#include "wav_writer.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cmath>
int main(int argc,char** argv){using namespace groove;if(argc!=4){std::fprintf(stderr,"usage: render-project SONG.mgxs OUTPUT.wav SECONDS(1..30)\n");return 2;}unsigned seconds=unsigned(std::strtoul(argv[3],nullptr,10));if(seconds<1 || seconds>30)return 2;
 const auto song=std::filesystem::absolute(argv[1]);const auto output=std::filesystem::absolute(argv[2]);std::filesystem::current_path(song.parent_path());DesktopFiles files;auto p=std::make_unique<ProjectState>();if(!read_project_file(files,song.filename().c_str(),*p) || p->content_kind!=ContentKind::Song || !project_paths(*p,song.filename().c_str(),true))return 3;
 DesktopSampleAssets assets;assets.files(&files);WavView views[kSampleAssets]{};if(!assets.prepare(*p,views)){std::fprintf(stderr,"Asset preparation: %s\n",assets.failure_reason());return 4;}auto engine=std::make_unique<Engine>();if(!engine->stage_project(*p,views) || !engine->submit({CommandType::Start,0,0,0}))return 5;
 unsigned frames=seconds*48000;uint8_t header[44];if(!wav_header(header,frames))return 6;FILE* out=std::fopen(output.c_str(),"wb");if(!out || std::fwrite(header,1,44,out)!=44)return 6;float audio[256];uint8_t pcm[512];double energy=0;float peak=0;unsigned nonzero=0;
 for(unsigned at=0;at<frames;at+=128){unsigned n=frames-at<128?frames-at:128;engine->render(audio,n);for(unsigned i=0;i<n*2;++i){float v=audio[i];if(!std::isfinite(v)){std::fclose(out);return 7;}energy+=double(v)*v;peak=std::fmax(peak,std::fabs(v));nonzero+=v!=0;int16_t value=pcm16(v);pcm[i*2]=uint8_t(value);pcm[i*2+1]=uint8_t(uint16_t(value)>>8);}if(std::fwrite(pcm,1,n*4,out)!=n*4){std::fclose(out);return 8;}}
 if(std::fclose(out)){return 8;}
 double rms=std::sqrt(energy/(frames*2));std::printf("current project preview: format=%u frames=%u peak=%.6f rms=%.6f nonzero=%u\n",kFormatVersion,frames,peak,rms,nonzero);return peak>.0001f && rms>.00001 && peak<1?0:9;
}
