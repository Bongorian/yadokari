#include "track_insert.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <initializer_list>

int main(){
 // Compare bits, including +/-0, subnormals, and both sides of each limit.
 for(float limit:{2.0f,4.0f})for(unsigned i=0;i<100000;++i){uint32_t bits=i*2654435761u;float x;std::memcpy(&x,&bits,4);if(!std::isfinite(x))continue;float expected=std::fmax(-limit,std::fmin(limit,x)),actual=groove::finite_insert_clip(x,limit);assert(std::memcmp(&expected,&actual,4)==0);}
 for(float x:{-0.0f,0.0f,-4.0f,4.0f,-2.0f,2.0f}){float expected=std::fmax(-2.0f,std::fmin(2.0f,x)),actual=groove::finite_insert_clip(x,2);assert(std::memcmp(&expected,&actual,4)==0);}

 for(unsigned i=0;i<=10000;++i){float x=float(i)/10000;assert(std::abs(groove::modulation_decay(x)-std::exp(-x))<0.000005f);}assert(groove::modulation_decay(2)==std::exp(-2.0f));
#ifdef GROOVE_PLAITS
 for(unsigned i=0;i<10000;++i){float phase=float(i)/10000;assert(std::abs(plaits::SineNoWrap(phase)-std::sin(phase*6.2831853f))<0.00002f);}
#endif
 for(unsigned mode=1;mode<=4;++mode){daisysp::Svf reference,fast;reference.Init(48000);fast.Init(48000);
  for(unsigned n=0;n<12000;++n){if(n%128==0){float hz=30+float((n*13)%12000),res=float(n%700)/1000;reference.SetFreq(hz);fast.SetFreq(hz);reference.SetRes(res);fast.SetRes(res);reference.SetDrive(0);fast.SetDrive(0);}float x=0.3f*std::sin(float(n)*0.031f);reference.Process(x);float expected=mode==1?reference.Low():mode==2?reference.High():mode==3?reference.Band():reference.Notch();float actual=fast.ProcessMode(x,mode);assert(std::abs(expected-actual)<0.000001f);}}
 daisysp::Overdrive drive;drive.Init();for(unsigned i=0;i<1000;++i){drive.SetDrive(float(i%100)/100);float x=std::sin(float(i)*0.07f);assert(std::abs(drive.Process(x)-drive.ProcessInline(x))<0.000001f);}
 {daisysp::Svf old,changed;old.Init(48000);changed.Init(48000);float last_a=-1,last_b=-1;
  for(unsigned n=0;n<12000;++n){if(n%64==0){float a=float((n/512)%7)/7,b=float((n/1024)%5)/5,hz=20+(48000*0.30f-20)*a*a;old.SetFreq(hz);old.SetRes(b*0.85f);old.SetDrive(0);if(a!=last_a)changed.SetFreq(hz);if(b!=last_b)changed.SetRes(b*0.85f);if(last_a<0)changed.SetDrive(0);last_a=a;last_b=b;}float x=0.3f*std::sin(float(n)*0.025f);old.Process(x);float y=changed.ProcessMode(x,1);assert(std::abs(old.Low()-y)<0.000001f);}}
 {daisysp::Svf control,left,right;control.Init(48000);left.Init(48000);right.Init(48000);for(unsigned n=0;n<4000;++n){if(n%64==0){float hz=200+float(n),res=float(n%256)/512;control.SetFreq(hz);control.SetRes(res);control.SetDrive(0);left.SetFreq(hz);left.SetRes(res);left.SetDrive(0);right.CopyCoefficientsFrom(left);}float x=0.3f*std::cos(float(n)*0.03f);control.Process(x);left.ProcessMode(-x,2);float output=right.ProcessMode(x,2);assert(std::abs(control.High()-output)<0.000001f);}}
 float memory[groove::TrackInsertSlot::kModFloats];for(auto& x:memory)x=123;
 groove::TrackInsertSlot slot;slot.bind_modulation(memory);slot.init(48000);
 for(unsigned mode=16;mode<=17;++mode){slot.reset();for(unsigned i=0;i<2048;++i){float l=0,r=0;slot.process(l,r,mode,0.5f,0.8f,1);assert(l==0 && r==0);}}
 for(unsigned mode=16;mode<=18;++mode){slot.reset();for(unsigned i=0;i<4096;++i){float l=0.3f*std::sin(float(i)*0.03f),r=0;slot.process(l,r,mode,0.5f,0.8f,1);assert(std::isfinite(l) && r==0);}}
 for(unsigned mode=1;mode<=18;++mode){
  slot.reset();float difference=0;
  for(unsigned n=0;n<4096;++n){float l=0.3f*std::sin(n*0.037f),r=0.17f*std::cos(n*0.053f),dry_l=l,dry_r=r;slot.process(l,r,mode,0.55f,0.7f,1);assert(std::isfinite(l) && std::isfinite(r));assert(std::abs(l)<=2.01f && std::abs(r)<=2.01f);difference+=std::abs(l-dry_l)+std::abs(r-dry_r);}
  assert(difference>1);
  float l=0,r=0;for(unsigned n=0;n<2048;++n){l=0.2f;r=-0.1f;slot.process(l,r,0,0.5f,0.5f,0);}assert(std::abs(l-0.2f)<0.001f && std::abs(r+0.1f)<0.001f);
 }
}
