#include "project_codec.h"
#include "parameter_resolver.h"
#include <cassert>
#include <memory>
#include <vector>
#include <cstdio>
using namespace groove;
struct Bytes {std::vector<uint8_t> data;size_t pos=0;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(pos+n>data.size())return false;__builtin_memcpy(p,data.data()+pos,n);pos+=n;return true;}bool at_end()const{return pos==data.size();}};
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::FM;
 auto& owners=p->unavailable_targets;assert(owners.intern("future.synth",12)==0);assert(owners.intern("mk3.fm",6)==1);
 const ParameterTarget missing=0xc000|7,extension=0xc000|(1<<5)|31;
 auto t=track_at(*p,0,0);t.steps[0].locks[0]={missing,.7f,true};t.steps[255].locks[1]={extension,17,true};t.mutation.frozen=true;t.mutation_source[200].locks[0]={missing,.3f,true};
 t.automation[0].parameter=missing;t.automation[0].enabled=true;for(float& v:t.automation[0].values)v=.6f;
 p->macros[0].active=true;p->macros[0].routes[0]={true,missing,1,.1f,.9f,LayerMode::Replace};
 p->gesture.count=1;p->gesture.points[0]={0,extension,3,0,0,GestureTarget::Parameter};
 assert(valid_project(*p));assert(resolve_parameter(*p,t,0,32,t.steps[0].locks,0).sources==0);
 assert(resolve_target_value(*p,t,0,missing,.25f,t.steps[0].locks,0,true,missing,1).value==.25f);
 Bytes file;assert(encode_project(file,*p));auto q=std::make_unique<ProjectState>();assert(decode_project(file,*q));assert(q->unavailable_targets.count==2);
 auto qt=track_at(*q,0,0);assert(unavailable_target(qt.steps[0].locks[0].id));assert(!__builtin_strcmp(target_owner_name(qt.steps[0].locks[0].id,q->unavailable_targets),"future.synth"));
 assert(qt.steps[0].locks[0].value==.7f && target_local(qt.steps[255].locks[1].id)==31 && qt.steps[255].locks[1].value==17);
 assert(qt.mutation_source[200].locks[0].value==.3f && qt.automation[0].values[255]==.6f && q->macros[0].routes[0].high==.9f && q->gesture.points[0].value==3);
 Bytes again;assert(encode_project(again,*q) && again.data==file.data);
 q->unavailable_targets.count=0;assert(!valid_project(*q));q->tempo=99;file.data.back()^=1;file.pos=0;assert(!decode_project(file,*q) && q->tempo==99 && q->unavailable_targets.count==0);
 UnavailableTargetOwners full;char guid[65];for(unsigned i=0;i<64;++i){std::snprintf(guid,sizeof guid,"future.%u",i);assert(full.intern(guid,unsigned(__builtin_strlen(guid)))==int(i));}assert(full.intern("future.0",8)==0 && full.intern("overflow",8)<0);
 // Registry lifetime belongs to the project; clearing cannot leak stale handles.
 make_empty_project(*p);assert(p->unavailable_targets.count==0 && valid_project(*p));
 std::puts("Unavailable targets: GUID/local/value retention, extended/frozen steps, inert resolution, bounds and transactional rejection passed");
}
