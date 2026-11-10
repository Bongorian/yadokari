#include "kit_voice_mix.h"
#include <cassert>
#include <cmath>
int main(){
 groove::KitVoiceMix mixer;mixer.reset(1,1,0,0,0);
 const float alpha=1-std::exp(-1.0f/(48000*0.005f));
 float l=1,r=1,dl=0,dr=0,rl=0,rr=0;
 mixer.process(l,r,1,0,1,1,0.5f,alpha,dl,dr,rl,rr);
 assert(l>0.99f && r>0.99f && dl>0 && dl<0.01f);
 for(unsigned n=0;n<5000;++n){l=r=1;dl=dr=rl=rr=0;mixer.process(l,r,1,0,1,1,0.5f,alpha,dl,dr,rl,rr);}
 assert(l<0.001f && r<0.001f && mixer.pan>0.999f && mixer.delay>0.999f);
 // Reusing a voice starts at its new hit's base or locked values immediately.
 mixer.reset(1,0.5f,-1,0.25f,0.75f);l=r=1;dl=dr=rl=rr=0;
 mixer.process(l,r,1,0.5f,-1,0.25f,0.75f,alpha,dl,dr,rl,rr);
 assert(l==0.5f && r==0 && dl==0.125f && rl==0.375f && dr==0 && rr==0);
 // A fully-open filter tracks its input, avoiding stale state when closed.
 mixer.reset(1,1,0,0,0);l=r=0.8f;dl=dr=rl=rr=0;
 mixer.process(l,r,1,1,0,0,0,alpha,dl,dr,rl,rr);l=r=0.8f;
 mixer.process(l,r,0,1,0,0,0,alpha,dl,dr,rl,rr);assert(l>0.79f && r>0.79f);
}
