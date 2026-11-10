#include "texture_voice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <chrono>
using groove::TextureStereo;
// Pre-optimization boundary converter, kept independent for exact regression.
// Build with production -O2: AArch64 contracts scalar MACs just as Circle does.
template<unsigned L,unsigned M> struct Original {
 TextureStereo history[128]{};unsigned head=0;
 void block(const TextureStereo* input,unsigned count,TextureStereo* output){unsigned out=0;for(unsigned i=0;i<count;++i){head=(head+1)&127;history[head]=input[i];while(out*M/L==i){unsigned phase=out*M%L;TextureStereo v{};for(unsigned k=0;phase+k*L<193;++k){float c=groove::kTextureRateFilter[phase+k*L]*L;const auto& s=history[(head-k)&127];v.l+=s.l*c;v.r+=s.r*c;}output[out++]=v;}}}
};
template<unsigned L,unsigned M,unsigned Count> void check(bool benchmark){
 Original<L,M> original;groove::TextureRateConverter<L,M> optimized;TextureStereo input[Count],a[Count*L/M],b[Count*L/M];unsigned rng=7;float error=0;
 for(unsigned block=0;block<4096;++block){for(auto& s:input){rng=1664525*rng+1013904223;s.l=float(int(rng>>8)-8388608)/8388608;rng=1664525*rng+1013904223;s.r=float(int(rng>>8)-8388608)/8388608;}original.block(input,Count,a);optimized.block(input,Count,b);for(unsigned i=0;i<Count*L/M;++i)error=fmaxf(error,fmaxf(fabsf(a[i].l-b[i].l),fabsf(a[i].r-b[i].r)));}
 std::printf("FIR %u:%u max error %.9g\n",L,M,error);assert(error==0);
 optimized.reset();Original<L,M> reset;reset.block(input,Count,a);optimized.block(input,Count,b);for(unsigned i=0;i<Count*L/M;++i)assert(a[i].l==b[i].l && a[i].r==b[i].r);
 if(!benchmark)return;
 volatile float sink=0;auto bench=[&](auto& converter){auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<32768;++i){converter.block(input,Count,a);sink=a[i%(Count*L/M)].l;}return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();};
 double before=bench(original),after=bench(optimized);std::printf("FIR %u:%u original %.3fs optimized %.3fs ratio %.3f sink %.3f\n",L,M,before,after,after/before,float(sink));

}
int main(int argc,char**){check<2,3,48>(argc>1);check<3,2,32>(argc>1);}
