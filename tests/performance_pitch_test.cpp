#include "live_effects.h"
#include <cassert>
#include <cmath>
#include <cstdio>
static unsigned run(groove::PerformanceFx mode){
 groove::MasterCapture history(48000);groove::PerformanceFxBus fx(48000);fx.set(mode,1);
 float previous=0;unsigned crossings=0;
 for(unsigned n=0;n<144000;++n){float sample=.3f*sinf(6.283185307f*440*n/48000);history.push(sample,sample);float l=sample,r=sample;fx.process(l,r,history,120);assert(std::isfinite(l) && std::isfinite(r));assert(fabsf(l-r)<0.00001f);if(n>=96000 && l>=0 && previous<0)++crossings;previous=l;}
 fx.set(groove::PerformanceFx::Off,0);float l=0,r=0;for(unsigned n=0;n<4096;++n){l=.2f;r=-.1f;fx.process(l,r,history,120);}assert(fabsf(l-.2f)<.0001f && fabsf(r+.1f)<.0001f);
 return crossings;
}
int main(){unsigned down=run(groove::PerformanceFx::PitchDive),up=run(groove::PerformanceFx::PitchRise);assert(down>190 && down<270);assert(up>800 && up<930);std::printf("pitch dive %u Hz, rise %u Hz; finite stereo and dry release passed\n",down,up);}
