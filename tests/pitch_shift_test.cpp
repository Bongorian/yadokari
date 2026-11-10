#include "track_insert.h"
#include "project_codec.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){PitchShiftStereo::Pool pool(2),failed(0);assert(pool.count()==2 && PitchShiftStereo::state_bytes()==2*sizeof(daisysp::PitchShifter));TrackInsertSlot first,second,unavailable;first.bind_pitch(pool,0);second.bind_pitch(pool,1);unavailable.bind_pitch(failed,0);first.init(48000);second.init(48000);unavailable.init(48000);assert(first.ready() && second.ready() && !unavailable.ready());
 for(unsigned n=0;n<4096;++n){float l=.2f*sinf(n*.1f),r=l,x=l,y=r;first.process(l,r,20,.75f,0,1);second.process(x,y,20,.75f,0,1);assert(l==x && r==y);}auto fx=std::make_unique<TrackInsertSlot>();fx->init(48000);const int shifts[]={-12,0,7,12};for(int shift:shifts){fx->reset();double energy=0;float prev=0;unsigned crossings=0;for(unsigned n=0;n<48000;++n){float l=.2f*sinf(6.2831853f*440*n/48000),r=l;fx->process(l,r,20,float(shift+12)/24,0,1);assert(__builtin_isfinite(l)&&fabsf(l)<2);if(n>=24000){energy+=l*l;if(prev<=0&&l>0)++crossings;}prev=l;}float expected=220*powf(2,float(shift)/12);assert(energy>1 && fabsf(float(crossings)-expected)<expected*.07f+3);}
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();auto t=track_at(*p,0,0);t.insert_fx[0]=20;t.insert_fx[1]=19.f/24;t.delay_send=.3f;t.reverb_send=.4f;ChunkBuffer b;assert(encode_project_buffer(b,*p));ChunkSource in{b.data,b.size,0};assert(decode_project(in,*out));auto o=track_at(*out,0,0);assert(o.insert_fx[0]==20 && o.insert_fx[1]==t.insert_fx[1] && o.delay_send==.3f && o.reverb_send==.4f);puts("Pitch shift: -12/0/+7/+12 pitch, finite stereo, slot/send roundtrip passed");}
