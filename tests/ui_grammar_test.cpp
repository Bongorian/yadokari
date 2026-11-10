#include "ui_renderer.h"
#include "page_descriptors.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <filesystem>
#include <cstdlib>
using namespace groove;
static unsigned violations=0,controls=0,failures=0;
#define CHECK(...) do{if(!(__VA_ARGS__)){std::printf("FAIL line=%d: %s\n",__LINE__,#__VA_ARGS__);++failures;}}while(false)
static void grammar(const Performance& u){
 auto type=screen_type(u);
 for(unsigned i=0;i<8;++i){const auto top=u.soft_presentation(i);if(!*top.label)continue;++controls;
  if(top_placement_exception(u,i)){CHECK(u.layer_page() && (i==2 || i==3) && top.work==ui::TopWork::Execute);continue;}
  if(type!=ui::ScreenType::Selection && type!=ui::ScreenType::Transient){bool bad=i<4?top.work!=ui::TopWork::Selection:top.work==ui::TopWork::Selection;if(bad){++violations;std::printf("grammar mode=%s shift=%d top=%u label=%s role=%s\n",mode_name(u.mode()),u.shifted(),i+1,top.label,ui::top_work_name(top.work));}}
 }
}
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::PlaitsMacro;p->ui.motion=0;
 auto e=std::make_unique<Engine>();CHECK(e->stage_project(*p));Performance u(*e,*p);uint64_t samples=0;
 auto advance=[&](unsigned frames=4800){float audio[128]{};while(frames){unsigned n=frames>64?64:frames;e->render(audio,n);samples+=n;u.animate(5000+samples/48);u.poll();frames-=n;}};
 advance();u.set_mode(UiMode::Synth);CHECK(u.soft_key(0));auto id=engine_target(Instrument::PlaitsMacro,0);unsigned edit_knob=0;while(edit_knob<8 && u.knob_parameter(edit_knob)!=id)++edit_knob;CHECK(edit_knob<8);CHECK(engine_target(id));CHECK(std::strcmp(u.knob_label(0),"TEMPO"));
 float base=parameter_base(*p,track_at(*p,0,0),id);MacroRoute route{};route.enabled=true;route.tracks=1;route.parameter=id;route.low=.15f;route.high=.85f;route.mode=LayerMode::Replace;
 CHECK(u.macro_route(0,0,route) && u.macro_value(0,1,true));advance();CHECK(u.knob_display_value(edit_knob)>.8f);CHECK(parameter_base(*p,track_at(*p,0,0),id)==base);
 CHECK(u.set_parameter(id,.5f));CHECK(u.set_lock(0,id,.2f));u.set_mode(UiMode::Step);CHECK(u.pad(12,0x10,4000));CHECK(u.pad(12,0x20,0));u.set_mode(UiMode::Synth);CHECK(u.play());advance(1000);advance(2400);CHECK(fabsf(u.knob_display_value(edit_knob)-.2f)<.02f);u.stop();advance(64);
 u.set_mode(UiMode::Perform);CHECK(u.soft_key(3) && u.performance_page()==3);CHECK(u.soft_key(2) && u.performance_page()==2);CHECK(u.soft_key(0) && u.performance_page()==0);CHECK(u.soft_key(1) && u.performance_page()==1);
 for(unsigned shift=0;shift<2;++shift){u.set_shift(shift);CHECK(!std::strcmp(u.soft_label(7),"AUDIO"));}u.set_shift(false);CHECK(u.soft_key(7) && u.audio_page());u.soft_key(7);CHECK(!u.audio_page());
 u.set_mode(UiMode::Settings);for(unsigned page=0;page<7;++page){while(u.settings_page()<page)CHECK(u.encoder_direction(0x08));CHECK(*u.knob_label(0));for(unsigned k=1;k<8;++k){CHECK(!*u.knob_label(k));CHECK(!u.knob(k,1000));}if(u.settings_count()>1){const char* name=u.knob_label(0);CHECK(u.encoder(1));CHECK(std::strcmp(name,u.knob_label(0)));CHECK(u.encoder(-1));}}
 CHECK(!std::strcmp(u.knob_label(0),"SPACE UNIT"));CHECK(u.knob(0,1000) && u.system_space_unit()==1);
 u.set_mode(UiMode::Browser);u.take_file_action();CHECK(!std::strcmp(u.knob_label(6),"AUDITION LEVEL"));CHECK(!std::strcmp(u.knob_label(7),"FAV FILTER"));CHECK(u.knob(7,1000));CHECK(u.browser().favorites_only && u.take_file_action()==FileAction::Refresh);CHECK(!std::strcmp(u.soft_label(6),"PROJECT"));
 u.set_mode(UiMode::Sampler);CHECK(u.soft_key(5) && u.erase_confirmation());CHECK(!std::strcmp(u.soft_label(0),"CONFIRM") && !std::strcmp(u.soft_label(1),"CANCEL"));CHECK(u.take_file_action()==FileAction::None);CHECK(u.soft_key(1));CHECK(!u.modal_active() && u.take_file_action()==FileAction::None);CHECK(u.soft_key(5) && u.soft_key(0));CHECK(u.take_file_action()==FileAction::MapPads);
 u.set_mode(UiMode::Diagnostics);for(unsigned i=0;i<4;++i)CHECK(!*u.soft_label(i));CHECK(!u.encoder_direction_available(0x08));
 auto lane=p->audio[0];lane.kind=TrackKind::Looper;CHECK(u.set_audio_lane(0,lane));u.set_mode(UiMode::Synth);CHECK(u.mode()==UiMode::Synth);lane.kind=TrackKind::Instrument;CHECK(u.set_audio_lane(0,lane));
 u.set_mode(UiMode::Synth);CHECK(u.soft_key(1));for(unsigned i=0;i<8;++i){CHECK(u.knob_parameter(i)==channel_parameter(i));CHECK(!std::strcmp(u.knob_label(i),channel_name(i)));}
 CHECK(u.set_parameter(1,.3f));float prior_cutoff=track_at(*p,0,0).cutoff;CHECK(u.knob(0,1000));CHECK(track_at(*p,0,0).level>.3f && track_at(*p,0,0).cutoff==prior_cutoff);
 for(unsigned page:{0u,2u}){u.set_mode(UiMode::Perform);CHECK(u.soft_key(page));for(unsigned i=0;i<8;++i){CHECK(u.knob_parameter(i)==channel_parameter(i));CHECK(!std::strcmp(u.knob_label(i),channel_name(i)));}}
 // Physical transfer entry keeps the established Top roles and blocks playing.
 u.set_mode(UiMode::Settings);while(u.settings_page()<6)CHECK(u.encoder_direction(0x08));CHECK(!std::strcmp(u.soft_label(4),"FILE TRANSFER"));CHECK(u.soft_key(4) && u.take_transfer_request());
 TransferView transfer;transfer.active=true;transfer.remaining=600;u.transfer_view(transfer);CHECK(u.soft_selected(0) && !u.soft_selected(1));CHECK(u.soft_key(1) && u.transfer_view().station && u.soft_selected(1));CHECK(u.soft_key(4) && u.take_transfer_start());
 for(unsigned i=0;i<8;++i){CHECK(!*u.knob_label(i));}
 CHECK(!u.dispatch({ControllerInput::Type::Knob,0,1000}));CHECK(!u.dispatch({ControllerInput::Type::Pad,0,4000}));CHECK(!u.knob(0,1000) && !u.pad(0,0x10,4000) && !u.encoder(1) && !u.strip_input(100,1024));ui::LedFrame transfer_leds;u.led_frame(transfer_leds);CHECK(transfer_leds.buttons[17] && transfer_leds.buttons[19]);for(auto pad:transfer_leds.pads){CHECK(!pad);}CHECK(u.soft_key(5) && u.take_transfer_browse() && u.take_transfer_close());transfer.started=true;u.transfer_view(transfer);CHECK(!u.soft_enabled(0) && !u.soft_enabled(4) && u.soft_enabled(7));CHECK(u.control(1,0x80) && u.take_transfer_close());u.control(1,0);
 if(const char* output=std::getenv("MGX_TRANSFER_FRAMES")){std::filesystem::create_directories(output);auto renderer=std::make_unique<UiRenderer>();for(unsigned theme=0;theme<ui::Theme::kCount;++theme){p->ui.theme=uint8_t(theme);auto themed=std::make_unique<Performance>(*e,*p);auto preferences=themed->device_settings();preferences.theme=uint8_t(theme);CHECK(themed->set_device_settings(preferences));for(unsigned state=0;state<4;++state){transfer.usb=state==3;transfer.active=true;transfer.started=state>0;transfer.failed=false;transfer.remaining=state==3?30:state?543:600;transfer.received=state==1?5*1024*1024:0;transfer.length=state==1?12*1024*1024:0;copy_text(transfer.ssid,sizeof transfer.ssid,state==3?"USB-C / MGF1 v1":"Groovebox-08c4e76a");copy_text(transfer.code,sizeof transfer.code,"7b2e59c1");copy_text(transfer.address,sizeof transfer.address,"http://192.168.76.1/");copy_text(transfer.discovery,sizeof transfer.discovery,state==3?"":"groovebox.local");copy_text(transfer.status,sizeof transfer.status,state==3?"USB FILE SESSION":state==2?"HASH_MISMATCH":state?"RECEIVING":"READY TO START");themed->transfer_view(transfer);renderer->draw(*p,*themed);char path[512];std::snprintf(path,sizeof path,"%s/theme-%u-transfer-%u.ppm",output,theme,state);FILE* image=std::fopen(path,"wb");CHECK(image);if(!image)continue;if(image){std::fprintf(image,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t pixel=renderer->pixels[x/480][y*480+x%480];unsigned char rgb[]={uint8_t(pixel>>16),uint8_t(pixel>>8),uint8_t(pixel)};CHECK(std::fwrite(rgb,1,3,image)==3);}std::fclose(image);}}}p->ui.theme=0;u.sync_ui_settings();}
 u.transfer_view({});
 // Provider semantics are inspected for every mode, engine and reachable local face.
 for(unsigned mode=0;mode<kPageDescriptorCount;++mode){u.set_shift(false);u.set_mode(UiMode(mode));for(unsigned page=0;page<7;++page){for(unsigned shift=0;shift<2;++shift){u.set_shift(shift);grammar(u);}u.set_shift(false);u.encoder_direction(0x08);}}
 CHECK(u.select_track(0));
 for(unsigned engine=0;engine<instrument_count();++engine){p->tracks[0].instrument=Instrument(engine);u.set_mode(UiMode::Synth);for(unsigned page=0;page<4;++page){u.set_shift(false);u.set_mode(UiMode::Synth);u.page_top_key(page);for(unsigned shift=0;shift<2;++shift){u.set_shift(shift);grammar(u);}u.set_shift(false);if(u.modal_active())u.modal_cancel();}}
 CHECK(!violations);CHECK(controls>500);CHECK(ui::Layout::legend_y==242 && ui::Layout::tracker_rows==9);
 for(unsigned i=0;i<ui::Theme::kCount;++i){const auto& theme=ui::Theme::by_index(i);CHECK(theme.value_edit==theme.accent);}

 std::printf("Wave11 grammar: %u Top controls, all modes/engines, K1 settings, live macro/lock values, single diagnostics, audio entry, cancellation and palette inspected\n",controls);
 std::printf("Collected failures=%u, placement violations=%u\n",failures,violations);
 return failures || violations?1:0;
}
