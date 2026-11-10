#include "soundfont_fixture.h"
#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <cassert>
#include <unistd.h>
using namespace groove;
int main(){
 auto cwd=std::filesystem::current_path();char root[]="/tmp/mk3-layer-XXXXXX";assert(mkdtemp(root) && chdir(root)==0);DesktopFiles fs;assert(fs.write("debug.sf2",0,kSoundFontFixture,sizeof kSoundFontFixture,true));DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);SessionFiles files(fs,library,*e,*p,*ui);float audio[128]{};
 assert(files.load_soundfont("debug.sf2"));e->render(audio,0);auto t=track_at(*p,0,0);auto& layer=t.layer;layer.count=2;snapshot_leaf(layer.parts[0].sound,t);layer.parts[1].sound.instrument=Instrument::Additive;t.instrument=Instrument::Layer;assert(files.save_preset("mine.mgxp"));assert(files.collect("Export","mine.mgxp"));assert(files.load_preset("Export/PRESETS/preset.mgxp"));e->render(audio,0);assert(t.instrument==Instrument::Layer && t.layer.count==2);assert(p->tracks[0].preset_slot<kPresetSlots && p->sounds[p->tracks[0].preset_slot].identity.assigned());
 // The portable copy survives removal of the original; failed original load keeps the current sound.
 assert(fs.remove("debug.sf2"));assert(files.load_preset("Export/PRESETS/preset.mgxp"));e->render(audio,0);assert(files.load_preset("mine.mgxp"));assert(t.instrument==Instrument::Layer && t.layer.parts[0].sound.instrument==Instrument::SoundFont);
 Command on{CommandType::MidiInput,0,60,0x90};on.scalar=100;assert(e->submit(on));double energy=0;for(unsigned n=0;n<20;++n){e->render(audio,64);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.01 && e->layer_note_voices(0)==2);
 auto bad=std::make_unique<ProjectState>();assert(read_project_file(fs,"Export/PRESETS/preset.mgxp",*bad));bad->tracks[0].layer.parts[0].sound.soundfont.values[1]=127;assert(save_recoverable(fs,"Export/bad.mgxp",*bad));assert(!files.load_preset("Export/bad.mgxp"));assert(t.layer.parts[0].sound.soundfont.values[1]==0);
 std::filesystem::current_path(cwd);std::filesystem::remove_all(root);puts("Layer files: mixed SF2/additive save, Collect, portable remount, missing asset/program refusal, empty pool passed");
}
