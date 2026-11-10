#include "session_files.h"
#include "ui_renderer.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <vector>
#include <unistd.h>
using namespace groove;
struct Files:DesktopFiles {bool managed=true;bool browser_file_management_allowed()const override{return managed;}bool fail_move=false;bool move(const char* a,const char* b)override{return !fail_move && DesktopFiles::move(a,b);}};
struct Library:ISampleLibrary {const char* cached=nullptr;uint8_t sample[2]{};bool load(const char*,WavView&)override{return false;}bool prepare(const ProjectState&,WavView*)override{return false;}WavView lookup(const char* path)const override{WavView v{};if(cached && browser_path_equal(path,cached)){v.data=sample;v.frames=1;}return v;}};
struct Sink {std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
int main(){
 auto previous=std::filesystem::current_path();char root[]="/tmp/mk3-browser-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
 {
 Files fs;Library library;auto p=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>();Performance ui(*engine,*p);SessionFiles files(fs,library,*engine,*p,ui);
 auto put=[&](const char* path){assert(fs.write(path,0,"test",4,true));};
 auto select=[&](const char* name){files.refresh();auto& b=ui.browser();for(unsigned i=0;i<b.count;++i)if(!std::strcmp(b.entries[i].name,name)){b.cursor=i;return;}assert(false);};
 auto apply=[&](BrowserOperation op,const char* name=nullptr){assert(ui.begin_browser_edit(op));if(name)assert(copy_text(ui.browser().edit.name,96,name));assert(ui.soft_key(0));return files.service();};
 ui.browser().directory[0]=0;ui.set_mode(UiMode::Browser);assert(files.service());
 assert(apply(BrowserOperation::NewFolder,"Takes") && fs.exists("TAKES"));
 put("loose.wav");select("loose.wav");fs.managed=false;assert(!apply(BrowserOperation::Delete) && fs.exists("loose.wav"));fs.managed=true;assert(apply(BrowserOperation::Rename,"renamed.wav"));assert(!fs.exists("loose.wav") && fs.exists("renamed.wav"));
 select("renamed.wav");fs.fail_move=true;assert(!apply(BrowserOperation::Rename,"failed.wav"));assert(fs.exists("renamed.wav") && !fs.exists("failed.wav"));fs.fail_move=false;
 put("collision.wav");select("renamed.wav");assert(!apply(BrowserOperation::Rename,"collision.wav") && fs.size("collision.wav")==4);
 select("renamed.wav");assert(ui.begin_browser_edit(BrowserOperation::Rename));auto mode=ui.mode();unsigned track=ui.selected_track();assert(ui.control(7,0x20));assert(ui.mode()==mode && ui.selected_track()==track);assert(ui.knob(1,1000));assert(ui.soft_label(0)==std::string("APPLY"));assert(ui.soft_key(1));assert(!ui.browser_editing() && ui.axes().modal==ModalKind::None);
 // A stopped, otherwise static Browser must repaint when a modal opens,
 // text/cursor changes, and when an operation updates the directory/status.
 auto renderer=std::make_unique<UiRenderer>();ui.animate(1000);renderer->draw(*p,ui);ui.animate(2000);renderer->draw(*p,ui);
 renderer->pixels[0][100*480+20]=0xffdebeef;assert(ui.begin_browser_edit(BrowserOperation::Rename));ui.animate(2200);renderer->draw(*p,ui);assert(renderer->pixels[0][100*480+20]!=0xffdebeef);
 renderer->pixels[0][100*480+20]=0xffdebeef;assert(ui.encoder(1));ui.animate(2400);renderer->draw(*p,ui);assert(renderer->pixels[0][100*480+20]!=0xffdebeef);assert(ui.soft_key(1));
 // Cached views and both current and on-disk project references protect WAVs.
 library.cached="renamed.wav";select("renamed.wav");assert(!apply(BrowserOperation::Delete) && fs.exists("renamed.wav"));library.cached=nullptr;
 assert(copy_text(p->assets[0].path,96,"renamed.wav"));select("renamed.wav");assert(!apply(BrowserOperation::Delete));p->assets[0].path[0]=0;
 auto stored=std::make_unique<ProjectState>();assert(copy_text(stored->assets[0].path,96,"renamed.wav"));Sink sink;assert(encode_project(sink,*stored));for(const char* name:{"song.mgx","song.MGXS","song.mgxk","song.mgxp"}){
 assert(fs.write(name,0,sink.bytes.data(),unsigned(sink.bytes.size()),true));
 assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Used);
 select("renamed.wav");assert(!apply(BrowserOperation::Delete));assert(fs.remove(name));
 }
 // A valid unrelated container is not itself a reason to refuse a loose file.
 stored->assets[0].path[0]=0;Sink unrelated;assert(encode_project(unrelated,*stored));assert(fs.write("unrelated.mgx",0,unrelated.bytes.data(),unsigned(unrelated.bytes.size()),true));
 assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Clear);
 unrelated.bytes.back()^=1;assert(fs.write("unrelated.mgx",0,unrelated.bytes.data(),unsigned(unrelated.bytes.size()),true));
 assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Unknown);assert(fs.remove("unrelated.mgx"));
 // R4: real factory-sized directories, >512 files and >256 folders must
 // remain fully scanned, including references/corruption inside FACTORY.
 assert(fs.directory("FACTORY") && fs.directory("FACTORY/PRESETS") && fs.directory("FACTORY/PRESETS/LIBRARY") && fs.directory("FACTORY/EXTRA") && fs.directory("FACTORY/FOLDERS"));
 make_empty_project(*stored);stored->content_kind=ContentKind::Preset;stored->tracks[0].instrument=Instrument::PolySaw;assert(parse_preset_guid("11111111111111111111111111111111",stored->tracks[0].preset_identity.guid));stored->tracks[0].preset_identity.revision=1;Sink metadata;assert(encode_project(metadata,*stored));
 for(unsigned i=0;i<200;++i){char name[160];std::snprintf(name,sizeof name,"FACTORY/PRESETS/LIBRARY/%03u.mgxp",i);assert(fs.write(name,0,metadata.bytes.data(),unsigned(metadata.bytes.size()),true));}
 for(unsigned i=0;i<400;++i){char name[160];std::snprintf(name,sizeof name,"FACTORY/EXTRA/%03u.bin",i);put(name);}
 for(unsigned i=0;i<270;++i){char name[160];std::snprintf(name,sizeof name,"FACTORY/FOLDERS/%03u",i);assert(fs.directory(name));}
 assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Clear);
 assert(copy_text(stored->assets[0].path,96,"../../../renamed.wav"));Sink referenced;assert(encode_project(referenced,*stored));assert(fs.write("FACTORY/PRESETS/LIBRARY/Reference.mgxp",0,referenced.bytes.data(),unsigned(referenced.bytes.size()),true));
 assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Used);assert(fs.remove("FACTORY/PRESETS/LIBRARY/Reference.mgxp"));put("FACTORY/PRESETS/LIBRARY/Bad.mgxp");assert(BrowserFileGuard::check(fs,library,*p,"renamed.wav")==FileReferences::Unknown);std::filesystem::remove_all("FACTORY");
 // A corrupt/unrecognized project makes reference knowledge incomplete.
 put("broken.mgx");select("renamed.wav");assert(!apply(BrowserOperation::Delete));assert(fs.remove("broken.mgx"));
 assert(symlink("/tmp","outside")==0);select("renamed.wav");assert(!apply(BrowserOperation::Delete));assert(unlink("outside")==0);
 select("renamed.wav");assert(apply(BrowserOperation::Delete) && !fs.exists("renamed.wav"));
 assert(fs.directory("Nonempty"));put("Nonempty/a.wav");select("Nonempty");assert(!apply(BrowserOperation::Delete));
 select("TAKES");assert(apply(BrowserOperation::Rename,"Renamed Takes") && fs.exists("RENAMED TAKES"));assert(!apply(BrowserOperation::NewFolder,"renamed takes"));select("RENAMED TAKES");assert(apply(BrowserOperation::Delete) && !fs.exists("RENAMED TAKES"));
 assert(!browser_name_valid("../escape") && !browser_name_valid("x/") && !browser_name_valid("file. ") && browser_name_valid("My-Take_01.wav"));
 }
 std::filesystem::current_path(previous);std::filesystem::remove_all(root);std::puts("Browser management: names, modal ownership, rename failure, collision, cache/project/unknown references, symlinks and empty folders passed");
}
