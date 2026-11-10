#include "factory_library_audit.h"
#include "factory_content.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <vector>
#include <unistd.h>
using namespace groove;
int main(int argc,char** argv){
 assert(argc==2);FILE* sample=std::fopen(argv[1],"rb");assert(sample);std::vector<uint8_t> pcm(96044);assert(std::fread(pcm.data(),1,pcm.size(),sample)==pcm.size());std::fclose(sample);
 auto cwd=std::filesystem::current_path();char root[]="/tmp/factory-audit-XXXXXX";assert(mkdtemp(root) && !chdir(root));
 DesktopFiles fs;FactoryInstallReport install;assert(install_factory_content(fs,pcm.data(),unsigned(pcm.size()),install));
 const uint8_t user[]={1,2,3};assert(fs.write("PROJECTS/User.mgxs",0,user,sizeof user,true));
 DesktopSampleAssets library;library.files(&fs);auto project=std::make_unique<ProjectState>();make_empty_project(*project);
 auto engine=std::make_unique<Engine>(48000);auto ui=std::make_unique<Performance>(*engine,*project);SessionFiles files(fs,library,*engine,*project,*ui);
 assert(engine->stage_project(*project));ui->project_loaded();float output[128];engine->render(output,64);ui->poll();
 auto browser=std::make_unique<BrowserState>(ui->browser());const auto report=audit_factory_library(files,fs,*ui);
 assert(report.complete && report.presets==163 && report.kits==24 && report.representative && !report.old_name && !report.old_formats);
 assert(!__builtin_memcmp(browser.get(),&ui->browser(),sizeof *browser));assert(project->tracks[0].instrument==Instrument::Empty);
 // Follow the actual shared Browser LOAD action for the representative.
 auto& b=ui->browser();b.library_list=true;b.library_originals=true;b.library_kits=false;b.library_instrument=Instrument::Fm6;ui->set_mode(UiMode::Library);
 assert(files.browse_library(FileAction::LibraryOpen));bool found=false;
 for(unsigned n=0;n<b.library_total;++n){if(data_equal(b.library_paths[b.cursor],kFactoryRepresentative)){found=true;break;}b.library_delta=1;assert(files.browse_library(FileAction::LibraryMove));}
 assert(found && !b.previewing && ui->soft_key(5));for(unsigned i=0;i<4096 && ui->visual().attached_instrument[0]!=unsigned(Instrument::Fm6);++i){files.service();engine->render(output,64);ui->poll();}
 LibraryEntry expected;assert(read_library_entry(fs,kFactoryRepresentative,expected));assert(project->tracks[0].instrument==Instrument::Fm6 && same_preset_revision(project->tracks[0].preset_identity,expected.identity));assert(ui->visual().attached_instrument[0]==unsigned(Instrument::Fm6));
 uint8_t actual[3]{};unsigned got=0;assert(fs.read("PROJECTS/User.mgxs",0,actual,sizeof actual,got) && got==3 && !__builtin_memcmp(actual,user,3));
 assert(fs.write("FACTORY/PRESETS/LIBRARY/DX7 Lately Bass.mgxp",0,user,sizeof user,true));auto bad=audit_factory_library(files,fs,*ui);assert(bad.old_name && bad.old_formats==1);
 std::filesystem::current_path(cwd);std::filesystem::remove_all(root);
 std::puts("Factory Browser:163 presets+24 kits, pagination/engine filters, original state retained, TX81z LOAD/mount, stale factory detection and user bytes preserved passed");
}
