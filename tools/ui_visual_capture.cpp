#include "ui_renderer.h"
#include "future_factory_presets.h"
#include <memory>
#include <filesystem>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using namespace groove;
static void require(bool ok){if(!ok)throw std::runtime_error("visual fixture setup failed");}
int main(int argc,char** argv){
 const bool retry=argc==2 && !std::strcmp(argv[1],"texture-spectrum");if(argc!=1 && !retry)return 2;const char* evidence=std::getenv("MGX_VISUAL_EVIDENCE_ROOT");std::string directory_name=(std::filesystem::path(evidence?evidence:"build/wave8-followup")/(retry?"motion-texture-retry":"motion")).string();const char* directory=directory_name.c_str();
 std::filesystem::create_directories(directory);FILE* index=std::fopen((std::filesystem::path(directory)/"frames.tsv").c_str(),"w");require(index);std::fprintf(index,"case\tframe\tsample\tactive\thero_hash\tpeak\n");
 const char* names[]={"additive-bars","wavetable-curve","texture-spectrum","va-scope","vocoder-bars","looper-ring","compressor-response"};
 for(unsigned fixture=0;fixture<7;++fixture){if(retry && fixture!=2)continue;
  auto p=std::make_unique<ProjectState>();require(make_future_factory_preset(*p,fixture==0?11:fixture==1?0:4));p->content_kind=ContentKind::Song;p->ui.motion=1;
  if(fixture==2){p->tracks[0].instrument=Instrument::Texture;p->tracks[0].texture.values[1]=0;p->tracks[0].texture.values[2]=0;p->tracks[0].texture.values[4]=.8f;p->tracks[0].texture.values[6]=.5f;}
  if(fixture==4){p->tracks[0].insert_fx[0]=19;p->tracks[0].insert_fx[1]=0;p->tracks[0].insert_fx[2]=0;p->tracks[0].insert_fx[3]=1;}
  if(fixture==6){p->tracks[0].compressor[0]=.06f;p->tracks[0].compressor[1]=6;p->tracks[0].compressor[3]=10;p->tracks[0].compressor[5]=1;}
  auto e=std::make_unique<Engine>();auto u=std::make_unique<Performance>(*e,*p);auto screen=std::make_unique<UiRenderer>();require(e->stage_project(*p));u->project_loaded();u->set_mode(UiMode::Synth);
  std::vector<int16_t> loop(48000);for(unsigned i=0;i<loop.size();++i)loop[i]=int16_t(12000*sinf(6.283185307f*220*i/48000));
  if(fixture==5){AudioLane lane;lane.kind=TrackKind::Looper;lane.clips[0].asset=0;lane.clips[0].frames=loop.size();WavView view{reinterpret_cast<const uint8_t*>(loop.data()),uint32_t(loop.size()),48000,1};SampleMetadata metadata;metadata.frames=loop.size();metadata.rate=48000;metadata.bits=16;metadata.channels=1;require(u->set_audio_lane(0,lane,false,0,view,metadata));u->set_mode(UiMode::Perform);for(unsigned i=0;i<5;++i)u->encoder_direction(0x08);require(u->audio_page());require(u->play());}
  else{Command note{CommandType::MidiInput,0,60,0x90};note.scalar=110;require(e->submit(note));}
  if(fixture==4 || fixture==6){u->set_mode(UiMode::Effects);require(u->soft_key(fixture==4?3:2));}
  uint64_t sample=0;float audio[256],input[256];
  // Fill the real granular input buffer before documenting it; neutral density
  // and an empty delayed buffer are not an audible granular fixture.
  if(fixture==2)for(unsigned remaining=48000;remaining;){unsigned n=remaining>128?128:remaining;for(unsigned i=0;i<n;++i)input[2*i]=input[2*i+1]=.4f*sinf(6.283185307f*300*(sample+i)/48000);e->render_ports(audio,nullptr,input,n);sample+=n;u->animate(sample/48);u->poll();remaining-=n;}

  for(unsigned frame=0;frame<24;++frame){if(frame==12)require(e->submit({CommandType::Stop,0,0,0}));for(unsigned remaining=1600;remaining;){unsigned n=remaining>128?128:remaining;for(unsigned i=0;i<n;++i){float value=frame<12?.4f*sinf(6.283185307f*300*(sample+i)/48000):0;input[2*i]=input[2*i+1]=value;}e->render_ports(audio,nullptr,input,n);sample+=n;u->animate(sample/48);u->poll();remaining-=n;}u->notify(UiNotice::None);screen->draw(*p,*u);unsigned side=1;uint64_t hash=1469598103934665603ull;for(int y=82;y<242;++y)for(int x=20;x<460;++x)hash=(hash^screen->pixels[side][y*480+x])*1099511628211ull;std::fprintf(index,"%s\t%u\t%llu\t%u\t%llu\t%.9f\n",names[fixture],frame,(unsigned long long)sample,frame<12,(unsigned long long)hash,u->visual().peak[0]);
   char path[160];std::snprintf(path,sizeof path,"%s/%s-%02u.ppm",directory,names[fixture],frame);FILE* file=std::fopen(path,"wb");require(file);std::fprintf(file,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t pixel=screen->pixels[x/480][y*480+x%480];uint8_t rgb[]={uint8_t(pixel>>16),uint8_t(pixel>>8),uint8_t(pixel)};require(std::fwrite(rgb,1,3,file)==3);}require(!std::fclose(file));
  }
 }
 require(!std::fclose(index));std::puts(retry?"Wave8 Texture-only retry: actual granular buffer primed, density .8, dry/wet .5; originals preserved":"Wave8 actual DSP capture: seven fixtures, 24 frames each, stop at frame 12; no hardware or user files changed");
}
