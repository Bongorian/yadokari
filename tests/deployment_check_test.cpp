#include "../firmware/deployment_check.h"
#include "update/project_snapshot.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
using namespace groove;
struct FaultFiles:DesktopFiles {
 bool fail_twin=false,fail_snapshot=false;
 FaultFiles():DesktopFiles(true){}
 bool write(const char* path,uint64_t offset,const void* data,unsigned n,bool truncate)override{
  if((fail_twin && std::strstr(path,"Twin-")) || (fail_snapshot && std::strstr(path,"Update-Recovery")))return false;
  return DesktopFiles::write(path,offset,data,n,truncate);
 }
};
struct Totals {unsigned passed=0,failed=0;};
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);auto previous=std::filesystem::current_path();char root[]="/tmp/mk3-deploy-XXXXXX";assert(mkdtemp(root));std::filesystem::current_path(root);FaultFiles fs;
 for(const char* d:{"PROJECTS","GROOVE","RECORDINGS","FACTORY","FACTORY/DEMOS","FACTORY/PRESETS","FACTORY/KITS","PRESETS","KITS"})assert(fs.directory(d));
 assert(fs.write("PROJECTS/Recovery.mgxs",0,"user recovery",13,true));assert(fs.write("GROOVE/CURRENT.TXT",0,"PROJECTS/Keep.mgxs",18,true));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=137;p->tracks[0].instrument=Instrument::Formant;p->patterns[0].tracks[0].steps[0].enabled=true;
 auto original=std::make_unique<ProjectState>(*p);auto e=std::make_unique<Engine>();assert(e->stage_project(*p));auto ui=std::make_unique<Performance>(*e,*p);DesktopSampleAssets library;library.files(&fs);auto files=std::make_unique<SessionFiles>(fs,library,*e,*p,*ui);files->project_locator("GROOVE/CURRENT.TXT");ui->project_path("PROJECTS/Keep.mgxs");float main[192]{},cue[192]{};e->render_ports(main,cue,nullptr,0);ui->poll();ui->project_loaded();
 assert(update::save_update_snapshot(fs,*p,*e));auto saved=std::make_unique<ProjectState>();assert(read_project_file(fs,update::kUpdateSnapshot,*saved) && saved->tempo==137);fs.fail_snapshot=true;assert(!update::save_update_snapshot(fs,*p,*e));fs.fail_snapshot=false;
 auto converted=std::make_unique<ProjectState>(*original);converted->tracks[0].instrument=Instrument::FM;converted->patterns[0].tracks[0].length=256;converted->song.enabled=true;converted->song.length=1;converted->song.sections[0].bars=16;assert(save_recoverable(fs,"PROJECTS/Deploy-LongPhrase.mgxs",*converted));converted.reset();
 auto probe=std::make_unique<DeploymentCheck>(*e,*p,*ui,*files,fs);uint64_t now=0;
 for(bool fail:{false,true}){
  fs.fail_twin=fail;Totals totals;
  assert(probe->start(now,[](void* context,const char* name,bool ok){auto& t=*static_cast<Totals*>(context);ok?++t.passed:++t.failed;std::printf("Deploy check %s ok %u\n",name,unsigned(ok));},&totals));assert(!probe->start(now,nullptr,nullptr));
  for(unsigned n=0;n<90000 && probe->busy();++n){now+=2;ui->animate(now);files->service();e->render_ports(main,cue,nullptr,96);ui->poll();probe->service(now);}
  unsigned passed=0;bool ok=false;assert(!probe->busy() && probe->result(passed,ok));std::printf("complete ok=%u expected=%u passed=%u failures=%u\n",unsigned(ok),unsigned(!fail),passed,totals.failed);assert(ok==!fail && bool(totals.failed)==fail);assert(!probe->result(passed,ok));
  assert(p->tempo==original->tempo && p->tracks[0].instrument==original->tracks[0].instrument && p->patterns[0].tracks[0].steps[0].enabled);
  assert(std::string(ui->project_path())=="PROJECTS/Keep.mgxs");char bytes[32]{};unsigned got=0;assert(fs.read("PROJECTS/Recovery.mgxs",0,bytes,32,got) && got==13 && !std::memcmp(bytes,"user recovery",13));assert(fs.read("GROOVE/CURRENT.TXT",0,bytes,32,got) && got==18 && !std::memcmp(bytes,"PROJECTS/Keep.mgxs",18));
  for(const auto& f:std::filesystem::directory_iterator("RECORDINGS"))assert(!f.is_regular_file());
  for(const auto& f:std::filesystem::directory_iterator("GROOVE"))assert(f.path().filename().string().find("Deploy-")!=0);
 }
 probe.reset();files.reset();ui.reset();e.reset();std::filesystem::current_path(previous);std::filesystem::remove_all(root);puts("deployment qualification: live worker flow and injected storage failure restore state; update snapshot never writes user Recovery or CURRENT");
}
