#pragma once
#include <math.h>
namespace groove {
// Original 16-band analysis/synthesis filterbank. No allocation or external DSP.
class Vocoder {
 struct Filter {float z1=0,z2=0;float process(float x,float b,float a1,float a2){float y=b*x+z1;z1=-a1*y+z2;z2=-b*x-a2*y;return y;}};
 struct Band {float b=0,a1=0,a2=0,envelope=0;Filter analysis,carrier[2];};
 Band bands_[16]{};float attack_=0,release_=0;
public:
 void levels(float* out)const{for(unsigned i=0;i<16;++i)out[i]=bands_[i].envelope;}
 void init(unsigned rate){float sr=float(rate?rate:48000);attack_=1-expf(-1/(sr*.005f));release_=1-expf(-1/(sr*.080f));for(unsigned i=0;i<16;++i){auto& b=bands_[i];float hz=80*powf(100.f,float(i)/15);if(hz>sr*.4f)hz=sr*.4f;float w=6.2831853f*hz/sr,alpha=sinf(w)/(2*3.2f),inv=1/(1+alpha);b.b=alpha*inv;b.a1=-2*cosf(w)*inv;b.a2=(1-alpha)*inv;}reset();}
 void reset(){for(auto& b:bands_){b.envelope=0;b.analysis={};b.carrier[0]={};b.carrier[1]={};}}
 void process(float& l,float& r,float mod){float a=0,c=0;for(auto& b:bands_){float m=fabsf(b.analysis.process(mod,b.b,b.a1,b.a2));b.envelope+=(m-b.envelope)*(m>b.envelope?attack_:release_);float gain=fminf(4,b.envelope*12);a+=b.carrier[0].process(l,b.b,b.a1,b.a2)*gain;c+=b.carrier[1].process(r,b.b,b.a1,b.a2)*gain;}l=a*.8f;r=c*.8f;}
};
}
