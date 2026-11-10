#include "obsolete_content.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
using namespace groove;
struct FailingLog:DesktopFiles {bool write(const char* path,uint64_t at,const void* p,unsigned n,bool truncate)override{return data_equal(path,kRetirementLog)?false:DesktopFiles::write(path,at,p,n,truncate);}};
int main(){char folder[]="/tmp/mgx-retire-XXXXXX";assert(mkdtemp(folder) && !chdir(folder));DesktopFiles fs;uint8_t old[]={'M','G','X','C',12,0,1,2};uint8_t current[]={'M','G','X','C',13,0};uint8_t future[]={'M','G','X','C',14,0};
 assert(fs.write("old.mgxs",0,old,sizeof old,true));assert(retire_obsolete_container(fs,"old.mgxs")==RetirementResult::Removed && !fs.exists("old.mgxs"));std::ifstream log(kRetirementLog);std::string records((std::istreambuf_iterator<char>(log)),{});assert(records.find("\"action\":\"intent\"")<records.find("\"action\":\"removed\"") && records.find("\"sha256\":")!=std::string::npos && records.find("6f6c642e6d677873")!=std::string::npos);
 for(const char* name:{"now.mgxp","renamed.mgp","renamed.csv"}){assert(fs.write(name,0,current,sizeof current,true));assert(retire_obsolete_container(fs,name)==RetirementResult::ProtectedContainer && fs.exists(name));}
 assert(fs.write("future.mgxs",0,future,sizeof future,true));assert(retire_obsolete_container(fs,"future.mgxs")==RetirementResult::ProtectedContainer);assert(fs.write("material.wav",0,"RIFFabcd",8,true));assert(retire_obsolete_container(fs,"material.wav")==RetirementResult::Preserved);
 assert(fs.write("failed.mgxk",0,old,sizeof old,true));FailingLog failure;assert(retire_obsolete_container(failure,"failed.mgxk")==RetirementResult::Failed && fs.exists("failed.mgxk"));assert(fs.write("GROOVE/DEVICE.BIN",0,"MDEV1234",8,true));assert(retire_obsolete_container(fs,"GROOVE/DEVICE.BIN")==RetirementResult::Preserved);
 std::filesystem::current_path("/");std::filesystem::remove_all(folder);puts("Obsolete content: header classification, current/future/material preservation, durable intent before removal and log-failure safety passed");}
