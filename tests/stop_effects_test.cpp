#include "live_effects.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main(){
 groove::StereoDelay delay(8000);
 float l=0,r=0;
 delay.process(0.8f,-0.4f,0.7f,10,l,r);
 for(unsigned i=0;i<200;++i)delay.process(0,0,0.7f,10,l,r);
 delay.reset();
 for(unsigned i=0;i<200;++i){delay.process(0,0,0.7f,10,l,r);assert(l==0 && r==0);}
 // Opposite-polarity taps must crossfade when delay time changes live.
 delay.reset();
 for(unsigned i=0;i<340;++i){float x=std::sin(float(i)*6.2831853f/80);delay.process(x,x,0,10,l,r);}
 float previous=l;
 delay.process(1,1,0,5,l,r);assert(std::abs(l-previous)<0.1f && l==r);
 for(unsigned i=0;i<160;++i){delay.process(0,0,2,5,l,r);assert(std::isfinite(l) && std::isfinite(r));}
 groove::StereoReverb reverb(8000);
 for(unsigned i=0;i<2000;++i)reverb.process(i==0?1:0,0,0.7f,0.5f,l,r);
 reverb.reset();
 for(unsigned i=0;i<2000;++i){reverb.process(0,0,0.7f,0.5f,l,r);assert(l==0 && r==0);}
 groove::MasterCapture history(8000);
 groove::PerformanceFxBus bus(8000);
 for(unsigned mode=1;mode<=unsigned(groove::PerformanceFx::PitchRise);++mode){
  bus.set(groove::PerformanceFx(mode),0.8f);
  for(unsigned i=0;i<8192;++i){
   l=0.2f*std::sin(float(i)*0.1f);r=-l*0.7f;
   bus.process(l,r,history,300);
   assert(std::isfinite(l) && std::isfinite(r) && std::abs(l)<=2 && std::abs(r)<=2);
   history.push(0.2f*std::sin(float(i)*0.1f),-0.14f*std::sin(float(i)*0.1f));
  }
 }
 bus.set(groove::PerformanceFx::Off,0);
 for(unsigned i=0;i<2048;++i){l=0.13f;r=-0.09f;bus.process(l,r,history,120);}
 assert(std::abs(l-0.13f)<0.001f && std::abs(r+0.09f)<0.001f);
 bus.reset();assert(bus.delay_send()==0 && bus.reverb_send()==0);
 auto release_ratio=[](unsigned rate,groove::PerformanceFx mode,bool tail,unsigned duration){groove::PerformanceFxBus effect(rate);groove::MasterCapture source(rate);effect.set_return_tail(tail);effect.set(mode,1);float a=0,b=0;for(unsigned i=0;i<rate/10;++i){a=0.1f;b=-0.1f;effect.process(a,b,source,120);}float before=mode==groove::PerformanceFx::DelayThrow?effect.delay_return():effect.reverb_return();assert(before>0.7f);effect.set(groove::PerformanceFx::Off,1);for(unsigned i=0;i<duration;++i){a=b=0;effect.process(a,b,source,120);}float after=mode==groove::PerformanceFx::DelayThrow?effect.delay_return():effect.reverb_return();return after/before;};
 for(auto mode:{groove::PerformanceFx::DelayThrow,groove::PerformanceFx::ReverbThrow}){float low=release_ratio(8000,mode,true,8000),high=release_ratio(48000,mode,true,48000);assert(std::abs(low-high)<0.003f && low>0.35f && low<0.39f);assert(release_ratio(8000,mode,false,1600)<0.001f);}
 // A throw line hears only explicitly thrown input. A later unrelated signal
 // passes unchanged even while the throw return is audible.
 groove::PerformanceThrows throws(8000);l=0.5f;r=-0.25f;throws.process(l,r,1,0,1,0,0,10,0.6f,0.5f);
 for(unsigned i=1;i<80;++i){l=0.1f;r=0.2f;throws.process(l,r,0,0,1,0,0,10,0.6f,0.5f);assert(std::abs(l-0.1f)<0.0001f && std::abs(r-0.2f)<0.0001f);}
 l=0.1f;r=0.2f;throws.process(l,r,0,0,1,0,0,10,0.6f,0.5f);assert(std::abs(l-0.6f)<0.0001f && std::abs(r+0.05f)<0.0001f);
 throws.reset();for(unsigned i=0;i<200;++i){l=0.1f;r=0.2f;throws.process(l,r,0,0,1,0,0.9f,10,0.6f,0.5f);assert(std::abs(l-0.1f)<0.0001f && std::abs(r-0.2f)<0.0001f);}
 throws.reset();bool reverb_heard=false;for(unsigned i=0;i<4000;++i){l=i?0:0.5f;r=0;throws.process(l,r,0,i?0:1,0,1,0,10,0.6f,0.5f);if(i && std::abs(l)>0.000001f)reverb_heard=true;assert(std::isfinite(l) && std::isfinite(r));}assert(reverb_heard);throws.reset();
 float early=0,late=0;for(unsigned i=0;i<20000;++i){l=i<1600?0.3f*std::sin(float(i)*0.08f):0;r=l;throws.process(l,r,0,1,0,1,0,10,0.6f,0.5f,true);assert(std::isfinite(l) && std::isfinite(r) && std::abs(l)<4 && std::abs(r)<4);if(i>=4000 && i<8000)early+=std::abs(l);if(i>=16000)late+=std::abs(l);}assert(early>0.1f && late>early*0.2f && late<early*5);throws.reset();
 groove::MasterCapture repeat_history(8000);groove::BeatRepeat repeat;
 for(unsigned i=0;i<1000;++i)repeat_history.push(float(i)/1000,-float(i)/1000);
 float loop[1000]{};
 for(unsigned i=0;i<3000;++i){l=r=0;assert(repeat.process(l,r,repeat_history,8000,120,true));if(i>=2000)loop[i-2000]=l;}
 for(unsigned i=0;i<1000;++i){l=r=0;assert(repeat.process(l,r,repeat_history,8000,120,true));assert(std::abs(l-loop[i])<0.0001f);}
 bool frozen=true;for(unsigned i=0;i<1000;++i){l=r=0;frozen=repeat.process(l,r,repeat_history,8000,120,false);}assert(!frozen);
 for(unsigned division=2;division<=16;division*=2){
  groove::MasterCapture source(8000);groove::BeatRepeat fx;
  for(unsigned i=0;i<4000;++i)source.push(float(i%1000)/1000,0);
  unsigned length=unsigned(8000*60/(120*division));
  for(unsigned i=0;i<length*2;++i){l=r=0;assert(fx.process(l,r,source,8000,120,true,division));}
  float a=0,b=0;assert(fx.process(a,b,source,8000,120,true,division));
  for(unsigned i=1;i<length;++i){l=r=0;assert(fx.process(l,r,source,8000,120,true,division));}
  l=r=0;assert(fx.process(l,r,source,8000,120,true,division));assert(std::abs(l-a)<0.0001f);
 }
}
