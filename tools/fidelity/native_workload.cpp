#include "native_stress_project.h"
#include "project_codec.h"
#include "update/sha256.h"
#include <memory>
#include <cstdio>
struct HashWriter {groove::update::Sha256 hash;size_t size=0;bool write(const uint8_t* data,size_t count){hash.add(data,count);size+=count;return true;}};
int main(){auto p=std::make_unique<groove::ProjectState>();puts("{\"version\":1,\"host_rate\":48000,\"tracks\":16,\"warmup_seconds\":2,\"measured_seconds_per_case\":6,\"cases\":[");
 for(unsigned c=0;c<19;++c){groove::make_native_stress_project(*p,c);if(!groove::valid_project(*p))return 1;HashWriter w;if(!groove::encode_project(w,*p))return 2;uint8_t digest[32];w.hash.finish(digest);printf("%s{\"case\":%u,\"engine\":\"%s\",\"model\":%u,\"quality\":%u,\"bytes\":%zu,\"sha256\":\"",c?",":"",c,c<3?"RESONATOR":"TEXTURE",c<3?c:(c-3)/4,c<3?0:(c-3)%4,w.size);for(auto byte:digest)printf("%02x",byte);puts("\"}");}
 puts("]}");}
