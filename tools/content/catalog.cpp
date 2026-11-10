#include "factory_content.h"
#include "../../platform/desktop/file_access.h"
#include <cstdio>
#include <vector>
#include <unistd.h>
#include <filesystem>
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: catalog SAMPLE.wav DATA_DIRECTORY\n");return 2;}
 // This checkout-owned build catalogue is disposable. Never clear a caller's
 // SD/user directory, nor follow a symlink to one. Source assets stay intact.
 const auto output=std::filesystem::absolute(argv[2]).lexically_normal();
 const auto owned=std::filesystem::current_path()/"build/factory-data";
 if(output==owned){if(std::filesystem::is_symlink(output) || std::filesystem::is_symlink("build"))return 1;std::filesystem::remove_all(output);std::filesystem::create_directories(output);}
 FILE* f=std::fopen(argv[1],"rb");if(!f)return 1;std::vector<uint8_t> sample(96044);bool ok=std::fread(sample.data(),1,sample.size(),f)==sample.size();std::fclose(f);if(!ok || chdir(argv[2]))return 1;
 groove::DesktopFiles fs;groove::FactoryInstallReport report;ok=groove::install_factory_content(fs,sample.data(),unsigned(sample.size()),report);

 std::printf("Factory catalog checked %u repaired %u demos %u ok %u retired %u\n",report.checked,report.repaired,groove::kFactoryDemoCount,unsigned(ok),report.retired);return ok?0:1;
}
