#pragma once
#include <stdint.h>
#include <math.h>
namespace groove {
constexpr unsigned kVisualPcm=512,kVisualScope=128,kVisualBands=64;
// Three actual memory taps, no asset pointers and no filesystem service.
struct VisualPcmFrame {uint64_t sample=0;uint32_t generation=0,rate=48000;uint8_t track=0,source=255;float pcm[3][kVisualPcm]{};};
struct VisualAnalysisFrame {uint64_t sample=0;uint32_t generation=0,rate=48000;uint8_t track=0,source=255;float scope_low[3][kVisualScope]{},scope_high[3][kVisualScope]{},spectrum[3][kVisualBands]{};};
class LiveVisualAnalysisSlot {
 static_assert(__atomic_always_lock_free(sizeof(unsigned),nullptr),"visual analysis needs lock-free ownership");
 unsigned state_=0;VisualPcmFrame input_{};VisualAnalysisFrame output_{};
 // Worker-owned FFT scratch; never placed on a caller/audio stack.
 float real_[kVisualPcm]{},imag_[kVisualPcm]{};
 void analyze(unsigned tap){
  for(unsigned i=0;i<kVisualScope;++i){float lo=1,hi=-1;for(unsigned j=0;j<4;++j){float v=input_.pcm[tap][i*4+j];if(!isfinite(v))v=0;if(v<lo)lo=v;if(v>hi)hi=v;}output_.scope_low[tap][i]=lo;output_.scope_high[tap][i]=hi;}
  for(unsigned i=0;i<kVisualPcm;++i){float v=input_.pcm[tap][i];real_[i]=(isfinite(v)?v:0)*(.5f-.5f*cosf(6.283185307f*i/(kVisualPcm-1)));imag_[i]=0;}
  for(unsigned i=1,j=0;i<kVisualPcm;++i){unsigned bit=kVisualPcm>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){float v=real_[i];real_[i]=real_[j];real_[j]=v;}}
  for(unsigned length=2;length<=kVisualPcm;length<<=1){float a=-6.283185307f/length,wr=cosf(a),wi=sinf(a);for(unsigned start=0;start<kVisualPcm;start+=length){float r=1,im=0;for(unsigned j=0;j<length/2;++j){unsigned lo=start+j,hi=lo+length/2;float tr=r*real_[hi]-im*imag_[hi],ti=r*imag_[hi]+im*real_[hi];real_[hi]=real_[lo]-tr;imag_[hi]=imag_[lo]-ti;real_[lo]+=tr;imag_[lo]+=ti;float nr=r*wr-im*wi;im=r*wi+im*wr;r=nr;}}}
  for(unsigned b=0;b<kVisualBands;++b){unsigned first=unsigned(powf(256.f,float(b)/kVisualBands)),end=unsigned(powf(256.f,float(b+1)/kVisualBands));if(end<=first)end=first+1;if(end>256)end=256;float peak=0;for(unsigned i=first;i<end;++i){float mag=sqrtf(real_[i]*real_[i]+imag_[i]*imag_[i])*4/kVisualPcm;if(mag>peak)peak=mag;}output_.spectrum[tap][b]=peak;}
 }
public:
 bool submit(const VisualPcmFrame& frame){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0)return false;input_=frame;__atomic_store_n(&state_,1u,__ATOMIC_RELEASE);
#if defined(GROOVE_MULTICORE_DISPLAY) && defined(__aarch64__)
 asm volatile("dsb ishst\n\tsev" ::: "memory");
#endif
 return true;}
 bool process_one(){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=1)return false;output_.sample=input_.sample;output_.generation=input_.generation;output_.rate=input_.rate;output_.track=input_.track;output_.source=input_.source;for(unsigned tap=0;tap<3;++tap)analyze(tap);__atomic_store_n(&state_,2u,__ATOMIC_RELEASE);return true;}
 bool collect(VisualAnalysisFrame& out){if(__atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=2)return false;out=output_;__atomic_store_n(&state_,0u,__ATOMIC_RELEASE);return true;}
 bool busy()const{return __atomic_load_n(&state_,__ATOMIC_ACQUIRE)!=0;}
};
}
