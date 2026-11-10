#include "factory_boot_gate.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <unistd.h>
struct TraceFiles:groove::DesktopFiles {
 unsigned reads=0,writes=0,stats=0,content_reads=0;bool fail_marker=false;
 bool read(const char* p,uint64_t at,void* data,unsigned n,unsigned& got)override{++reads;if(__builtin_strcmp(p,groove::kFactoryBootMarker))++content_reads;return DesktopFiles::read(p,at,data,n,got);}
 bool write(const char* p,uint64_t at,const void* data,unsigned n,bool trunc)override{++writes;if(fail_marker && !__builtin_strcmp(p,groove::kFactoryBootTemporary))return false;return DesktopFiles::write(p,at,data,n,trunc);}
 bool revision(const char* p,groove::FileRevision& out)override{++stats;return DesktopFiles::revision(p,out);}
 void reset(){reads=writes=stats=content_reads=0;}
};
int main(int argc,char** argv){using namespace groove;assert(argc==2);FILE* source=fopen(argv[1],"rb");assert(source);std::vector<uint8_t> sample(96044);assert(fread(sample.data(),1,sample.size(),source)==sample.size());fclose(source);
 char dir[]="/tmp/mgx-factory-gate-XXXXXX";assert(mkdtemp(dir) && !chdir(dir));TraceFiles fs;auto install=[&](unsigned build,FactoryBootReport& r){return prepare_factory_content(fs,sample.data(),unsigned(sample.size()),build,r);};
 FactoryBootReport initial;assert(install(490,initial) && !initial.skipped && initial.marker_saved && initial.install.repaired==sizeof kFactoryCatalog/sizeof *kFactoryCatalog);
 fs.reset();FactoryBootReport normal;assert(install(490,normal) && normal.skipped && !normal.install.checked && fs.reads==1 && !fs.content_reads && !fs.writes && fs.stats==sizeof kFactoryCatalog/sizeof *kFactoryCatalog);
 // Exercise all existing file decoders without putting the pool on the stack.
 auto project=std::make_unique<ProjectState>();for(const auto& file:kFactoryCatalog){assert(fs.size(file.path)==file.bytes);if(data_mgx_file(file.path))assert(read_project_file(fs,file.path,*project));else if(data_suffix(file.path,".wav")){WavView view;uint64_t at=0;assert(sample_file_header(fs,file.path,view,at));}else assert(soundfont_file_header(fs,file.path));}
 uint8_t change=123;assert(fs.write("FACTORY/DEBUG/440Hz.wav",44,&change,1,false));fs.reset();FactoryBootReport edited;assert(install(490,edited) && edited.skipped && !fs.content_reads && !fs.writes);
 FactoryBootReport update;assert(install(491,update) && !update.skipped && update.install.repaired==1);fs.reset();FactoryBootReport second;assert(install(491,second) && second.skipped && !fs.content_reads);
 // Bad digest, truncated record and a self-consistent but foreign catalog hash.
 for(unsigned corruption=0;corruption<3;++corruption){uint8_t marker[96];unsigned got=0;assert(fs.read(kFactoryBootMarker,0,marker,sizeof marker,got) && got==sizeof marker);marker[32]^=1;if(corruption==2)update::Sha256::hash(marker,64,marker+64);assert(fs.write(kFactoryBootMarker,0,marker,corruption==1?20:96,true));FactoryBootReport bad;assert(install(491,bad) && !bad.skipped && !bad.install.repaired);}
 assert(fs.remove("FACTORY/DEBUG/440Hz.wav"));FactoryBootReport missing;assert(install(491,missing) && !missing.skipped && missing.install.repaired==1);
 assert(fs.write("FACTORY/DEBUG/440Hz.wav",0,&change,1,true));FactoryBootReport resized;assert(install(491,resized) && !resized.skipped && resized.install.repaired==1);
 // Preserve user data, invalidate the old marker before a failed publication.
 assert(fs.write("PROJECTS/Keep.mgxs",0,&change,1,true));fs.fail_marker=true;FactoryBootReport interrupted;assert(!install(492,interrupted) && !fs.exists(kFactoryBootMarker));fs.fail_marker=false;FactoryBootReport retry;assert(install(492,retry) && !retry.skipped && fs.size("PROJECTS/Keep.mgxs")==1);
 assert(copy_file(fs,"FACTORY/KITS/Circuit.mgxk",kFactoryOptionalKit));FactoryBootReport optional;assert(install(493,optional) && !optional.skipped);fs.reset();FactoryBootReport optional_skip;assert(install(493,optional_skip) && optional_skip.skipped && fs.stats==sizeof kFactoryCatalog/sizeof *kFactoryCatalog+1 && !fs.content_reads && !fs.writes);assert(fs.remove(kFactoryOptionalKit));FactoryBootReport optional_missing;assert(install(493,optional_missing) && !optional_missing.skipped);
 std::puts("PASS factory gate: first/update/full, stat-only skip, corrupt/missing/size, interrupted marker, optional kit, all factory decoders and user preservation");
}
