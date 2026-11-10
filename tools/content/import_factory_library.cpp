// Normalize a user-authored SD library for its FACTORY install location.
// ContentKind is authoritative; values, FX and preserved chunks stay intact.
#include "project_files.h"
#include "data_layout.h"
#include "../../platform/desktop/file_access.h"
#include <filesystem>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdio>
using namespace groove;
static void clean_paths(ProjectState& project){for(auto& a:project.assets){bool ended=false;for(char& c:a.path){if(ended)c=0;else if(!c)ended=true;}}}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: import-factory-library SD_ROOT OUTPUT_ROOT\n");return 2;}
 const std::filesystem::path input=argv[1],output=argv[2];DesktopFiles fs;
 auto source=std::make_unique<ProjectState>(),roundtrip=std::make_unique<ProjectState>();std::vector<std::filesystem::path> paths;
 for(const auto& e:std::filesystem::recursive_directory_iterator(input))if(e.is_regular_file() && data_mgx_file(e.path().c_str()))paths.push_back(e.path());
 std::sort(paths.begin(),paths.end());unsigned presets=0,kits=0;
 for(const auto& from:paths){
  std::fprintf(stderr,"Importing %s\n",from.c_str());
  PreservedStore original;if(!read_project_candidate(fs,from.c_str(),*source,nullptr,&original) || original.dropped_unknown)return 1;
  if(source->content_kind!=ContentKind::Preset && source->content_kind!=ContentKind::Kit)return 1;
  const auto logical_source=from.lexically_relative(input).generic_string();if(!project_paths(*source,logical_source.c_str(),true))return 1;
  // Route the two legacy sample references through the supported DATA shelf.
  for(auto& asset:source->assets)if(data_under(asset.path,"assets/factory")){char path[96];std::snprintf(path,sizeof path,"ASSETS/SAMPLES/GBX-%08x-%s",asset.metadata.content_crc,filename(asset.path));if(!copy_text(asset.path,sizeof asset.path,path))return 1;}
  clean_paths(*source);ChunkBuffer before;if(!encode_project_buffer(before,*source,&original))return 1;
  char name[112],target[192];if(!data_content_name(name,sizeof name,from.filename().c_str(),unsigned(source->content_kind)))return 1;
  const char* shelf=source->content_kind==ContentKind::Preset?"PRESETS":"KITS";
  std::string logical=std::string("FACTORY/")+shelf+"/LIBRARY/"+name;
  if(!copy_text(target,sizeof target,logical.c_str()) || !project_paths(*source,target,false))return 1;
  const auto destination=output/shelf/name;std::filesystem::create_directories(destination.parent_path());
  if(std::filesystem::exists(destination)){std::fprintf(stderr,"Refusing duplicate/existing master: %s\n",destination.c_str());return 1;}
  {FileWriter writer(fs,destination.c_str());if(!encode_project(writer,*source,&original) || !writer.finish())return 1;}
  PreservedStore decoded;if(!read_project_candidate(fs,destination.c_str(),*roundtrip,nullptr,&decoded) || decoded.dropped_unknown || !project_paths(*roundtrip,target,true)){std::fprintf(stderr,"Decode/resolve failed\n");return 1;}
  clean_paths(*roundtrip);ChunkBuffer after;if(!encode_project_buffer(after,*roundtrip,&decoded))return 1;
  if(before.size!=after.size || __builtin_memcmp(before.data,after.data,before.size)){std::fprintf(stderr,"Canonical content changed: %s\n",from.c_str());return 1;}
  if(source->content_kind==ContentKind::Preset)++presets;else ++kits;
 }
 std::printf("Imported %u presets / %u kits; canonical values, FX, assets and preserved chunks identical\n",presets,kits);
 return presets+kits?0:1;
}
