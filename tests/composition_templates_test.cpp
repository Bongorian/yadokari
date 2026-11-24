#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <fstream>
#include <vector>
using namespace groove;
int main(){
 char root[]="/tmp/mk3-templates-XXXXXX";assert(mkdtemp(root));auto old=std::filesystem::current_path();std::filesystem::current_path(root);DesktopFiles fs(true);for(const char* d:{"FACTORY","FACTORY/DEMOS","PROJECTS","GROOVE"})assert(fs.directory(d));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto e=std::make_unique<Engine>();assert(e->stage_project(*p));auto ui=std::make_unique<Performance>(*e,*p);DesktopSampleAssets library;library.files(&fs);auto files=std::make_unique<SessionFiles>(fs,library,*e,*p,*ui);float audio[128]{};auto flush=[&](){e->render(audio,0);ui->poll();};flush();
 ui->request_project(Performance::ProjectAction::New);assert(ui->file_action_pending());assert(files->service());assert(ui->pending_project()==Performance::ProjectAction::New);assert(ui->modal_cancel());
 auto fixture=std::make_unique<ProjectState>();make_empty_project(*fixture);fixture->tempo=137;fixture->tracks[0].instrument=Instrument::VirtualAnalog;fixture->patterns[0].tracks[0].steps[0].enabled=true;assert(save_recoverable(fs,"FACTORY/DEMOS/A-first.mgxs",*fixture));auto master_bytes=[](){std::ifstream f("FACTORY/DEMOS/A-first.mgxs",std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(f),{});};const auto bytes=master_bytes();
 // Hard links give the browser 70 actual Song entries without redundant megabyte writes.
 for(unsigned n=1;n<70;++n){char path[128];std::snprintf(path,sizeof path,"FACTORY/DEMOS/T%02u.mgxs",n);assert(::link("FACTORY/DEMOS/A-first.mgxs",path)==0);}
 assert(fs.write("FACTORY/DEMOS/ignore.csv",0,"x",1,true));make_empty_project(*fixture);fixture->content_kind=ContentKind::Preset;fixture->tracks[0].instrument=Instrument::VirtualAnalog;fixture->tracks[0].preset_identity=builtin_factory_identity(17001);assert(save_recoverable(fs,"FACTORY/DEMOS/not-a-song.mgxs",*fixture));
 ui->request_project(Performance::ProjectAction::New);assert(files->service());assert(ui->browser().new_project_list && ui->browser().folder_total==71 && ui->browser().count==64 && std::string(ui->browser().entries[0].name)=="EMPTY");assert(ui->move_browser(70));assert(files->service());assert(ui->browser().folder_offset==64 && ui->browser().cursor==6 && std::string(ui->browser().entries[6].name)=="T69.mgxs");
 UiRenderer renderer;renderer.draw(*p,*ui);assert(ui->knob_label(1)[0]==0 && ui->soft_label(2)[0]==0);assert(ui->request_file(FileAction::Open));assert(files->service());flush();assert(p->tempo==137 && std::string(ui->project_name())=="Untitled" && !ui->browser().new_project_list);ui->request_save();assert(ui->browser().edit.operation==BrowserOperation::SaveProject);ui->cancel_browser_edit();assert(files->save("PROJECTS/Named.mgxs"));assert(master_bytes()==bytes);
 auto check=std::make_unique<ProjectState>();assert(read_project_file(fs,"FACTORY/DEMOS/A-first.mgxs",*check) && check->tempo==137); // Save left the master bytes untouched.
 ui->request_project(Performance::ProjectAction::New);assert(files->service());assert(ui->soft_key(1));assert(!ui->browser().new_project_list && ui->mode()==UiMode::Project);
 // Browser discovery and open use the canonical project name throughout.
 assert(fs.directory("Orphans"));
 assert(fs.move("FACTORY/DEMOS/A-first.mgxs","Orphans/Take.mgxs.tmp"));
 assert(fs.write("Orphans/Bad.mgxs.tmp",0,"MGXC",4,true));
 auto& b=ui->browser();assert(copy_text(b.directory,sizeof b.directory,"Orphans"));
 ui->set_mode(UiMode::Browser);files->refresh();
 assert(b.count==1 && std::string(b.entries[0].name)=="Take.mgxs" && b.entries[0].content_kind==1);
 p->tempo=111;assert(ui->soft_key(0) && files->service());flush();
 assert(p->tempo==137 && fs.exists("Orphans/Take.mgxs.tmp") && !fs.exists("Orphans/Take.mgxs"));
 assert(files->save("Orphans/Take.mgxs"));flush();
 assert(fs.exists("Orphans/Take.mgxs") && !fs.exists("Orphans/Take.mgxs.tmp"));
 assert(::link("Orphans/Take.mgxs","Orphans/Take.mgxs.tmp")==0);
 assert(copy_text(b.directory,sizeof b.directory,"Orphans"));files->refresh();
 assert(b.count==1 && !b.entries[0].orphan_song && std::string(b.entries[0].name)=="Take.mgxs");
 // A maximum-length Save As stem still fits with both suffixes during discovery.
 std::string long_name=std::string(95,'L')+".mgxs";
 std::string long_temp="Orphans/"+long_name+".tmp";
 assert(::link("Orphans/Take.mgxs",long_temp.c_str())==0);files->refresh();
 assert(b.count==2 && b.entries[0].orphan_song && long_name==b.entries[0].name);
 // A portable groove goes through the real Extract, Save, Files/Open and Paste controls.
 ui->set_mode(UiMode::Generate);assert(ui->soft_key(2));auto t=track_at(*p,p->active_pattern,ui->selected_track());
 t.steps[0].enabled=true;t.steps[0].microtiming=-21;t.steps[0].velocity=115;
 assert(ui->soft_key(4));ui->set_shift(true);assert(ui->soft_key(4));ui->set_shift(false);assert(files->service());
 assert(fs.exists("GROOVES/Groove-001.grv"));assert(ui->request_file(FileAction::SaveGroove) && files->service());
 assert(fs.exists("GROOVES/Groove-002.grv"));GrooveTemplate exported;assert(read_groove_template(fs,"GROOVES/Groove-001.grv",exported));
 assert(exported.timing[0]==-21 && exported.velocity[0]==15);
 t.steps[0].microtiming=31;assert(ui->soft_key(4));
 ui->set_mode(UiMode::Browser);assert(copy_text(b.directory,sizeof b.directory,"GROOVES"));files->refresh();b.cursor=0;
 assert(ui->soft_enabled(0) && !ui->soft_enabled(2) && !ui->soft_enabled(3));
 assert(ui->soft_key(0) && files->service() && ui->mode()==UiMode::Generate && ui->generate_page()==2);
 assert(ui->soft_key(5));flush();assert(t.groove.timing[0]==-21 && t.groove.velocity[0]==15 && t.groove.amount==100);
 assert(fs.write("GROOVES/Bad.grv",0,"YGR1",4,true) && !read_groove_template(fs,"GROOVES/Bad.grv",exported));
 files.reset();ui.reset();e.reset();std::filesystem::current_path(old);std::filesystem::remove_all(root);puts("composition templates PASS: zero-template fallback; 70-file paging; ContentKind filter; unnamed Save As; factory source kept; cancel");
}
