// Maximum native controls and subsequent reset must remain memory-safe.
#include "texture_dsp.h"
#include <cmath>
#include <cstdio>
int main(){for(unsigned mode=0;mode<4;++mode)for(unsigned quality=0;quality<4;++quality){groove::TextureDsp dsp;dsp.init(7);float p[15]={float(mode),.5f,1,0,1,.5f,1,1,1,1,0,float(quality),52,0,0};groove::TextureFrame in[32],out[32];for(unsigned frame=0;frame<128000;frame+=32){for(unsigned i=0;i<32;++i)in[i]={int16_t(900*sinf(float(frame+i)*.086394f)),int16_t(900*sinf(float(frame+i)*.086394f))};dsp.process(p,in,out,frame%1600==0);}dsp.init(7);std::printf("mode %u quality %u reset OK\n",mode,quality);}}
