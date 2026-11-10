// Note adapter around unmodified msfa. Scaling equations follow the Apache-2.0
// Dx7Note by Google Inc. (2012) and Pascal Gauthier (2016-2017); see msfa/LICENSE.
#include "fm6_voice.h"
#include <math.h>
#include <string.h>
#if defined(__linux__)
#include <new>
#else
#include <circle/new.h>
#endif
#define __CONTROLLERS_H
#include "../third_party/msfa/fm_core.h"
#include "../third_party/msfa/env.h"
#include "../third_party/msfa/pitchenv.h"
#include "../third_party/msfa/lfo.h"
#include "../third_party/msfa/sin.h"
#include "../third_party/msfa/exp2.h"
#include "../third_party/msfa/freqlut.h"
#include "plaits/resources.h"
namespace groove {
namespace {
constexpr float q24=16777216;
struct Fm6Dsp {
 FmCore core;Env env[6];PitchEnv pitch{};Lfo lfo{};FmOpParams op[6]{};
 int32_t feedback[2]{},buffer[64]{};unsigned cursor=64,rate=48000,velocity=100,algorithm=0;float position=0,last=0,hz=261.626f;bool playing=false,gate=true;
 uint8_t raw[128]{};float values[21]{},ratio[6]{},fixed[6]{};int outlevel[6]{},rate_scale[6]{};
};
static_assert(sizeof(Fm6Dsp)<=2048,"FM6 voice storage");
int clamp99(int x){return x<0?0:x>99?99:x;}
const uint8_t velocity_curve[64]={0,70,86,97,106,114,121,126,132,138,142,148,152,156,160,163,166,170,173,174,178,181,184,186,189,190,194,196,198,200,202,205,206,209,211,214,216,218,220,222,224,225,227,229,230,232,233,235,237,238,240,241,242,243,244,246,246,248,249,250,251,252,253,254};
const uint8_t exp_curve[33]={0,1,2,3,4,5,6,7,8,9,11,14,16,19,23,27,33,39,47,56,66,80,94,110,126,142,158,174,190,206,222,238,250};
int scaling(int note,const uint8_t* p){int offset=note-int(p[8])-17;int group=(offset>=0?offset+1:-(offset-1))/3;unsigned curve=offset>=0?(p[11]>>2)&3:p[11]&3;int depth=p[offset>=0?10:9];int v=(curve==0 || curve==3)?(group*depth*329)>>12:(exp_curve[group>32?32:group]*depth*329)>>15;return curve<2?-v:v;}
void envelope(Fm6Dsp& d,unsigned i,bool start){const uint8_t* p=d.raw+17*i;int rates[4],levels[4];for(unsigned k=0;k<4;++k){float scale=k==0?d.values[2]:k==3?d.values[3]:1;rates[k]=clamp99(int(p[k])-int(8*log2f(scale)));levels[k]=clamp99(p[4+k]);}if(start)d.env[i].init(rates,levels,d.outlevel[i],d.rate_scale[i]);else d.env[i].update(rates,levels,d.outlevel[i],d.rate_scale[i]);}
void block(Fm6Dsp& d,float hz,bool gate){
 if(!gate && d.gate){for(auto& e:d.env)e.keydown(false);d.pitch.keydown(false);}d.gate=gate;
 int algo=d.values[17]<0?(d.raw[110]&31):int(d.values[17]);int fb=d.values[18]<0?(d.raw[111]&7):int(d.values[18]);
 int32_t lfo=d.lfo.getsample(),delay=d.lfo.getdelay();const int sensitivity[]={0,10,20,33,55,92,153,255};float modulation=float(lfo)/q24-.5f;
 float pitch=float(d.pitch.getsample())/q24+modulation*(float(delay)/q24)*(d.raw[114]*165.0f/64)*sensitivity[(d.raw[116]>>4)&7]/32768*d.values[4];
 float multiplier=exp2f(pitch+(d.values[19]+float(int(d.raw[117])-24))/12);
 bool alive=false;
 for(unsigned i=0;i<6;++i){auto* p=d.raw+17*i;float f=(p[15]&1)?d.fixed[i]:hz*d.ratio[i]*multiplier;f*=exp2f(d.values[11+i]/1200);if(f<.01f)f=.01f;if(f>20000)f=20000;
  d.op[i].freq=int32_t(f*q24/48000);int32_t level=d.env[i].getsample();float gain=d.values[5+i]*(FmCore::isCarrier(algo,i)?1:d.values[1]);
  // DX7 amplitude sensitivity is applied in logarithmic envelope units.
  const float sensitivity_amp[]={0,.2588235f,.427451f,1};float am=(1-float(lfo)/q24)*(float(delay)/q24)*float(d.raw[115])/99*sensitivity_amp[p[13]&3]*d.values[4];
  gain*=exp2f(-am*4);d.op[i].level_in=gain>.00001f?level+int32_t(log2f(gain)*q24):0;alive|=d.env[i].isActive();
 }
 memset(d.buffer,0,sizeof d.buffer);d.core.render(d.buffer,d.op,algo,d.feedback,fb?8-fb:16);d.cursor=0;d.playing=alive || gate;
}
}
Fm6Voice::Fm6Voice(){new(storage_) Fm6Dsp();}
Fm6Voice::~Fm6Voice(){reinterpret_cast<Fm6Dsp*>(storage_)->~Fm6Dsp();}
void Fm6Voice::init(unsigned rate){auto& d=*reinterpret_cast<Fm6Dsp*>(storage_);d.rate=rate;static bool initialized=false;if(!initialized){Sin::init();Exp2::init();Freqlut::init(48000);Env::init_sr(48000);PitchEnv::init(48000);Lfo::init(48000);initialized=true;}}
void Fm6Voice::trigger(float hz,unsigned velocity,const Fm6State& s){auto& d=*reinterpret_cast<Fm6Dsp*>(storage_);d.hz=hz;d.velocity=velocity>127?127:velocity;d.gate=d.playing=true;d.cursor=64;d.position=1;d.last=0;memset(d.op,0,sizeof d.op);memset(d.feedback,0,sizeof d.feedback);for(unsigned i=0;i<21;++i)d.values[i]=s.values[i];int patch=int(s.values[0]);memcpy(d.raw,patch<0?s.patch:plaits::fm_patches_table[unsigned(patch)/32]+128*(unsigned(patch)%32),128);
 int note=int(69+12*log2f(hz/440)+.5f);if(note<0)note=0;if(note>127)note=127;
 for(unsigned i=0;i<6;++i){auto* p=d.raw+17*i;int level=Env::scaleoutlevel(clamp99(p[14]))+scaling(note,p);if(level>127)level=127;int vel=(int(p[13]>>2)&7)*(int(velocity_curve[d.velocity>>1])-239);d.outlevel[i]=int(level*32+(((vel+7)>>3)*16)*d.values[20]);if(d.outlevel[i]<0)d.outlevel[i]=0;int key=note/3-7;if(key<0)key=0;if(key>31)key=31;d.rate_scale[i]=((p[12]&7)*key)>>3;
  float coarse=float((p[15]>>1)&31);d.ratio[i]=(coarse?coarse:.5f)*(1+.01f*clamp99(p[16]))*exp2f((int((p[12]>>3)&15)-7)*1.5f/1200);d.fixed[i]=powf(10,float(((p[15]>>1)&3)*100+clamp99(p[16]))*.01f);envelope(d,i,true);
 }
 int rates[4],levels[4];for(unsigned i=0;i<4;++i){rates[i]=clamp99(d.raw[102+i]);levels[i]=clamp99(d.raw[106+i]);}d.pitch.set(rates,levels);uint8_t lfo[]={uint8_t(clamp99(d.raw[112])),uint8_t(clamp99(d.raw[113])),d.raw[114],d.raw[115],uint8_t(d.raw[116]&1),uint8_t(((d.raw[116]>>1)&7)>5?5:(d.raw[116]>>1)&7)};d.lfo.reset(lfo);d.lfo.keydown();
}
void Fm6Voice::parameters(const float* values){auto& d=*reinterpret_cast<Fm6Dsp*>(storage_);for(unsigned i=0;i<21;++i)d.values[i]=values[i];}
float Fm6Voice::next(float hz,bool gate){auto& d=*reinterpret_cast<Fm6Dsp*>(storage_);if(!d.playing)return 0;d.position+=48000.0f/d.rate;while(d.position>=1){if(d.cursor==64)block(d,hz,gate);d.last=float(d.buffer[d.cursor++])/q24*.12f;d.position-=1;}return d.last;}
bool Fm6Voice::active()const{return reinterpret_cast<const Fm6Dsp*>(storage_)->playing;}
}
