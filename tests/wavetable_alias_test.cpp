#include "wavetable_voice.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace groove;
namespace {
constexpr unsigned N=8192;
using Audio=std::array<float,N>;
constexpr double pi=3.14159265358979323846;

// The pre-Wave-A linear reader and nominal-pitch mip rule, driven by the
// same phase map as the new voice. At zero warp this is the stock oscillator.
float linear_wave(unsigned a,unsigned mip,float phase){
 float x=phase*256;
 unsigned i=unsigned(x)&255;
 float lo=kWavetableData[a][mip][i],hi=kWavetableData[a][mip][(i+1)&255];
 return (lo+(hi-lo)*(x-floorf(x)))/32767;
}
float reference(float position,float phase,float step){
 float allowed=.45f/step;
 unsigned mip=0,harmonics=64;
 while(mip<6 && harmonics>allowed){++mip;harmonics>>=1;}
 float blend=std::clamp(2-allowed/harmonics,0.f,1.f);
 unsigned a=unsigned(position),b=std::min(a+1,127u);
 auto read=[&](unsigned level){
  float lo=linear_wave(a,level,phase);
  return lo+(linear_wave(b,level,phase)-lo)*(position-a);
 };
 float hi=read(mip);
 return hi+(read(std::min(6u,mip+1))-hi)*blend;
}

// Coherent FFT: stationary integer-ratio PM and sync can only generate
// multiples of the fundamental below Nyquist. Count all remaining bin
// power, including aliased harmonics that folded back into the passband.
double alias_db(const Audio& samples,unsigned bin){
 std::array<std::complex<double>,N> fft{};
 for(unsigned i=0,j=0;i<N;++i){
  fft[j]=samples[i];
  unsigned bit=N/2;
  while(bit && (j&bit)){j^=bit;bit>>=1;}
  j^=bit;
 }
 for(unsigned width=2;width<=N;width*=2){
  const auto rotate=std::polar(1.,-2*pi/width);
  for(unsigned first=0;first<N;first+=width){
   std::complex<double> phase(1.,0.);
   for(unsigned i=first;i<first+width/2;++i){
    auto a=fft[i],b=phase*fft[i+width/2];
    fft[i]=a+b;fft[i+width/2]=a-b;phase*=rotate;
   }
  }
 }
 double harmonic=0,other=0;
 for(unsigned i=1;i<=N/2;++i){
  double p=std::norm(fft[i])*(i==N/2?1:2);
  if(i%bin==0)harmonic+=p;else other+=p;
 }
 assert(harmonic+other>1e-8);
 return 10*std::log10(std::max(other/(harmonic+other),1e-30));
}

struct Setting {const char* name;unsigned mode;float warp,ratio,pm;};
struct Result {double linear,advanced,neutral;};
Result measure(const Setting& s,float position,unsigned bin){
 constexpr unsigned rate=48000;
 float step=float(bin)/N,hz=step*rate;
 float values[]={position,0,1,.001f,.005f,.001f,1,1,float(s.mode),s.warp,s.ratio,s.pm};
 WavetableVoice voice;
 voice.init(rate);voice.trigger(hz,1,values);
 // One full period both aligns phase and settles ADSR and IIR histories.
 for(unsigned i=0;i<N;++i)voice.next(hz,true);
 Audio old{},extended{},neutral{};
 for(unsigned i=0;i<N;++i){
  float p=float((i*bin)%N)/N;
  // Apply the same top-register controls so the comparison doesn't credit
  // differences in the effective sync ratio or modulation index.
  float warp=wavetable_limited_warp(s.mode,s.warp,step);
  float pm=wavetable_limited_pm(s.pm,step*s.ratio);
  auto phases=wavetable_read_phases(p,s.mode,warp,p*s.ratio,pm);
  float x=reference(position,phases.first,step);
  old[i]=x+phases.blend*(reference(position,phases.second,step)-x);
  neutral[i]=reference(position,p,step);
  extended[i]=voice.next(hz,true);
 }
 return {alias_db(old,bin),alias_db(extended,bin),alias_db(neutral,bin)};
}
}

// Optional deterministic sweep through low notes and the top octave. Keep
// the smaller coherent matrix in the normal sanitizer test budget.
static void extended_sweep(){
 uint32_t seed=1971;
 auto pick=[&](){seed=1664525u*seed+1013904223u;return seed>>5;};
 double worst=-300,neutral_margin=300,linear_margin=300;
 for(unsigned j=0;j<768;++j){
  Setting s{"sweep",pick()%4,float(int(pick()%201)-100)/100,float(1+pick()%8),float(pick()%101)/100};
  if(j%3==0)s.pm=0;
  if(j%5==0)s.warp=0;
  if(s.pm==0 && s.warp==0)s.warp=.37f;
  unsigned bin=3+2*(pick()%1810);
  float position=float(pick()%12700)/100;
  auto r=measure(s,position,bin);
  worst=std::max(worst,r.advanced);
  neutral_margin=std::min(neutral_margin,r.neutral-r.advanced);
  linear_margin=std::min(linear_margin,r.linear-r.advanced);
  assert(std::isfinite(r.advanced) && r.advanced<=r.linear && r.advanced<=r.neutral);
 }
 std::printf("768 additional stationary cases: worst %.2f dB, minimum improvement versus stock %.2f dB, versus matched linear %.2f dB\n",worst,neutral_margin,linear_margin);
}

int main(int argc,char** argv){
 assert(prepare_wavetable_bank());
 if(argc>1){assert(argc==2 && std::strcmp(argv[1],"--sweep")==0);extended_sweep();return 0;}
 constexpr Setting cases[]={{"Bend+",0,1,1,0},{"Bend-",0,-1,1,0},
  {"Sync+",1,.8f,1,0},{"Sync-",1,-.8f,1,0},{"PWM",2,1,1,0},
  {"Asym",3,-1,1,0},{"PM",0,0,2,.75f},{"Sync+PM",1,.7f,2,.75f}};
 for(auto s:cases){
  double worst=-300,worst_old=-300,worst_neutral=-300;
  for(float position:{0.f,10.5f,42.25f,97.5f})
   for(unsigned bin:{59u,223u,541u,997u,1531u}){
    auto r=measure(s,position,bin);
    assert(std::isfinite(r.advanced) && r.advanced<=r.linear && r.advanced<=r.neutral);
    assert(r.advanced < -75.);
    worst=std::max(worst,r.advanced);worst_old=std::max(worst_old,r.linear);worst_neutral=std::max(worst_neutral,r.neutral);
    std::printf("alias %-7s wave=%5.2f bin=%4u neutral=%8.2f linear=%8.2f extended=%8.2f dB\n",
                s.name,position,bin,r.neutral,r.linear,r.advanced);
   }
  std::printf("MAX %-7s neutral=%.2f linear=%.2f extended=%.2f dB\n",
              s.name,worst_neutral,worst_old,worst);
 }
 // Asymmetric lobe distortion close to Nyquist used to leak at -54 dB.
 auto top=measure({"high Asym",3,-.64f,5,0},119.11f,3607);
 assert(top.advanced<=top.neutral && top.advanced < -100);
 std::printf("High Asym regression: stock %.2f dB, matched linear %.2f dB, extended %.2f dB\n",top.neutral,top.linear,top.advanced);
}
