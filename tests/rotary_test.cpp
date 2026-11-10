#include "track_insert.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace groove;
int main(){Rotary rotary;rotary.init(48000);float buffer[4096]{};double stereo=0,energy=0;float before=0,maxstep=0;
 for(unsigned n=0;n<48000*9;++n){float l=.4f*sinf(6.2831853f*220*n/48000)+.1f*sinf(6.2831853f*2200*n/48000),r=l;unsigned speed=n<48000*2?0:n<48000*6?1:2;float previous=rotary.speed(0);rotary.process(buffer,l,r,speed,.3f);assert(__builtin_isfinite(l)&&__builtin_isfinite(r));assert(fabsf(rotary.speed(0)-previous)<.001f);energy+=l*l+r*r;stereo+=fabsf(l-r);if(n>1000)maxstep=fmaxf(maxstep,fabsf(l-before));before=l;if(n==48000*2-1)assert(rotary.speed(0)>.6f&&rotary.speed(0)<.9f);if(n==48000*6-1)assert(rotary.speed(0)>6.5f&&rotary.speed(1)>4.5f);}
 assert(energy>1&&stereo>1&&maxstep<.4f&&rotary.speed(0)<.6f&&rotary.speed(1)>rotary.speed(0));auto slot=std::make_unique<TrackInsertSlot>();slot->bind_modulation(buffer);slot->init(48000);for(unsigned n=0;n<12000;++n){float l=.2f*sinf(n*.1f),r=l;slot->process(l,r,21,n<4000?0:n<8000?.5f:1,.4f,.8f);assert(__builtin_isfinite(l)&&fabsf(l)<2);}assert(fx_descriptor(21)&&fx_descriptor(21)->controls[0].count==3);puts("Rotary: horn/drum stereo, slow/fast/brake ramps, finite transition and slot choice contract passed");}
