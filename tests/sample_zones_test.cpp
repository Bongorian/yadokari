#include "project_codec.h"
#include <cassert>
#include <memory>
#include <vector>
using namespace groove;
struct Buffer {std::vector<uint8_t> data;size_t at=0;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(at+n>data.size())return false;__builtin_memcpy(p,data.data()+at,n);at+=n;return true;}bool at_end()const{return at==data.size();}};
int main(){
 auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 auto& edit=track_at(*project,0,0).sample_edit;
 edit.slice_count=edit.zone_count=4;for(unsigned i=0;i<=4;++i)edit.markers[i]=float(i)/4;
 edit.zones[0]={48,0,54,1,63};edit.zones[1]={48,0,54,64,127};
 edit.zones[2]={60,55,127,1,63};edit.zones[3]={60,55,127,64,127};
 assert(valid_sample_edit(edit));
 assert(select_sample_zone(edit,48,40)==0 && select_sample_zone(edit,48,100)==1);
 assert(select_sample_zone(edit,72,40)==2 && select_sample_zone(edit,72,100)==3);
 Buffer buffer;assert(encode_project(buffer,*project));assert(buffer.data[0]=='M' && buffer.data[1]=='G' && buffer.data[2]=='X' && buffer.data[3]=='C');
 auto restored=std::make_unique<ProjectState>();assert(decode_project(buffer,*restored));
 const auto& saved=track_at(*restored,0,0).sample_edit;
 assert(saved.zone_count==4 && saved.zones[3].root==60 && saved.zones[3].velocity_low==64);
 assert(select_sample_zone(saved,72,100)==3);
}
