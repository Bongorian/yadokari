#pragma once
#include <math.h>
namespace groove {
// Fourfold internal rate; polyBLEP edges for saw/pulse and integrated triangle.
// Only the explicit sync/PM path uses this adapter; the default stays Daisy.
class VaModOscillator {
 float phase_[2]{},triangle_[2]{},previous_pm_=0,edge_=0;
 static float wrap(float p){return p-floorf(p);}
 static float blep(float p,float dt){if(dt<=0)return 0;if(p<dt){float t=p/dt;return t+t-t*t-1;}if(p>1-dt){float t=(p-1)/dt;return t*t+t+t+1;}return 0;}
 static float raw(float p,unsigned wave,float width){return wave==0?1-2*p:wave==1?(p<width?1.f:-1.f):wave==2?1-4*fabsf(p-.5f):sinf(p*6.2831853f);}
 float sample(unsigned id,float p,float dt,unsigned wave,float width){if(wave==0)return raw(p,wave,width)+blep(p,dt);if(wave==1 || wave==2){float square=raw(p,1,width)+blep(p,dt)-blep(wrap(p-width),dt);if(wave==1)return square;triangle_[id]=dt*4*square+(1-dt*4)*triangle_[id];return triangle_[id]*2;}return raw(p,wave,width);}
public:
 void reset(){phase_[0]=phase_[1]=triangle_[0]=triangle_[1]=previous_pm_=edge_=0;}
 void process(float hz1,float hz2,float rate,unsigned wave1,unsigned wave2,float width,bool sync,float pm,float& a,float& b){a=b=0;float d1=fminf(.1125f,hz1/(rate*4)),d2=fminf(.1125f,hz2/(rate*4));for(unsigned n=0;n<4;++n){float first=sample(0,phase_[0],d1,wave1,width);float modulation=first*pm*.25f;float delta=modulation-previous_pm_;previous_pm_=modulation;float p=wrap(phase_[1]+modulation);float second=sample(1,p,fminf(.45f,d2+fabsf(delta)),wave2,1-width)+edge_;edge_=0;a+=first;b+=second;phase_[0]+=d1;phase_[1]=wrap(phase_[1]+d2);if(phase_[0]>=1){phase_[0]-=1;if(sync){float fraction=d1>0?phase_[0]/d1:0;float before=raw(wrap(phase_[1]+modulation),wave2,1-width);phase_[1]=fraction*d2;float after=raw(wrap(phase_[1]+modulation),wave2,1-width);edge_=-.5f*(after-before)*(1-fraction)*(1-fraction);}}}a*=.25f;b*=.25f;}
};
}
