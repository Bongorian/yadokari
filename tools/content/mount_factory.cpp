#include "session_files.h"
#include "factory_library.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <cstdio>

using namespace groove;
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::filesystem::current_path(argv[1]);
 unsigned count=0;
 for(const auto& original:kFactoryLibrary){
  if(!original.path)continue;
  DesktopFiles fs;
  DesktopSampleAssets samples;
  samples.files(&fs);
  auto song=std::make_unique<ProjectState>();
  make_empty_project(*song);
  auto engine=std::make_unique<Engine>();
  auto ui=std::make_unique<Performance>(*engine,*song);
  SessionFiles files(fs,samples,*engine,*song,*ui);
  if(!engine->stage_project(*song))return 1;
  float out[128]{};
  engine->render(out,64);
  ui->poll();
  LibraryEntry expected;
  if(!read_library_entry(fs,original.path,expected) || !files.load_preset(original.path)
     || !same_preset_revision(song->tracks[0].preset_identity,expected.identity)
     || song->tracks[0].instrument!=expected.instrument){
   std::fprintf(stderr,"Factory mount failed: %s (%s)\n",original.path,ui->browser().status);
   return 1;
  }
  ++count;
 }
 std::printf("Factory originals mounted from staged DATA: %u\n",count);
 return count==187?0:1;
}
