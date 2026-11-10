#include "factory_content.h"
#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <unistd.h>
int main(int argc,char** argv){using namespace groove;assert(argc==2 && chdir(argv[1])==0);
 std::setvbuf(stdout,nullptr,_IONBF,0);
 DesktopFiles fs;DesktopSampleAssets assets;assets.files(&fs);auto p=std::make_unique<ProjectState>();auto e=std::make_unique<Engine>();Performance ui(*e,*p);SessionFiles files(fs,assets,*e,*p,ui);float audio[256]{};
 // Empty is silent in both sequenced/live paths and survives the file codec.
 make_empty_project(*p);assert(e->stage_project(*p));assert(e->submit({CommandType::Trigger,0,0,127}));e->render(audio,128);for(float v:audio)assert(v==0);
 // Isolate every pad of each kit through the real sample loader and DSP path.
 for(unsigned kit=0;kit<3;++kit){char path[160];std::snprintf(path,sizeof path,"FACTORY/KITS/%s.mgxk",factory_kit_name(kit));assert(files.load(path));e->render(audio,0);ui.poll();
  for(unsigned pad=0;pad<6;++pad){Command hit{CommandType::Trigger,0,0,110};hit.parameter=1;hit.scalar=float(60+pad);assert(e->submit(hit));float peak=0;for(unsigned n=0;n<24000;n+=128){e->render(audio,128);ui.poll();for(float v:audio){assert(std::isfinite(v));peak=std::fmax(peak,std::abs(v));}}assert(peak>.008f);std::printf("%s %s: isolated peak %.4f\n",factory_kit_name(kit),factory_drum_name(pad),peak);}
 }
 assert(files.load(kFactoryStartupProject));e->render(audio,0);ui.poll();const unsigned tempo=p->tempo;p->tempo=99;
 assert(!files.save(kFactoryStartupProject));ui.request_save();assert(ui.browser_editing() && data_equal(ui.browser().edit.source,kUserProject));ui.cancel_browser_edit();
 assert(files.save("PROJECTS/Edited.mgx"));assert(data_equal(ui.project_path(),"PROJECTS/Edited.mgx"));assert(files.load(kFactoryStartupProject));e->render(audio,0);ui.poll();assert(p->tempo==tempo);
 assert(files.load("PROJECTS/Edited.mgx"));e->render(audio,0);ui.poll();assert(p->tempo==99);
 // Internal kind, not extension, selects preset load into the current track.
 assert(data_content_kind(fs,"FACTORY/DEBUG/Test-Tone.mgxp")==3);assert(files.load_preset("FACTORY/DEBUG/Test-Tone.mgxp"));e->render(audio,0);ui.poll();assert(p->tempo==99);
 std::puts("factory workflow: protected original, Save As, reload/reset, typed preset passed");
}
