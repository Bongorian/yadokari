#include "parameter_resolver.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
struct Sink{std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
struct Source{const std::vector<uint8_t>& bytes;size_t pos=0;bool read(uint8_t* p,size_t n){if(n>bytes.size()-pos)return false;for(size_t i=0;i<n;++i)p[i]=bytes[pos++];return true;}bool at_end()const{return pos==bytes.size();}};
int main(){using namespace groove;auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);for(unsigned i=0;i<88;++i){uint16_t id=uint16_t(384+i);assert(parameter_at(248+i)==id && parameter_index(id)==248+i && parameter_descriptor(id));}
 assert(group_fx_id(1,5)==421 && group_fx_group(421)==1 && group_fx_field(421)==5);assert(group_send_id(0,1)==449 && group_parallel_id(0)==464);const uint16_t id=464;assert(set_parameter_base(*p,t,id,.2f));p->morph_enabled=true;p->morph_a[0].values[morph_index(id)]=.2f;p->morph_b[0].values[morph_index(id)]=.8f;p->morph_position=.5f;auto result=resolve_parameter(*p,t,0,id,nullptr,0);assert(fabsf(result.value-.5f)<.0001f && (result.sources&FromMorph));
 auto& macro=p->macros[0];macro.active=true;macro.value=1;macro.routes[0]={true,id,1,0,.7f,LayerMode::Replace};result=resolve_parameter(*p,t,0,id,nullptr,0);assert(fabsf(result.value-.7f)<.0001f && (result.sources&FromMacro));ParameterLock locks[kLocks]{};locks[0]={id,.3f,true};result=resolve_parameter(*p,t,0,id,locks,0);assert(fabsf(result.value-.3f)<.0001f && (result.sources&FromLock));result=resolve_parameter(*p,t,0,id,locks,0,true,id,.2f);assert(fabsf(result.value-.5f)<.0001f && (result.sources&FromControlAll));
 Sink sink;assert(encode_project(sink,*p));auto q=std::make_unique<ProjectState>();Source src{sink.bytes};assert(decode_project(src,*q));assert(fabsf(q->routing.groups[0].parallel-.2f)<.0001f);assert(fabsf(q->morph_b[0].values[morph_index(id)]-.8f)<.0001f);puts("group registry, scene/macro/lock precedence and project roundtrip passed");}
