// Build-only distribution catalogue; source assets/provenance stay in assets/.
#include "project_files.h"
#include "data_layout.h"
#include "preset_assets.h"
#include "../../platform/desktop/file_access.h"
#include <filesystem>
#include <memory>
#include <cstdio>
using namespace groove;
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::filesystem::path root=argv[1];DesktopFiles fs;auto project=std::make_unique<ProjectState>();unsigned files=0,samples=0;
 for(const char* source:{"assets/kits","assets/presets"}){
  for(const auto& entry:std::filesystem::directory_iterator(source)){
   std::string from=entry.path().generic_string();if(!entry.is_regular_file() || (!data_suffix(from.c_str(),".mgxp") && !data_suffix(from.c_str(),".mgxk")))continue;
   if(!read_project_file(fs,from.c_str(),*project) || !project_paths(*project,from.c_str(),true))return 1;
   if(project->content_kind!=ContentKind::Kit && project->content_kind!=ContentKind::Preset)return 1;
   const char* shelf=project->content_kind==ContentKind::Kit?"FACTORY/KITS":"FACTORY/PRESETS";
   uint64_t needed=project_asset_mask(*project);unsigned asset_index=0;
   for(auto& asset:project->assets){unsigned slot=asset_index++;if(!(needed&(uint64_t(1)<<slot))){asset={};continue;}if(!asset.path[0])continue;uint32_t crc=0;if(!file_checksum(fs,asset.path,crc))return 1;
    char name[66];std::snprintf(name,sizeof name,"%08x-",unsigned(crc));if(!copy_text(name+9,sizeof name-9,filename(asset.path)))copy_text(name+9,sizeof name-9,"sample.wav");
    char logical[96];const char* folder=project->content_kind==ContentKind::Kit?"FACTORY/DRUMS":"FACTORY/PRESETS/SAMPLES";if(!join_path(logical,sizeof logical,folder,name))return 1;
    auto to=root/logical;std::filesystem::create_directories(to.parent_path());std::string path=to.generic_string();
    if(!fs.exists(path.c_str()) || !same_file(fs,asset.path,path.c_str())){if(!copy_file(fs,asset.path,path.c_str()))return 1;++samples;}
    copy_text(asset.path,sizeof asset.path,logical);asset.metadata.content_crc=crc;
   }
   char logical[192];if(!join_path(logical,sizeof logical,shelf,entry.path().filename().string().c_str()) || !project_paths(*project,logical,false))return 1;
   auto to=root/logical;std::filesystem::create_directories(to.parent_path());std::string output=to.generic_string();FileWriter writer(fs,output.c_str());if(!encode_project(writer,*project) || !writer.finish())return 1;++files;
  }
 }
 std::filesystem::create_directories(root/"FACTORY");std::filesystem::copy_file("assets/factory/NOTICE.txt",root/"FACTORY/NOTICE.txt",std::filesystem::copy_options::overwrite_existing);
 if(std::filesystem::exists("assets/factory/library.json"))std::filesystem::copy_file("assets/factory/library.json",root/"FACTORY/library.json",std::filesystem::copy_options::overwrite_existing);
 std::printf("Canonical factory distribution: %u containers, %u refreshed sample files; source assets unchanged\n",files,samples);
}
