#pragma once
#include <math.h>
#include "Utility/dsp.h"
namespace groove {
// A mono cabinet split into horn/drum bands, rendered into fixed stereo mics.
// Both variable-delay paths use the slot's existing 2048-frame stereo storage.
class Rotary {
 float rate_=48000,low_=0,split_=0,phase_[2]{},hz_[2]{},up_[2]{},down_[2]{};unsigned write_=0,age_=0;
 float read(const float* buffer,unsigned band,float distance)const{unsigned n=unsigned(distance);float f=distance-n;float a=age_>=n?buffer[((write_-n)&2047)*2+band]:0,b=age_>n?buffer[((write_-n-1)&2047)*2+band]:0;return a+(b-a)*f;}
public:
 void init(unsigned rate){rate_=float(rate?rate:48000);split_=1-expf(-6.2831853f*800/rate_);for(unsigned b=0;b<2;++b){up_[b]=1-expf(-1/(rate_*(b?1.8f:.8f)));down_[b]=1-expf(-1/(rate_*(b?2.5f:1.2f)));}reset();}
 void reset(){low_=0;phase_[0]=0;phase_[1]=.2f;hz_[0]=hz_[1]=0;write_=age_=0;}
 float speed(unsigned band)const{return hz_[band<2?band:0];}
 void process(float* buffer,float& l,float& r,unsigned speed,float drive){if(!buffer)return;float input=daisysp::SoftClip((l+r)*.5f*(1+5*drive))/(1+2*drive);low_+=(input-low_)*split_;buffer[write_*2]=input-low_;buffer[write_*2+1]=low_;
  float output[2]{};for(unsigned band=0;band<2;++band){float target=speed==2?0:speed==1?(band?5.4f:6.8f):(band?.65f:.8f);hz_[band]+=(target-hz_[band])*(target>hz_[band]?up_[band]:down_[band]);phase_[band]+=hz_[band]/rate_;if(phase_[band]>=1)phase_[band]-=1;for(unsigned ch=0;ch<2;++ch){float angle=(phase_[band]+(ch?.25f:-.25f))*6.2831853f;float rotation=sinf(angle),amplitude=.65f+.35f*cosf(angle);float distance=rate_*((band?.007f:.004f)+(band?.002f:.001f)*rotation);distance=fmaxf(1,fminf(2046,distance));output[ch]+=read(buffer,band,distance)*amplitude;}}
  write_=(write_+1)&2047;if(age_<2048)++age_;l=output[0];r=output[1];
 }
};
}
