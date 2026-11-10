#include "soundfont.h"
#include "project_save_slot.h"
#include "../../platform/desktop/file_access.h"
#include <chrono>
#include <cstdio>
#include <memory>
using namespace groove;
static uint64_t now(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct CountFiles:DesktopFiles{uint64_t reads=0,bytes=0;bool read(const char* p,uint64_t o,void* d,unsigned n,unsigned& got)override{++reads;bool ok=DesktopFiles::read(p,o,d,n,got);bytes+=got;return ok;}};
int main(){CountFiles fs;SoundFontCache fonts;WavView view;uint64_t a=now();bool ok=fonts.load(fs,"/usr/share/sounds/sf2/FluidR3_GM.sf2",view);uint64_t us=now()-a;if(!ok)return 2;unsigned bank=0,program=0;if(!soundfont_first(view.soundfont,bank,program))return 3;
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto slot=std::make_unique<ProjectEncodeSlot>();a=now();if(!slot->submit_copy(*p,"PROJECTS/Optimization.mgxs"))return 4;uint64_t copy=now()-a;if(!slot->process_one())return 5;ProjectEncodeSlot::Result r;if(!slot->collect(r)||!r.ok)return 6;delete[] r.bytes;
 printf("{\"sf2\":\"FluidR3_GM.sf2\",\"size\":%llu,\"load_us\":%llu,\"read_calls\":%llu,\"read_bytes\":%llu,\"resident_bytes\":%zu,\"bank\":%u,\"program\":%u,\"snapshot_us\":%llu,\"snapshot_bytes\":%zu,\"encoded_bytes\":%u,\"clock\":\"host ASan/UBSan steady clock; not Pi deadline\"}\n",(unsigned long long)fs.size("/usr/share/sounds/sf2/FluidR3_GM.sf2"),(unsigned long long)us,(unsigned long long)fs.reads,(unsigned long long)fs.bytes,soundfont_resident_bytes(),bank,program,(unsigned long long)copy,sizeof(ProjectState),r.size);return 0;}
