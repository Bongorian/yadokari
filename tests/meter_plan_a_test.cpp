#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>
using namespace groove;
static unsigned count(const SoftwareCanvas& c,unsigned side,int x,int y,int w,int h,uint32_t color){unsigned n=0;for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)n+=c.pixels[side][row*480+col]==color;return n;}
static void capture(const SoftwareCanvas& c,const std::filesystem::path& path){FILE* f=std::fopen(path.c_str(),"wb");assert(f);std::fputs("P6\n960 272\n255\n",f);for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t color=c.pixels[x/480][y*480+x%480];uint8_t rgb[]={uint8_t(color>>16),uint8_t(color>>8),uint8_t(color)};assert(std::fwrite(rgb,1,3,f)==3);}assert(!std::fclose(f));}
// Frozen v558 geometry/colors: an independent before-image for allowed deltas
// and retained/increased danger ink. Not the new color-policy implementation.
static void legacy_meter(ui::Widgets& c,int x,int y,int w,int h,float level,bool horizontal){c.rounded(0,x,y,w,h,3,c.theme.raised);for(unsigned n=0;n<12;++n){if(float(n)/12>=ui::clamp01(level))break;uint32_t color=n>10?c.theme.red:n>8?c.theme.amber:c.theme.motion;if(horizontal)c.rounded(0,x+int(n)*w/12,y,w/12-2,h,1,color);else c.rounded(0,x,y+h-(int(n)+1)*h/12,w,h/12-2,1,color);}}
static float amount(float peak){return ui::clamp01((20*log10f(peak+.00000001f)+60)/60);}
int main(int argc,char** argv){
 auto live=std::make_unique<ui::Widgets>(),old=std::make_unique<ui::Widgets>();std::filesystem::path out=argc>1?argv[1]:"";if(argc>1)std::filesystem::create_directories(out);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->ui.motion=uint8_t(ui::Motion::Off);const Instrument inst[]={Instrument::PolySaw,Instrument::Sampler,Instrument::DrumKit,Instrument::Layer,Instrument::PolySaw,Instrument::ExternalMidi,Instrument::Empty};for(unsigned i=0;i<7;++i)p->tracks[i].instrument=inst[i];p->audio[4].kind=TrackKind::Audio;
 auto engine=std::make_unique<Engine>();assert(engine->stage_project(*p));auto perf=std::make_unique<Performance>(*engine,*p);float pcm[128]{},input[128]{};assert(engine->submit({CommandType::Trigger,0,0,127}));for(unsigned block=0;block<200;++block){for(unsigned n=0;n<64;++n){float v=.04f*sinf(float(block*64+n)*.057f);input[2*n]=v;input[2*n+1]=v*.5f;assert(engine->receive_audio(input[2*n],input[2*n+1]));}engine->render(pcm,64);perf->animate((block+1)*64/48);perf->poll();}
 assert(perf->visual().peak[0]>0);auto screen=std::make_unique<UiRenderer>();unsigned checks=0;
 for(unsigned ti=0;ti<ui::Theme::kCount;++ti){const auto& theme=ui::Theme::by_index(ti);live->theme=old->theme=theme;
  for(unsigned kind=0;kind<7;++kind){const auto source=ui::track_color(*p,kind,theme);assert(source==ui::engine_kind_color(ui::EngineKind(kind),theme) && source!=theme.motion && source!=theme.raised);
   for(bool horizontal:{false,true})for(float level:{0.f,.5f,.8f,1.f,1.2f}){live->fill(0,0,0,480,272,theme.background);old->fill(0,0,0,480,272,theme.background);int w=horizontal?144:6,h=horizontal?6:144;live->meter(0,24,40,w,h,level,source,horizontal);legacy_meter(*old,24,40,w,h,level,horizontal);
    assert(!count(*live,0,24,40,w,h,theme.motion));assert(count(*live,0,24,40,w,h,theme.danger)==count(*old,0,24,40,w,h,theme.danger));assert(count(*live,0,24,40,w,h,theme.warn)==count(*old,0,24,40,w,h,theme.warn));if(level>0)assert(count(*live,0,24,40,w,h,source)>0);
    for(unsigned i=0;i<480*272;++i){if(live->pixels[0][i]==old->pixels[0][i])continue;int px=int(i%480),py=int(i/480);bool normal=false;for(unsigned n=0;n<9 && float(n)/12<ui::clamp01(level);++n){int sx=horizontal?24+int(n)*w/12:24,sy=horizontal?40:40+h-(int(n)+1)*h/12,sw=horizontal?w/12-2:w,sh=horizontal?h:h/12-2;normal|=px>=sx && px<sx+sw && py>=sy && py<sy+sh;}assert(normal);}++checks;
   }
  }
  live->fill(0,0,0,480,272,theme.background);live->fill(1,0,0,480,272,theme.background);
  for(unsigned kind=0;kind<7;++kind){int x=10+int(kind)*58;auto source=ui::track_color(*p,kind,theme);char label[12];std::snprintf(label,sizeof label,"%u",kind+1);live->mixer_strip(0,x,50,52,128,label,.7f,0,.6f,"LIVE",false,source,.8f);int mx=x+23,my=74,py=my+74-10*74/12;
   assert(count(*live,0,mx-2,py,10,2,theme.motion)==20);assert(count(*live,0,mx,my,6,74,source)>0);for(int yy=my;yy<my+74;++yy)if(yy<py || yy>=py+2)assert(!count(*live,0,mx,yy,6,1,theme.motion));
   live->mixer_strip(1,x,50,52,128,label,.7f,0,1,"CLIP",false,source,1);assert(count(*live,1,mx-2,my,10,2,theme.danger)==20);assert(!count(*live,1,mx-2,my,10,74,theme.motion));
   // The old white held marker masked the top 2px of a clipped bar.
   old->fill(0,0,0,480,272,theme.background);legacy_meter(*old,mx,my,6,74,1,false);old->fill(0,mx-2,my,10,2,theme.text);assert(count(*live,1,mx-2,my,10,74,theme.danger)>=count(*old,0,mx-2,my,10,74,theme.danger));
  }
  for(unsigned side=0;side<2;++side){live->label(side,14,16,side?"PLAN A / HELD CLIP = DANGER":"PLAN A / KIND BODY + MOTION HOLD",theme.text,ui::FontRole::Small,450);live->label(side,14,194,"WIDGET FIXTURE / normalized level and hold inputs",theme.secondary,ui::FontRole::Small,450);}
  for(unsigned n=0;n<3;++n){float a=n==0?.5f:n==1?.8f:1;uint32_t expected=n==0?theme.teal:n==1?theme.warn:theme.danger;live->level_meter(0,24+int(n)*146,221,125,4,a,theme.teal);assert(count(*live,0,24+int(n)*146,221,125,4,expected)>0 && !count(*live,0,24+int(n)*146,221,125,4,theme.motion));}
  if(argc>1)capture(*live,out/(std::to_string(ti)+"-widget.ppm"));
  auto settings=perf->device_settings();settings.theme=uint8_t(ti);assert(perf->set_device_settings(settings));perf->set_mode(UiMode::Mixer);screen->invalidate();screen->draw(*p,*perf);
  const uint32_t source=ui::track_color(*p,0,theme);int y0=64+int(lroundf(float(103-76)*ui::Layout::body_h/131));assert(count(*screen,0,33,y0,6,100,source)>0);
  auto master=amount(perf->visual().master_peak);uint32_t master_color=master>11.f/12?theme.danger:master>9.f/12?theme.warn:theme.teal;assert(count(*screen,1,385,54,85,4,master_color)>0 && !count(*screen,1,385,54,85,4,theme.motion));
  if(argc>1)capture(*screen,out/(std::to_string(ti)+"-mixer.ppm"));
 }
 // Header input and master overload must also use danger, not the neutral
 // ink or motion. These are real Engine input/limiter values, not UI injection.
 p->routing.monitor=1;p->routing.monitor_bus=1;p->routing.main=1;assert(engine->stage_project(*p));engine->render(pcm,0);perf->project_loaded();for(unsigned block=0;block<200;++block){for(unsigned n=0;n<64;++n)assert(engine->receive_audio(1.5f,1.5f));engine->render(pcm,64);perf->animate(1000+(block+1)*64/48);perf->poll();}assert(perf->visual().input_lr[0]>=1 && amount(perf->visual().master_peak)>11.f/12);
 for(unsigned ti=0;ti<ui::Theme::kCount;++ti){const auto& theme=ui::Theme::by_index(ti);auto settings=perf->device_settings();settings.theme=uint8_t(ti);assert(perf->set_device_settings(settings));perf->set_mode(UiMode::Mixer);screen->invalidate();screen->draw(*p,*perf);assert(count(*screen,1,258,51,76,3,theme.danger)>0 && count(*screen,1,258,57,76,3,theme.danger)>0 && count(*screen,1,385,54,85,4,theme.danger)>0);assert(!count(*screen,1,385,54,85,4,theme.motion));if(argc>1)capture(*screen,out/(std::to_string(ti)+"-clip.ppm"));}
 std::printf("Meter Plan A: %u normal/warn/clip pixel cases,9 themes x7 kinds, held-only motion/clip priority, unchanged geometry and real DSP MIXER/header frames passed\n",checks);
}
