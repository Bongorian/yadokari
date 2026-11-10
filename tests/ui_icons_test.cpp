#include "ui_renderer.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <cstdio>
using namespace groove;
int main(){
 assert(!std::strcmp(ui::font_family,"Barlow Semi Condensed"));
 // Browser startup advice stays in Browser; actual file results still expire.
 ui::FileToast toast;toast.observe("Open a folder or WAV",0);assert(!toast.active(0));
 toast.observe("Open failed",10);assert(toast.active(10));
 toast.observe("Sample saved",100);assert(toast.active(2099) && !toast.active(2100));
 auto canvas=std::make_unique<SoftwareCanvas>();std::set<std::string> outlines;
 for(unsigned theme=0;theme<ui::Theme::kCount;++theme)for(unsigned id=1;id<ui::icons::Count;++id){
  canvas->fill(0,0,0,480,272,0xff000000);canvas->icon(0,10,10,id,0xffffffff);std::string mask;
  for(int y=0;y<30;++y)for(int x=0;x<30;++x){bool on=canvas->pixels[0][y*480+x]==0xffffffff;if(on)assert(x>=10 && x<24 && y>=10 && y<24);if(x>=10 && x<24 && y>=10 && y<24)mask+=on?'1':'0';}
  assert(mask.find('1')!=std::string::npos);if(!theme)assert(outlines.insert(mask).second);else assert(outlines.count(mask));
  canvas->icon(0,40,10,id,0xffffffff,true);if(id!=ui::icons::Play && id!=ui::icons::Stop && id!=ui::icons::Record)for(int y=0;y<14;++y)for(int x=0;x<14;++x)assert(canvas->pixels[0][(10+y)*480+10+x]==canvas->pixels[0][(10+y)*480+40+x]);
 }
 std::set<unsigned> categories;for(unsigned page=0;page<7;++page){auto d=settings_button(page%3,false,page);assert(d.selected && d.enabled && d.icon);assert(categories.insert(d.icon).second);}
 assert(common_top_control(4,true,false,false,false,false).icon==ui::icons::Source);
 assert(synth_button_control(7,{Instrument::VirtualAnalog,true}).icon==ui::icons::Bake);
 assert(project_button(0,false,false,false,0,0).icon==ui::icons::Store);
 assert(!std::strcmp(engine_family_control(Instrument::SoundFont).label,"SAMPLER"));
 std::set<unsigned> strips;for(unsigned i=0;i<4;++i){HeaderStatusContext c;c.strip=StripMode(i);assert(strips.insert(header_status(5,c).icon).second);}
 for(unsigned id=0;id<instrument_count();++id)if(Instrument(id)!=Instrument::Empty && Instrument(id)!=Instrument::Unknown)assert(engine_family_control(Instrument(id)).icon);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->ui.motion=0;auto e=std::make_unique<Engine>();auto u=std::make_unique<Performance>(*e,*p);auto view=std::make_unique<UiRenderer>();u->set_mode(UiMode::Routing);u->animate(2000);assert(!u->file_notice_text()[0]);u->notify(UiNotice::None);view->draw(*p,*u);
 auto before=std::make_unique<uint32_t[]>(2*480*272);std::memcpy(before.get(),view->pixels,sizeof view->pixels);
 u->dispatch({ControllerInput::Type::KnobTouch,0,1});assert(u->knob_overlay_mask()==1);view->draw(*p,*u);
 // Only the active cell may change in the body/footer. No strip, divider or duplicated names.
 for(unsigned side=0;side<2;++side)for(int y=64;y<272;++y)for(int x=0;x<480;++x)if(side || x<4 || x>=116 || !((y>=ui::Layout::overlay_y && y<ui::Layout::overlay_y+ui::Layout::overlay_h) || y>=270))assert(before[side*480*272+y*480+x]==view->pixels[side][y*480+x]);
 // Both captures use an idle body/footer; independent header marquee is excluded.
 u->animate(2100);u->dispatch({ControllerInput::Type::KnobTouch,5,1});assert(u->knob_overlay_mask()==33);
 u->dispatch({ControllerInput::Type::KnobTouch,0,0});u->animate(3299);assert(u->knob_overlay_mask()==33);u->animate(3300);assert(u->knob_overlay_mask()==32);
 u->dispatch({ControllerInput::Type::KnobTouch,5,0});u->animate(4499);assert(u->knob_overlay_mask()==32);u->animate(4500);assert(!u->knob_overlay_mask());view->draw(*p,*u);for(unsigned side=0;side<2;++side)assert(!std::memcmp(before.get()+side*480*272+64*480,view->pixels[side]+64*480,(272-64)*480*sizeof(uint32_t)));
 u->knob(1,1000);assert(u->knob_overlay_mask()==2);u->animate(6000);assert(!u->knob_overlay_mask());
 u->set_mode(UiMode::Diagnostics);for(unsigned i=0;i<8;++i)if(!u->knob_label(i)[0]){u->dispatch({ControllerInput::Type::KnobTouch,uint8_t(i),1});assert(!(u->knob_overlay_mask()&(1u<<i)));}
 std::puts("Wave7: native icon bounds/unique monochrome shapes, fill rules, descriptor categories/families, independent overlay cells and expiry passed");
}
