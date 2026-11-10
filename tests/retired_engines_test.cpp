#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
using namespace groove;
int main(){assert(instrument_count()==27);char temp[]="/tmp/mgx-unsupported-engine-XXXXXX";assert(mkdtemp(temp) && !chdir(temp));DesktopFiles fs;assert(fs.directory("PROJECTS"));auto current=std::make_unique<ProjectState>(),unknown=std::make_unique<ProjectState>(),decoded=std::make_unique<ProjectState>();make_empty_project(*current);current->tracks[0].instrument=Instrument::VirtualAnalog;auto e=std::make_unique<Engine>();assert(e->stage_project(*current));auto ui=std::make_unique<Performance>(*e,*current);DesktopSampleAssets assets;assets.files(&fs);ProjectDecodeSlot worker;auto files=std::make_unique<SessionFiles>(fs,assets,*e,*current,*ui);float samples[48]{};e->render(samples,0);const unsigned generation=e->staged_generation();
 for(const char* name:{"kick","noise","snare","hihat"}){const std::string guid=std::string("mk3.")+name;Instrument unresolved=Instrument::Empty;assert(!engine_from_guid(guid.data(),guid.size(),unresolved));for(unsigned kind=1;kind<=3;kind+=2){make_empty_project(*unknown);unknown->content_kind=ContentKind(kind);unknown->tracks[0].instrument=Instrument::Unknown;if(kind==3)unknown->tracks[0].preset_identity=builtin_factory_identity(0x900);PreservedOwner store;assert(store);ChunkBuffer body;ChunkWriter<ChunkBuffer> writer(body);assert(writer.u8(guid.size()) && writer.block(reinterpret_cast<const uint8_t*>(guid.data()),guid.size()) && writer.u16(1) && store->tracks[0].engn.set(body.data,body.size));char path[96];std::snprintf(path,sizeof path,"PROJECTS/%s.%s",name,kind==1?"mgxs":"mgxp");{FileWriter out(fs,path);assert(encode_project(out,*unknown,store.get()) && out.finish());}assert(read_project_file(fs,path,*decoded) && decoded->tracks[0].instrument==Instrument::Unknown);ui->browser().status[0]=0;assert(!files->load(path) && __builtin_strstr(ui->browser().status,"Unsupported engine") && current->tracks[0].instrument==Instrument::VirtualAnalog && e->staged_generation()==generation);
 if(kind==3){ui->browser().status[0]=0;assert(!files->audition_preset(path) && __builtin_strstr(ui->browser().status,"Unsupported engine"));
 }}}
 make_empty_project(*unknown);assert(unknown->unavailable_targets.intern(engine_guid(Instrument::VirtualAnalog),__builtin_strlen(engine_guid(Instrument::VirtualAnalog)))==0);assert(files->admit_content_engines(*unknown));
 files->set_project_load_worker(&worker);for(const char* name:{"kick","noise","snare","hihat"}){char path[96];std::snprintf(path,sizeof path,"PROJECTS/%s.mgxp",name);assert(files->start_kit_load(path));for(unsigned n=0;n<1024 && files->kit_load_busy();++n){worker.process_one();files->service();e->render(samples,0);ui->poll();}bool ok=true,probe=true;assert(!files->kit_load_busy() && files->take_kit_load_result(ok,probe) && !ok && __builtin_strstr(ui->browser().status,"Unsupported engine"));}
 auto blank=std::make_unique<ProjectState>();make_empty_project(*blank);assert(e->stage_project(*blank));assert(e->submit({CommandType::Trigger,0,0,127}));e->render(samples,24);for(float value:samples)assert(value==0);assert(!ui->browser().previewing);
 std::puts("Unsupported engines: four retired identities unresolved, Song/Preset/async/preview refused transactionally, no remap, Empty silent;27 registrations");
}
