#include "engine.h"
#include "project.h"
#include <cassert>
#include <memory>
#include <vector>
using namespace groove;
static std::vector<uint64_t> run(unsigned chunk){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=120;
 auto track=track_at(*p,0,0);track.instrument=Instrument::ExternalMidi;track.length=2;track.steps[0].enabled=true;track.steps[1].enabled=true;track.steps[0].note=60;track.steps[1].note=62;
 auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*p));assert(e->submit({CommandType::Start,0,0,0}));
 std::vector<float> out(chunk*2);std::vector<uint64_t> notes;
 auto render=[&](unsigned total){for(unsigned n=0;n<total;n+=chunk){e->render(out.data(),chunk);MidiEvent m;while(e->pop_midi(m))if((m.bytes[0]&0xf0)==0x90 && m.bytes[2])notes.push_back(m.sample);}};
 render(512);assert(e->submit({CommandType::SetSwing,0,0,80}));Command micro{CommandType::SetMicrotiming,0,1,0};micro.scalar=96;assert(e->submit(micro));render(7680);
 return notes;
}
int main(){auto a=run(8),b=run(64);assert(a==b);assert(a.size()>=5);assert(a[1]>1000);}
