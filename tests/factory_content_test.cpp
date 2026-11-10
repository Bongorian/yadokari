#include "factory_boot_gate.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <unistd.h>
int main(int argc,char** argv){using namespace groove;assert(argc==2);
 FILE* sample_file=std::fopen(argv[1],"rb");assert(sample_file);std::vector<uint8_t> sample(96044);assert(std::fread(sample.data(),1,sample.size(),sample_file)==sample.size());std::fclose(sample_file);
 char tmp[]="/tmp/mk3-factory-test-XXXXXX";assert(mkdtemp(tmp) && chdir(tmp)==0);DesktopFiles fs;FactoryInstallReport first;
 assert(install_factory_content(fs,sample.data(),unsigned(sample.size()),first));constexpr unsigned required=sizeof kFactoryCatalog/sizeof *kFactoryCatalog;assert(first.checked==required && first.repaired==required);assert(!fs.exists("FACTORY/KITS/Techno-01.mgxk"));assert(benchmark_file_matches(fs,"BENCH.MGXS"));assert(fs.size("FACTORY/DEBUG/Test-Tone.mgxp")<32768);assert(data_content_kind(fs,"FACTORY/DEBUG/Test-Tone.mgxp")==3);
 static_assert(kFutureFactoryPresets==22,"Declared built-in recipe inventory changed");
 auto p=std::make_unique<ProjectState>();uint8_t hashes[2][32]{};
 for(unsigned n=16;n<22;++n){char path[160];std::snprintf(path,sizeof path,"FACTORY/PRESETS/DRUMS/%s.mgxp",future_factory_preset_name(n));assert(read_project_file(fs,path,*p));assert(p->tracks[0].instrument==(n<19?Instrument::DaisyAnalogDrums:Instrument::DaisySyntheticDrums));assert(p->tracks[0].engine_parameters[0]==float((n-16)%3)*.5f);}
 for(const auto& file:kFactorySynthDrums){assert(fs.size(file.path)==file.bytes);if(data_mgx_file(file.path)){assert(read_project_file(fs,file.path,*p) && project_paths(*p,file.path,true));if(p->content_kind==ContentKind::Kit)for(unsigned n=0;n<6;++n)assert(fs.exists(p->assets[n].path) && p->tracks[0].kit.pads[n].sample[0]==n);}else{WavView wav;uint64_t at=0;assert(sample_file_header(fs,file.path,wav,at) && wav.rate==48000 && wav.channels==1 && wav.bits==16);}}
 auto digest=[&](const char* path,uint8_t* out){std::vector<uint8_t> bytes(fs.size(path));unsigned got=0;assert(fs.read(path,0,bytes.data(),unsigned(bytes.size()),got) && got==bytes.size());update::Sha256::hash(bytes.data(),bytes.size(),out);};
 assert(read_project_file(fs,kFactoryStartupProject,*p));for(const auto& preset:p->sounds)assert(preset.instrument==Instrument::Empty);digest(kFactoryStartupProject,hashes[0]);
 for(const char* file:kRetiredFactoryDemos){char path[160];assert(join_path(path,sizeof path,"FACTORY/DEMOS",file) && !fs.exists(path));}
 const uint8_t retired[]={1,2,3};assert(fs.write("FACTORY/DEMOS/01-Neon-House.mgx",0,retired,sizeof retired,true));
 assert(fs.write("FACTORY/DEBUG/Benchmark.mgx",0,retired,sizeof retired,true));
 assert(fs.write("FACTORY/PRESETS/LIBRARY/M1 Piano.mgxp",0,retired,sizeof retired,true));
 assert(fs.write("PRESETS/M1 Piano.mgxp",0,retired,sizeof retired,true));
 assert(fs.write("FACTORY/PRESETS/LIBRARY/DX7 Lately Bass.mgxp",0,retired,sizeof retired,true));
 assert(fs.write("PRESETS/DX7 Lately Bass.mgxp",0,retired,sizeof retired,true));
 FactoryInstallReport again;assert(install_factory_content(fs,sample.data(),unsigned(sample.size()),again) && again.repaired==0 && again.retired==4);assert(!fs.exists("FACTORY/DEMOS/01-Neon-House.mgx") && !fs.exists("FACTORY/DEBUG/Benchmark.mgx"));
 assert(!fs.exists("FACTORY/PRESETS/LIBRARY/M1 Piano.mgxp") && fs.exists("FACTORY/PRESETS/LIBRARY/GM Grand Piano.mgxp"));
 assert(fs.size("PRESETS/M1 Piano.mgxp")==sizeof retired);assert(fs.size("PRESETS/DX7 Lately Bass.mgxp")==sizeof retired);assert(!fs.exists("FACTORY/PRESETS/LIBRARY/DX7 Lately Bass.mgxp"));
 // A modified template and a corrupted debug WAV recover, user edits persist.
 const uint8_t bad[]={1,2,3};assert(read_project_file(fs,kFactoryStartupProject,*p));p->tempo=99;assert(save_recoverable(fs,kFactoryStartupProject,*p));assert(fs.write("FACTORY/DEBUG/440Hz.wav",0,bad,sizeof bad,true));assert(fs.write("PROJECTS/User.mgx",0,bad,sizeof bad,true));
 FactoryInstallReport repair;assert(install_factory_content(fs,sample.data(),unsigned(sample.size()),repair) && repair.repaired==2);assert(fs.size("PROJECTS/User.mgx")==3);digest(kFactoryStartupProject,hashes[1]);assert(update::equal(hashes[1],hashes[0],32));
 assert(factory_path("factory/DEMOS/01-Neon-House.mgx") && factory_path("PROJECTS/../FACTORY/DEBUG/440Hz.wav") && !factory_path("FACTORY-User/a.mgx"));
 // Cleanup is independent of decoding: even an unsupported/corrupt MGX and its
 // recoverable temporary must survive a firmware format bump.
 for(const char* path:{"PROJECTS/Recovery.mgxs","PROJECTS/v8.mgx","kits/old.mgxk","PRESETS/unknown.mgxp","PROJECTS/Recovery.mgxs.tmp"})assert(!data_disposable_file(path));
 assert(data_disposable_file("assets/catalog.CSV") && !data_disposable_file("wpa_supplicant.conf") && !data_disposable_file("assets/NOTICE.txt") && !data_disposable_file("RECORDINGS/take.wav") && !data_disposable_file("firmware/manifest.csv"));
 assert(data_browser_entry("","FACTORY",true) && !data_browser_entry("","GROOVE",true) && !data_browser_entry("","BENCH.MGXS",false));
 assert(data_browser_entry("assets","song.MGX",false) && data_browser_entry("assets","sequence.MIDI",false) && !data_browser_entry("assets","catalog.csv",false));
 for(unsigned i=0;i<90;++i){char name[40];std::snprintf(name,sizeof name,"PROJECTS/%03u.csv",i);assert(fs.write(name,0,bad,3,true));}
 FileEntry entries[64];assert(fs.list("PROJECTS",entries,64)==1 && data_equal(entries[0].name,"User.mgx"));
 // The fresh project action must clear arrangement and selected Morph set.
 p->song.enabled=true;p->song.length=8;p->morph_set=3;make_empty_project(*p);assert(!p->song.enabled && !p->song.length && !p->morph_set);
 std::printf("factory: operational debug, empty pool, retired songs, repair/idempotence, user preservation, browser filtering passed (%s)\n",tmp);
}
