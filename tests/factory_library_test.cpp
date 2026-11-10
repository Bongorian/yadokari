#include "factory_library.h"
#include "project_files.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <memory>
#include <vector>
#include <cstdio>
using namespace groove;
int main(int argc,char** argv){
 assert(argc==2);
 DesktopFiles fs;auto project=std::make_unique<ProjectState>();std::vector<uint8_t> raw(kFactoryLibraryMaxBytes);unsigned presets=0,kits=0;FactoryLibraryBank bank;assert(bank.ready());
 for(const auto& entry:kFactoryLibrary){if(!entry.path)continue;assert(bank.unpack(entry,raw.data(),unsigned(raw.size())));char master[192];const char* kind=data_under(entry.path,"FACTORY/KITS")?"KITS":"PRESETS";std::snprintf(master,sizeof master,"%s/%s/%s",argv[1],kind,filename(entry.path));std::vector<uint8_t> source(entry.bytes);unsigned got=0;assert(fs.read(master,0,source.data(),entry.bytes,got) && got==entry.bytes && !__builtin_memcmp(source.data(),raw.data(),got));PreservedOwner preserved;assert(preserved && read_project_candidate(fs,master,*project,nullptr,preserved.get()) && !preserved->dropped_unknown);ChunkBuffer canonical;assert(encode_project_buffer(canonical,*project,preserved.get()) && canonical.size==source.size() && !__builtin_memcmp(canonical.data,source.data(),source.size()));assert(project_paths(*project,entry.path,true));for(const auto& asset:project->assets)if(asset.path[0])assert(data_under(asset.path,"ASSETS"));if(project->content_kind==ContentKind::Preset)++presets;else{assert(project->content_kind==ContentKind::Kit);++kits;}assert(!bank.unpack(entry,raw.data(),entry.bytes-1));}
 assert(presets+kits==kFactoryLibraryCount);
 uint8_t digest[32]{},out[8]{},literal[]={0,'x'},overlap[]={0,'x',0x80,1,0};update::Sha256::hash(reinterpret_cast<const uint8_t*>("xxxx"),4,digest);assert(unpack_factory_library(overlap,sizeof overlap,out,4,digest));
 const uint8_t invalid[][4]={{0x80,0,0,0},{0x80,1,0,0},{2,'a',0,0},{0,'x',0x80,0}};
 for(const auto& bad:invalid){assert(!unpack_factory_library(bad,4,out,4,digest));}
 assert(!unpack_factory_library(literal,sizeof literal,out,1,digest));assert(!unpack_factory_library(overlap,4,out,4,digest));assert(!unpack_factory_library(overlap,5,out,3,digest));
 std::printf("Factory library: current allocated inventory checked (regeneration pending if empty); byte integrity, bounds and corruption checks passed\n");
}
