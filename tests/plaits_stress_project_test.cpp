#include "../src/benchmark_project.h"
#include "../src/project_codec.h"
#include <vector>
#include <cassert>
#include <memory>
struct Buffer {std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
// Benchmark19-derived fixture: Factory/debug asset namespace; no old-workload performance comparison.
static void check_hash(const groove::ProjectState& p,const char* expected){Buffer b;assert(groove::encode_project(b,p));uint8_t digest[32];groove::update::Sha256::hash(b.bytes.data(),b.bytes.size(),digest);const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;++i){assert(hex[digest[i]>>4]==expected[i*2]);assert(hex[digest[i]&15]==expected[i*2+1]);}}
int main(){auto p=std::make_unique<groove::ProjectState>();groove::make_plaits_stress_project(*p);assert(groove::valid_project(*p));bool models[24]{};for(unsigned scene=0;scene<16;++scene)for(unsigned t=0;t<16;++t){const auto track=track_at(*p,scene,t);assert(track.instrument==groove::Instrument::PlaitsMacro && track.level<=0.025f);models[track.plaits_model]=true;for(unsigned step=0;step<16;++step)assert(track.steps[step].enabled==(step%4==0));}for(unsigned i=0;i<24;++i)assert(models[i]==(i<16));check_hash(*p,"9758a53827b75a45ebbd0dcc166b69b429fd817c19f80b0ba3eb5793721e7068");groove::make_plaits_stress_project(*p,300);assert(p->tempo==300 && groove::valid_project(*p));check_hash(*p,"3072de0bc5fcd52038ac5c891bd42fde09ea953e701055edac23d785cac6b7cc");}
