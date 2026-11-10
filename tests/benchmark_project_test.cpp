#include "benchmark_project.h"
#include "project_codec.h"
#include <cassert>
#include <memory>
#include <vector>
struct Buffer {std::vector<uint8_t> bytes;size_t cursor=0;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(cursor+n>bytes.size())return false;for(size_t i=0;i<n;++i)p[i]=bytes[cursor+i];cursor+=n;return true;}bool at_end()const{return cursor==bytes.size();}};
struct MemoryFile:groove::IFileAccess {
 std::vector<uint8_t> bytes;
 unsigned list(const char*,groove::FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t off,void* data,unsigned wanted,unsigned& got)override{if(off>bytes.size())return false;got=unsigned(bytes.size()-off);if(got>wanted)got=wanted;for(unsigned i=0;i<got;++i)static_cast<uint8_t*>(data)[i]=bytes[size_t(off)+i];return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}
 bool move(const char*,const char*)override{return false;}
 bool remove(const char*)override{return false;}
 bool directory(const char*)override{return false;}
 uint64_t size(const char*)override{return bytes.size();}
 bool exists(const char*)override{return true;}
};
int main(){
 auto p=std::make_unique<groove::ProjectState>();groove::make_benchmark_project(*p);
 assert(p->tempo==140 && p->chain.enabled && p->chain.length==16 && p->chain.bars==1);
 bool instruments[19]{},models[24]{};
 for(unsigned scene=0;scene<16;++scene){assert(p->chain.scenes[scene]==scene);for(unsigned t=0;t<16;++t){const auto tr=track_at(*p,scene,t);assert(p->scenes[scene].pattern_for_track[t]==scene);unsigned hits=0;for(unsigned i=0;i<16;++i)hits+=tr.steps[i].enabled;if(hits){instruments[unsigned(tr.instrument)]=true;if(tr.instrument==groove::Instrument::PlaitsMacro){assert(tr.plaits_model<24);models[tr.plaits_model]=true;}assert(hits>=2);}}}
 const groove::Instrument expected[]={groove::Instrument::DaisyAnalogDrums,groove::Instrument::DaisyAnalogDrums,groove::Instrument::DaisyAnalogDrums,groove::Instrument::Sampler,groove::Instrument::Acid,groove::Instrument::FM,groove::Instrument::PlaitsMacro,groove::Instrument::DaisySynth,groove::Instrument::DaisyAnalogDrums,groove::Instrument::DaisySyntheticDrums,groove::Instrument::PolySaw,groove::Instrument::ChipClock,groove::Instrument::WestCoast};
 for(auto kind:expected)assert(instruments[unsigned(kind)]);
 unsigned model_count=0;for(bool found:models)model_count+=found;assert(model_count==4);
 for(unsigned t=0;t<groove::kTracks;++t)if(p->tracks[t].instrument==groove::Instrument::PlaitsMacro)assert(p->tracks[t].plaits_model==(t*5)%24);
 for(const auto& sound:p->sounds)assert(sound.instrument==groove::Instrument::Empty);
 Buffer one,two;assert(groove::encode_project(one,*p));uint8_t hash[32];groove::update::Sha256::hash(one.bytes.data(),one.bytes.size(),hash);assert(groove::update::equal(hash,groove::kBenchmarkFileSha256,32));MemoryFile fs;fs.bytes=one.bytes;assert(groove::benchmark_file_matches(fs,"BENCH.MGXS"));fs.bytes[42]^=1;assert(!groove::benchmark_file_matches(fs,"BENCH.MGXS"));auto restored=std::make_unique<groove::ProjectState>();assert(groove::decode_project(one,*restored));assert(groove::encode_project(two,*restored));assert(one.bytes==two.bytes);
 auto again=std::make_unique<groove::ProjectState>();groove::make_benchmark_project(*again);Buffer three;assert(groove::encode_project(three,*again) && three.bytes==two.bytes);
}
