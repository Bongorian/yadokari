#include "wav_sampler.h"
#include <cassert>
#include <cmath>
using namespace groove;
int main(){
 int16_t pcm[]={0,8192,16384,24576};
 WavView sample{reinterpret_cast<const uint8_t*>(pcm),4,48000,1,16,1};
 SampleEdit edit;edit.mode=SampleMode::PingPong;edit.fade_ms=0;edit.crossfade_ms=0;edit.release_ms=0;
 WavVoice voice;assert(voice.start_edit(sample,48000,1,false,false,edit));
 const float expected[]={0,0.25f,0.5f,0.75f,0.5f,0.25f,0,0.25f,0.5f};
 for(float x:expected){float l,r;voice.next(l,r);assert(std::fabs(l-x)<0.0001f && l==r && voice.active());}
 voice.release();float l,r;voice.next(l,r);assert(!voice.active());
 assert(voice.start_edit(sample,48000,1,false,true,edit));
 const float reverse[]={0.75f,0.5f,0.25f,0,0.25f};
 for(float x:reverse){voice.next(l,r);assert(std::fabs(l-x)<0.0001f);}
 edit.start=0.5f;edit.end=0.75f;assert(voice.start_edit(sample,48000,8,false,false,edit));
 for(unsigned i=0;i<32;++i){voice.next(l,r);assert(std::isfinite(l) && l==r && voice.active());}
}
