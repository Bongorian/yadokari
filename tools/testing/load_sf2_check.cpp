#include "../../firmware/load_sf2_generator.h"
#include "../../platform/desktop/file_access.h"
#include "../../src/soundfont.h"
#include <memory>
#include <cassert>
#include <filesystem>
#include <cstdio>
using namespace groove;
int main(){std::filesystem::create_directories("build/final-test/sf2");std::filesystem::current_path("build/final-test/sf2");DesktopFiles fs;auto gen=std::make_unique<LoadSf2Generator>();while(!gen->service(fs))assert(!gen->failed());assert(fs.size("GROOVE/LOADTEST.SF2")>33554432);assert(soundfont_file_header(fs,"GROOVE/LOADTEST.SF2"));SoundFontCache cache;WavView view;assert(cache.load(fs,"GROOVE/LOADTEST.SF2",view));unsigned bank,program;assert(soundfont_first(view.soundfont,bank,program)&&bank==0&&program==0);for(unsigned i=0;i<4;++i)assert(soundfont_name(view.soundfont,0,i));SoundFontVoice voice;voice.init(0,48000);SoundFontState state;voice.trigger(view.soundfont,261.6256f,100,state);float peak=0;for(unsigned i=0;i<48000;++i){float l,r;voice.next(261.6256f,true,l,r);assert(std::isfinite(l)&&std::isfinite(r));peak=fmaxf(peak,fmaxf(fabsf(l),fabsf(r)));}assert(peak>.001f);printf("Large SF2: %llu bytes, four programs, 16 referenced zones, finite sounding voice peak %f\n",(unsigned long long)fs.size("GROOVE/LOADTEST.SF2"),peak);}
