#include "page_descriptors.h"
#include "ui_renderer.h"
#include "ui/text_alignment.h"
#include "future_factory_presets.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
using namespace groove;
static uint64_t roi(const ui::Widgets& w,unsigned side){uint64_t h=1469598103934665603ull;for(int y=80;y<244;++y)for(int x=20;x<460;++x)h=(h^w.pixels[side][y*480+x])*1099511628211ull;return h;}
int main(){
 for(unsigned i=0;i<instrument_count();++i){const auto* d=engine_descriptor(i);assert(d && d->visual.caption && unsigned(d->visual.element)<=unsigned(VisualElement::Response));if(d->page.hero!=HeroVisual::None && d->page.hero!=HeroVisual::MidiInfo)assert(d->visual.element!=VisualElement::None);}
 for(unsigned i=0;i<kFxCount;++i){const auto* d=fx_descriptor(i);assert(d && d->visual.caption && d->visual.element!=VisualElement::None);}assert(!fx_descriptor(kFxCount));
 ui::VisualMotion motion;VisualData d;d.available=d.live=true;d.count=2;d.values[1]=1;d.phase=d.position=.1f;
 motion.update(1,1,0,d,true);d.phase=d.position=.9f;motion.update(1,2,10,d,true);auto middle=motion.update(1,2,35,d,true);assert(fabsf(middle.phase-.5f)<.001f);motion.update(1,2,60,d,true);assert(!motion.active(60));motion.update(1,3,70,d,true);assert(!motion.active(70));
 motion.update(2,1,80,d,true,true);d.phase=.1f;motion.update(2,2,90,d,true,true);middle=motion.update(2,2,115,d,true,true);assert(middle.phase<.001f || middle.phase>.999f);
 auto widgets=std::make_unique<ui::Widgets>();for(unsigned theme=0;theme<ui::Theme::kCount;++theme){widgets->theme=ui::Theme::by_index(theme);for(unsigned kind=1;kind<=6;++kind){VisualDeclaration decl;decl.element=VisualElement(kind);VisualData value;value.available=true;value.count=128;value.recorded=.6f;value.layers=2;value.point=true;value.phase=value.position=.25f;value.label="REAL DATA";for(unsigned i=0;i<128;++i){value.values[i]=.4f*sinf(float(i)*.12f);value.low[i]=-value.values[i];}widgets->fill(0,0,0,480,272,widgets->theme.background);widgets->visual_element(0,24,82,430,118,decl,value,widgets->theme.accent);auto first=roi(*widgets,0);widgets->fill(0,0,0,480,272,widgets->theme.background);widgets->visual_element(0,24,82,430,118,decl,value,widgets->theme.accent);assert(first==roi(*widgets,0));}}

 // Geometry contracts are measured on the final native pixel placements.
 unsigned observed=0;widgets->layout_band(76,131,64,194);widgets->text_observer=[](const SoftwareCanvas::TextPlacement& p,void* ctx){assert(!ui::alignment_errors(p));++*static_cast<unsigned*>(ctx);};widgets->text_observer_context=&observed;
 widgets->value_row(0,24,104,206,23,{"FOCUS","1.0 harm",0});widgets->value_row(0,248,104,206,23,{"WIDTH","16.0 harm",1});widgets->list_row(0,24,130,430,28,{"Sample name","PCM / 48K","READY"});widgets->centered_label(0,24,170,430,18,"CELL",widgets->theme.text);widgets->section(0,24,82,430,"HEADING");assert(observed>=8);widgets->text_observer=nullptr;widgets->layout_reset();
 auto models=std::make_unique<ProjectState>();make_empty_project(*models);auto mt=track_at(*models,0,0);mt.instrument=Instrument::PlaitsMacro;auto recipe=ui::control_recipe(mt,false);assert(recipe.presentation==VisualElement::Bars && recipe.count>=3 && !recipe.live);for(unsigned i=0;i<recipe.count;++i)assert(recipe.names[i] && recipe.values[i]>=0 && recipe.values[i]<=1);
 auto p=std::make_unique<ProjectState>();assert(make_future_factory_preset(*p,11));p->ui.motion=0;auto e=std::make_unique<Engine>();auto u=std::make_unique<Performance>(*e,*p);auto view=std::make_unique<UiRenderer>();assert(e->stage_project(*p));u->project_loaded();u->set_mode(UiMode::Synth);
 uint64_t samples=0;float block[256]{};auto advance=[&](unsigned frames){while(frames){unsigned n=frames>128?128:frames;e->render(block,n);samples+=n;u->animate(samples/48);u->poll();frames-=n;}};
 bool focus_seen=false,width_seen=false;struct Formatting {bool* focus;bool* width;} formatting{&focus_seen,&width_seen};view->text_observer=[](const SoftwareCanvas::TextPlacement& value,void* context){auto* flags=static_cast<Formatting*>(context);if(!std::strcmp(value.text,"1.0 harm"))*flags->focus=true;if(!std::strcmp(value.text,"16.0 harm"))*flags->width=true;assert(!std::strstr(value.text,"1600.0 harm"));};view->text_observer_context=&formatting;
 advance(7200);view->draw(*p,*u);assert(focus_seen && width_seen);auto idle=roi(*view,1);auto selected_pcm=ui::pcm_visual(*u,VisualSource::CapturePcm);assert(!std::strcmp(selected_pcm.label,"TRACK POST FADER / LIVE PCM"));auto relative=ui::engine_visual_data(engine_descriptor(unsigned(Instrument::Additive))->visual,track_at(*p,0,0),*u);float max_recipe=0;for(unsigned i=0;i<16;++i)max_recipe=fmaxf(max_recipe,relative.values[i]);assert(fabsf(max_recipe-1)<.0001f);advance(7200);view->draw(*p,*u);assert(idle==roi(*view,1));
 Command note{CommandType::MidiInput,0,60,0x90};note.scalar=100;assert(e->submit(note));advance(7200);auto data=ui::engine_visual_data(engine_descriptor(unsigned(Instrument::Additive))->visual,track_at(*p,0,0),*u);assert(data.live && data.count==16);view->draw(*p,*u);assert(idle!=roi(*view,1));
 assert(e->submit({CommandType::Stop,0,0,0}));advance(9600);view->draw(*p,*u);auto silent=roi(*view,1);advance(7200);view->draw(*p,*u);assert(silent==roi(*view,1));
 assert(u->analysis());u->select_track(1);assert(!u->analysis());u->select_track(0);u->animate(samples/48+1000);assert(!u->analysis() && !u->visual_fresh());
 auto capture=ui::pcm_visual(*u,VisualSource::CapturePcm);assert(std::strcmp(capture.label,"GROUP CAPTURE / LIVE PCM"));
 mt.instrument=Instrument::Wavetable;mt.wavetable.values[0]=27;auto table=ui::engine_visual_data(engine_descriptor(unsigned(Instrument::Wavetable))->visual,mt,*u);assert(table.range && table.bipolar && table.count==128 && !table.live);float lo=1,hi=0;for(unsigned i=0;i<128;++i){lo=fminf(lo,table.values[i]);hi=fmaxf(hi,table.values[i]);}assert(hi-lo>.3f);assert(fabsf(table.position-27.f/127)<.00001f);
 p->morph_position=.7f;auto morph=ui::engine_visual_data(kMorphVisual,track_at(*p,0,0),*u);assert(morph.position==.7f);
 auto fresh=std::make_unique<Engine>();assert(fresh->stage_project(*p));e->stage_project(*p);u->project_loaded();assert(!u->analysis());

 struct Audit {unsigned errors=0,current=0,pages[kPageDescriptorCount]{},kinds[6]{};} audit;
 view->text_observer=[](const SoftwareCanvas::TextPlacement& p,void* context){auto& a=*static_cast<Audit*>(context);unsigned error=ui::alignment_errors(p);if(error && a.errors++<12)std::fprintf(stderr,"alignment page=%u err=%u x=%d field=%d/%d kind=%u text=%s\n",a.current,error,p.x,p.field_x,p.field_w,unsigned(p.kind),p.text);if(p.band_h)++a.pages[a.current];++a.kinds[unsigned(p.kind)];};view->text_observer_context=&audit;
 for(unsigned mode=0;mode<kPageDescriptorCount;++mode){audit.current=mode;u->set_mode(UiMode(mode));u->notify(UiNotice::None);view->invalidate();view->draw(*p,*u);assert(audit.pages[mode]);}assert(!audit.errors && audit.kinds[unsigned(ui::TextKind::Hero)] && audit.kinds[unsigned(ui::TextKind::ValueNumber)]);
 u->set_mode(UiMode::Library);u->browser().library_instrument=Instrument::Unknown;u->browser().count=0;view->invalidate();view->draw(*p,*u);
 // A completed empty inspection must repaint READING into the empty state.
 u->set_mode(UiMode::Synth);assert(u->toggle_source_select());u->browser().count=0;view->invalidate();view->draw(*p,*u);auto reading=roi(*view,1);u->take_file_action();view->draw(*p,*u);assert(reading!=roi(*view,1));
 // A declared waveform/recipe occupies a real area, including while idle.
 widgets->fill(0,0,0,480,272,widgets->theme.background);widgets->visual_element(0,24,82,430,118,engine_descriptor(unsigned(Instrument::Wavetable))->visual,table,widgets->theme.accent);unsigned area=0;auto brightness=[](uint32_t v){return int((v>>16)&255)+int((v>>8)&255)+int(v&255);};for(int y=108;y<184;++y)for(int x=24;x<454;++x)area+=brightness(widgets->pixels[0][y*480+x])>brightness(widgets->theme.surface)+30;assert(area>2000);
 std::puts("Wave8: descriptor coverage, endpoint interpolation, six themed primitives, live/idle hero ROI, stale selection/generation and Morph fallback passed");
}
