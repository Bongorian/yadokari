#include "parameter_resolver.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
using namespace groove;
struct Buffer {std::vector<uint8_t> data;size_t pos=0;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(pos+n>data.size())return false;for(size_t i=0;i<n;++i)p[i]=data[pos++];return true;}bool at_end()const{return pos==data.size();}};
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::DrumKit;
 const uint16_t id=uint16_t(320+5*4+1);const unsigned i=parameter_index(id);
 assert(parameter_at(i)==id && i<kMorphParameters);
 assert(set_track_parameter(t,id,0.2f));assert(std::abs(parameter_base(*p,t,id)-0.2f)<0.0001f);
 assert(std::abs(clamp_parameter(id,0.35f)-0.35f)<0.0001f);
 p->morph_a[0].values[i]=0.2f;p->morph_b[0].values[i]=0.8f;p->morph_enabled=true;p->morph_position=0.5f;
 auto value=resolve_parameter(*p,t,0,id,nullptr,0);assert(std::abs(value.value-0.5f)<0.0001f && (value.sources&FromMorph));
 auto& macro=p->macros[0];macro.active=true;macro.value=0.25f;macro.routes[0]={true,id,1,0,1,LayerMode::Replace};
 value=resolve_parameter(*p,t,0,id,nullptr,0);assert(std::abs(value.value-0.25f)<0.0001f && (value.sources&FromMacro));
 ParameterLock locks[kLocks]{};locks[0]={id,0.7f,true};
 value=resolve_parameter(*p,t,0,id,locks,0);assert(std::abs(value.value-0.7f)<0.0001f && (value.sources&FromLock));
 ParameterLock pad_locks[kLocks]{};pad_locks[0]={id,0.9f,true};
 value=resolve_parameter(*p,t,0,id,locks,0,false,0,0,nullptr,true,nullptr,0,nullptr,pad_locks);assert(std::abs(value.value-0.9f)<0.0001f);
 value=resolve_parameter(*p,t,0,id,nullptr,0);assert(std::abs(value.value-0.25f)<0.0001f);
 t.kit.steps[5][7].fx_lock[1]=100;t.kit.steps[6][7].fx_lock[1]=42;
 Buffer bytes;assert(encode_project(bytes,*p));auto restored=std::make_unique<ProjectState>();assert(decode_project(bytes,*restored));
 assert(std::abs(restored->morph_b[0].values[i]-0.8f)<0.0001f && std::abs(track_at(*restored,0,0).kit.pads[5].insert_fx[1]-0.2f)<0.0001f);
 assert(track_at(*restored,0,0).kit.steps[5][7].fx_lock[1]==100 && track_at(*restored,0,0).kit.steps[6][7].fx_lock[1]==42);
}
