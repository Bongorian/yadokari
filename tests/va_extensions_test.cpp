#include "engine.h"
#include "project_codec.h"
#include "future_factory_presets.h"
#include "fixtures/va_dual_v1_reference.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();
 for(unsigned preset=4;preset<8;++preset){assert(make_future_factory_preset(*p,preset));const auto& state=p->tracks[0].virtual_analog;ReferenceVirtualAnalogVoice before;VirtualAnalogVoice after;before.init(48000);after.init(48000);before.trigger(.8f,state.values,123);after.trigger(.8f,state.values,123);for(unsigned n=0;n<48000;++n){float old=before.next(220,n<24000),now=after.next(220,n<24000);assert(old==now);}for(unsigned n=18;n<26;++n)assert(state.values[n]==VirtualAnalogState{}.values[n]);}
 float reference[4096]{};VirtualAnalogState state;state.values[16]=7;state.values[4]=16000;state.values[7]=0;state.values[13]=1;VirtualAnalogVoice voice;voice.init(48000);voice.trigger(1,state.values,1);for(float& x:reference)x=voice.next(220,true);
 const unsigned fields[]={18,19,20,22,23,24,25};for(unsigned field:fields){auto modified=state;modified.values[field]=field==20?24:field==24?2:1;if(field==24)modified.values[23]=1;if(field==25)modified.values[23]=2;voice.trigger(1,modified.values,1);double energy=0,difference=0;for(float expected:reference){float x=voice.next(220,true);assert(__builtin_isfinite(x)&&fabsf(x)<4);energy+=x*x;difference+=fabsf(expected-x);}assert(energy>.001&&difference>.01);}
 for(unsigned kind=0;kind<3;++kind){auto a=state;a.values[23]=float(kind);a.values[17]=.5f;voice.trigger(1,a.values,1);for(float& x:reference)x=voice.next(440,true);a.values[17]=3;voice.trigger(1,a.values,1);double diff=0;for(float x:reference)diff+=fabsf(x-voice.next(440,true));assert(diff>.01);}
 assert(make_future_factory_preset(*p,4));for(unsigned i=18;i<26;++i)p->tracks[0].virtual_analog.values[i]=kVirtualAnalogParameters[i].maximum;ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));assert(bytes.data[4]==kFormatVersion);ChunkSource source{bytes.data,bytes.size,0};assert(decode_project(source,*out));for(unsigned i=0;i<26;++i)assert(p->tracks[0].virtual_analog.values[i]==out->tracks[0].virtual_analog.values[i]);bytes.data[4]=uint8_t(kFormatVersion-1);ChunkSource old{bytes.data,bytes.size,0};assert(!decode_project(old,*out));assert(instrument_voice_limit(Instrument::VirtualAnalog)==8);for(unsigned i=18;i<26;++i)assert(engine_parameter(Instrument::VirtualAnalog,i)->ui.tier==EngineEditTier::Detail);
 puts("VA: factory4 bit-exact, sync/PM/pitch/sub/filter differences, all-filter drive, VATH2/current MGXC roundtrip and old rejection, 8-voice detail contract passed");}
