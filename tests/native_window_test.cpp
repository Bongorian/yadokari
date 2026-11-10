#include "native_window.h"
#include <cassert>
#include <initializer_list>
#include <cstring>
int main(){int16_t samples[4096];float window[16384],output[4096],reference[4096];uint32_t rng=77;for(auto& s:samples){rng=rng*1664525+1013904223;s=int16_t(rng);}for(float& w:window){rng=rng*1664525+1013904223;w=float(int32_t(rng))/2147483648.f;}for(unsigned stride:{1u,2u,4u})for(unsigned count:{1u,3u,4u,31u,1024u,4095u,4096u}){for(unsigned i=0;i<count;++i)reference[i]=window[i*stride]*samples[i];groove::native_analysis_window(output,samples,window,stride,count);assert(!std::memcmp(reference,output,count*sizeof(float)));}}
