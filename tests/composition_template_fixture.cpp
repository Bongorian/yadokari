#include "project_files.h"
#include "wav_writer.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <memory>
#include <cstdio>
using namespace groove;
int main(int argc,char** argv){if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);DesktopFiles fs(true);for(const char* path:{"FACTORY","FACTORY/DEMOS","ASSETS","ASSETS/SAMPLES"})assert(fs.directory(path));
 uint8_t header[44];assert(wav_header(header,4800));assert(fs.write("ASSETS/SAMPLES/Test.wav",0,header,44,true));uint8_t pcm[4800*4];for(unsigned i=0;i<4800;++i){int16_t v=int16_t(std::sin(i*.1f)*12000*(1-float(i)/4800));for(unsigned c=0;c<2;++c){pcm[i*4+c*2]=uint8_t(v);pcm[i*4+c*2+1]=uint8_t(uint16_t(v)>>8);}}assert(fs.write("ASSETS/SAMPLES/Test.wav",44,pcm,sizeof pcm,false));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=300;copy_text(p->assets[0].path,sizeof p->assets[0].path,"../../ASSETS/SAMPLES/Test.wav");assert(file_checksum(fs,"ASSETS/SAMPLES/Test.wav",p->assets[0].metadata.content_crc));p->assets[0].metadata.rate=48000;p->assets[0].metadata.frames=4800;p->assets[0].metadata.channels=2;p->assets[0].metadata.bits=16;
 for(unsigned t=0;t<5;++t){p->tracks[t].instrument=t?Instrument::VirtualAnalog:Instrument::DrumKit;p->tracks[t].level=.3f;if(!t)p->tracks[t].kit.pads[0].sample[0]=0;for(unsigned pat=0;pat<3;++pat){p->patterns[pat].tracks[t].length=16;if(!t)p->patterns[pat].tracks[t].kit.steps[0][pat*4].enabled=true;else{auto& step=p->patterns[pat].tracks[t].steps[pat*4];step.enabled=true;step.note=uint8_t(48+t*3);}p->scenes[pat].pattern_for_track[t]=uint8_t(pat);}}
 p->song.length=5;p->song.loop=p->song.enabled=true;const unsigned refs[]={0,1,2,1,0},bars[]={4,8,8,8,4};for(unsigned i=0;i<5;++i){p->song.sections[i].scene=uint8_t(refs[i]);p->song.sections[i].bars=uint16_t(bars[i]);}
 for(unsigned i=0;i<3;++i){char path[96];std::snprintf(path,sizeof path,"FACTORY/DEMOS/fixture-%u.mgxs",i);assert(save_recoverable(fs,path,*p));}
 p->song.length=4;assert(save_recoverable(fs,"FACTORY/DEMOS/invalid-structure.mgxs",*p));p->song.length=5;p->tracks[4].level=0;assert(save_recoverable(fs,"FACTORY/DEMOS/invalid-silent.mgxs",*p));puts("Synthetic C fixtures only: no authored genres or factory masters.");
}
