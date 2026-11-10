#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdio>
struct Sink{std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
struct Source{const std::vector<uint8_t>& bytes;size_t pos=0;bool read(uint8_t* p,size_t n){if(n>bytes.size()-pos)return false;for(size_t i=0;i<n;++i)p[i]=bytes[pos++];return true;}bool at_end()const{return pos==bytes.size();}};
int main(){using namespace groove;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::DaisyAnalogDrums;p->routing.groups[0].members=1;p->routing.groups[0].insert_fx[0]=15;p->routing.groups[0].insert_fx[1]=1;p->routing.groups[0].insert_fx[2]=.8f;
 assert(valid_routing(p->routing));Sink sink;assert(encode_project(sink,*p));auto restored=std::make_unique<ProjectState>();Source source{sink.bytes};assert(decode_project(source,*restored));assert(restored->routing.groups[0].insert_fx[0]==15 && restored->routing.groups[0].insert_fx[2]==.8f);
 auto dry=std::make_unique<Engine>(8000),wet=std::make_unique<Engine>(8000);auto plain=std::make_unique<ProjectState>(*p);plain->routing.groups[0].insert_fx[0]=0;assert(dry->stage_project(*plain) && wet->stage_project(*p));float out1[128],out2[128];dry->render(out1,0);wet->render(out2,0);assert(dry->submit({CommandType::Trigger,0,0,127}));assert(wet->submit({CommandType::Trigger,0,0,127}));float difference=0,energy=0;for(unsigned b=0;b<128;++b){dry->render(out1,64);wet->render(out2,64);for(unsigned i=0;i<128;++i){assert(std::isfinite(out2[i]));difference+=fabsf(out1[i]-out2[i]);energy+=fabsf(out1[i]);}}assert(energy>1 && difference>1);
 auto ui=std::make_unique<Performance>(*wet,*p);ui->set_mode(UiMode::Routing);assert(ui->soft_key(0) && ui->routing_fx_page());assert(ui->knob(1,1000));assert(p->routing.groups[0].insert_fx[0]==16);assert(ui->knob(7,-1000) && !(p->routing.groups[0].members&1));assert(ui->knob(7,1000) && (p->routing.groups[0].members&1));assert(ui->encoder(1) && ui->group_cursor()==1);assert(ui->soft_key(0) && !ui->routing_fx_page());
 std::printf("group insert codec, audible route and UI passed (difference %.1f)\n",difference);
}
