#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>
using namespace groove;
static void capture(const UiRenderer& r,const std::filesystem::path& path){
 FILE* f=std::fopen(path.c_str(),"wb");assert(f);std::fputs("P6\n960 272\n255\n",f);
 for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){auto c=r.pixels[x/480][y*480+x%480];unsigned char rgb[]={uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)};assert(std::fwrite(rgb,1,3,f)==3);}
 assert(!std::fclose(f));
}
int main(int argc,char** argv){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->ui.motion=uint8_t(ui::Motion::Off);
 const Instrument instruments[]={Instrument::DaisyAnalogDrums,Instrument::Sampler,Instrument::DrumKit,Instrument::Layer,Instrument::DaisyAnalogDrums,Instrument::ExternalMidi,Instrument::Empty};
 for(unsigned n=0;n<16;++n)p->tracks[n].instrument=instruments[n%7];
 p->audio[4].kind=TrackKind::Audio;p->audio[11].kind=TrackKind::Looper;
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));auto perf=std::make_unique<Performance>(*e,*p);auto r=std::make_unique<UiRenderer>();float audio[128]{};e->render(audio,0);perf->poll();
 struct Positions {unsigned seen=0;int row[16]{},col[16]{};} positions;
 r->text_observer=[](const SoftwareCanvas::TextPlacement& t,void* context){auto& q=*static_cast<Positions*>(context);if(t.side || t.kind!=ui::TextKind::ValueNumber || t.field_w!=24 || t.field_x<15 || (t.field_x-15)%117)return;unsigned n=unsigned(std::atoi(t.text));if(n<1 || n>16)return;--n;q.seen|=1u<<n;q.col[n]=(t.field_x-15)/117;q.row[n]=t.y;};r->text_observer_context=&positions;
 std::filesystem::path out=argc>1?argv[1]:"";if(argc>1)std::filesystem::create_directories(out);
 auto draw=[&]{perf->notify(UiNotice::None);r->invalidate();r->draw(*p,*perf);};
 for(unsigned ti=0;ti<ui::Theme::kCount;++ti){
  auto settings=perf->device_settings();settings.theme=uint8_t(ti);settings.led=3;assert(perf->set_device_settings(settings));const auto& theme=ui::Theme::by_index(ti);
  for(unsigned kind=0;kind<7;++kind){auto k=ui::EngineKind(kind);auto c=ui::engine_kind_color(k,theme);if(theme.monochrome)assert((c>>16&255)==(c>>8&255) && (c>>8&255)==(c&255));else assert(c==ui::engine_kind_chip(k).color);
   ui::PadGround ground{k};const unsigned fixed[]={10,7,4,14,8,15,17};assert(ui::PadPalette::base(ui::LedState::Assigned,theme,ground)==mk3::indexed_appearance(fixed[kind]));assert((ui::PadPalette::encode(ui::LedState::Assigned,0,ui::Motion::Off,theme,ground)>>2)==fixed[kind]);
   if(theme.monochrome)assert(ui::PadPalette::encode(ui::LedState::Selected,0,ui::Motion::Off,theme,ground)>>2==17);
  }
  assert(ui::PadPalette::base(ui::LedState::Assigned,theme,ui::PadPalette::neutral)==mk3::indexed_appearance(ui::PadPalette::ready));
  for(unsigned kind=0;kind<7;++kind)for(unsigned state=0;state<=unsigned(ui::LedState::MutedBoth);++state)for(unsigned motion=0;motion<3;++motion)for(uint64_t time:{0ull,500ull})assert(ui::PadPalette::encode(ui::LedState(state),time,ui::Motion(motion),theme,{ui::EngineKind(kind)})==ui::PadPalette::encode(ui::LedState(state),time,ui::Motion(motion),ui::Theme::by_index(0),{ui::EngineKind(kind)}));
  assert(perf->select_track(0));perf->set_mode(UiMode::Pad);positions.seen=0;draw();assert(positions.seen==0xffff);
  for(unsigned n=0;n<16;++n){assert(positions.col[n]==int(n%4));const auto color=ui::track_color(*p,n,theme);bool found=false;int x=45+int(n%4)*117,y=64+int(lroundf(float((82+int(mk3::pad_logical(n)/4)*30-76)*ui::Layout::body_h)/131));for(int yy=y;yy<y+16;++yy)for(int xx=x;xx<x+69;++xx)found|=r->pixels[0][yy*480+xx]==color;assert(found);}
  for(unsigned col=0;col<4;++col)for(unsigned row=1;row<4;++row)assert(positions.row[(row-1)*4+col]>positions.row[row*4+col]);
  ui::LedFrame led;perf->led_frame(led);
  for(unsigned n=0;n<16;++n){unsigned hw=mk3::pad_logical(n);assert(led.pads[hw]==ui::PadPalette::encode(perf->pad_feedback(n),perf->ui_time(),perf->visual_settings().motion,theme,{ui::track_engine_kind(*p,n)}));}
  assert(ui::track_color(*p,4,theme)==ui::track_color(*p,11,theme)); // Audio and Looper
  assert(ui::track_color(*p,0,theme)==ui::track_color(*p,7,theme));
  if(argc>1)capture(*r,out/(std::to_string(ti)+"-pad.ppm"));
  // Actual press selects the unchanged logical number, with no note trigger.
  perf->control(5,0x08);for(unsigned hw=0;hw<16;++hw){assert(perf->pad(hw,0x10,3000));assert(perf->selected_track()==mk3::pad_logical(hw));assert(perf->pad(hw,0x20,0));}perf->control(5,0);
  // SELECT map enters and leaves without requiring an unrelated redraw.
  perf->select_track(0);perf->set_mode(UiMode::Synth);perf->animate(1000);draw();std::vector<uint32_t> original(r->pixels[0],r->pixels[0]+480*272);
  perf->control(5,0x08);assert(perf->pad_role()==ui::PadRole::TrackSelect);positions.seen=0;r->draw(*p,*perf);assert(positions.seen==0xffff);if(argc>1)capture(*r,out/(std::to_string(ti)+"-select.ppm"));
  perf->control(5,0);r->draw(*p,*perf);assert(std::equal(original.begin()+76*480,original.begin()+207*480,r->pixels[0]+76*480));
  // Changing a different track must repaint even if wire quantization collides.
  perf->set_mode(UiMode::Pad);draw();p->tracks[7].instrument=Instrument::Sampler;r->draw(*p,*perf);assert(ui::track_color(*p,7,theme)==ui::engine_kind_color(ui::EngineKind::Sampler,theme));p->tracks[7].instrument=Instrument::DaisyAnalogDrums;
  perf->set_mode(UiMode::Mixer);draw();if(argc>1)capture(*r,out/(std::to_string(ti)+"-mixer.ppm"));
  perf->select_track(0);p->tracks[0].root_note=60;p->tracks[0].scale_root=0;p->tracks[0].scale_mask=0xfff;
  for(auto mode:{UiMode::Keyboard,UiMode::Chord}){perf->set_mode(mode);perf->led_frame(led);assert(led.pads[mk3::pad_logical(0)]==mk3::indexed(17,3));assert(led.pads[mk3::pad_logical(1)]==mk3::indexed(10,2));}
  perf->set_mode(UiMode::Settings);draw();perf->led_frame(led);for(unsigned n=0;n<7;++n){unsigned hw=mk3::pad_logical(n);assert(led.pads[hw]==ui::PadPalette::encode(perf->pad_feedback(n),perf->ui_time(),perf->visual_settings().motion,theme,ui::PadPalette::neutral));}
 }
 std::puts("Track kind: 9 themes, seven grounds, neutral/events, current kind, physical 16 positions and SELECT restore passed");
}
