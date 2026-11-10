#include "track_insert.h"
#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace groove;
double run(unsigned mod,unsigned carrier,bool input_left,bool input_right,bool slot_signal){TrackInsertSlot fx;fx.init(48000);double energy=0;for(unsigned n=0;n<48000;++n){float wave=.3f*sinf(float(n)*.063f),saw=.2f*(float(n%120)/60-1),l=slot_signal?saw:0,r=l;fx.process(l,r,19,float(mod)/2,float(carrier)/3,1,input_left?wave:0,input_right?wave:0);assert(__builtin_isfinite(l) && __builtin_isfinite(r));if(n>24000)energy+=l*l+r*r;}return energy;}
int main(){assert(fx_descriptor(19) && fx_descriptor(19)->bands==16);double internal=run(0,0,true,false,true),external=run(0,1,true,false,false);assert(internal>.01 && external>.01);assert(run(1,0,true,false,true)<1e-8);assert(run(0,0,false,false,true)<1e-8);assert(run(2,3,true,true,false)>.01);auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].insert_fx[0]=19;p->tracks[0].insert_fx[1]=1;p->tracks[0].insert_fx[2]=2.f/3;p->routing.groups[0].insert_fx[0]=19;assert(valid_project(*p));ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource src{bytes.data,bytes.size,0};auto out=std::make_unique<ProjectState>();assert(decode_project(src,*out));assert(out->tracks[0].insert_fx[0]==19 && out->routing.groups[0].insert_fx[0]==19);printf("Vocoder: internal/external carrier, L/R/stereo modulator, silence and codec passed %.6f %.6f\n",internal,external);}
