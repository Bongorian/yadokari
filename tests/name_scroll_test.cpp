#include "software_canvas.h"
#include "project_codec.h"
#include "update/view.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
using namespace groove;
struct Storage: update::Storage {
 bool read_journal(unsigned,uint8_t*)override{return false;}bool write_journal(unsigned,const uint8_t*)override{return false;}
 bool open_image(unsigned,bool)override{return false;}bool write_image(const uint8_t*,unsigned)override{return false;}
 bool read_image(uint8_t*,unsigned,unsigned&)override{return false;}bool close_image()override{return false;}
};
struct Reboot:update::Rebooter{bool supported()const override{return false;}bool restart(unsigned,bool)override{return false;}};
int main(){
 auto c=std::make_unique<SoftwareCanvas>(),expected=std::make_unique<SoftwareCanvas>();const char* name="A very long synthesizer and sample name.wav";const int width=96,full=ui::text_width(name);const uint32_t sentinel=0xff123456;
 auto clear=[&](){c->fill(0,0,0,480,272,sentinel);expected->fill(0,0,0,480,272,sentinel);};
 auto reference=[&](int offset){ui::raster_text([&](int x,int y,unsigned alpha){if(x>=30 && x<30+width)expected->blend(0,x,y,0xffffffff,alpha);},30-offset,90,name,ui::FontRole::Body,full+4);};
 clear();c->text_clock(1000,true);c->scrolling_label(0,30,90,name,0xffffffff,ui::FontRole::Body,width);reference(0);assert(!std::memcmp(c->pixels[0],expected->pixels[0],sizeof c->pixels[0]));
 c->text_clock(2000);assert(!c->scrolling_due(76,207));c->text_clock(3200);assert(c->scrolling_due(76,207));clear();c->scrolling_label(0,30,90,name,0xffffffff,ui::FontRole::Body,width);reference(28);assert(!std::memcmp(c->pixels[0],expected->pixels[0],sizeof c->pixels[0]));
 unsigned travel=unsigned((uint64_t(full-width)*1000+27)/28);c->text_clock(1000+1200+travel+500);clear();c->scrolling_label(0,30,90,name,0xffffffff,ui::FontRole::Body,width);reference(full-width);assert(!std::memcmp(c->pixels[0],expected->pixels[0],sizeof c->pixels[0]));
 c->scrolling_label(0,30,90,"Short",0xffffffff,ui::FontRole::Body,width);c->text_clock(99999);assert(!c->scrolling_due(76,207));
 Storage storage;Reboot reboot;uint8_t key[32]{};update::Manager manager(storage,reboot,key);
 auto project=std::make_unique<ProjectState>();
 for(unsigned i=0;i<ui::Theme::kCount;++i){project->ui.theme=uint8_t(i);assert(valid_project(*project));c->theme=ui::Theme::by_index(i);update::draw_view(*c,manager,"ETHERNET",2,false,408);assert(c->pixels[0][271*480]==c->theme.background);assert(c->pixels[0][16*480+10]==c->theme.surface);assert(c->pixels[1][271*480]==c->theme.background);}
 project->ui.theme=ui::Theme::kCount-1;ChunkBuffer encoded;assert(encode_project_buffer(encoded,*project));ChunkSource source{encoded.data,encoded.size,0};auto restored=std::make_unique<ProjectState>();assert(decode_project(source,*restored));assert(restored->ui.theme==0 && track_at(*restored,0,0).instrument==Instrument::Empty);
 project->ui.theme=255;assert(valid_project(*project));
 std::puts("name scroll: dwell/travel/end/clipping/short reset; update view: all themes passed");
}
