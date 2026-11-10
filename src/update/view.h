#pragma once
#include "manager.h"
#include "../ui/theme.h"
#include "../ui/led_state.h"
#include "../ui/font_renderer.h"
namespace groove { namespace update {
inline void controller_leds(const Manager& manager,bool busy,bool can_confirm,uint8_t pads[16],uint8_t buttons[62],const ui::Theme& theme=ui::Theme::by_index(ui::Theme::kDefault)){
 for(unsigned i=0;i<16;++i)pads[i]=0;
 for(unsigned i=0;i<62;++i)buttons[i]=0;
 buttons[43]=0x9e;
 uint16_t enabled=1u<<15;
 if(manager.loaded()&&manager.armed()&&!busy){enabled|=15;const auto& state=manager.state();if(state.phase==Phase::Ready)enabled|=1u<<5;if(state.phase==Phase::Trial&&can_confirm)enabled|=1u<<6;if(state.phase==Phase::Stable||state.phase==Phase::Recovery){if(state.slots[state.active].size)enabled|=1u<<7;const auto& previous=state.slots[1-state.active];if(previous.version&&previous.version<=state.highest)enabled|=1u<<8;}}
 for(unsigned pad=0;pad<16;++pad)if(enabled&(1u<<pad))pads[(3-pad/4)*4+pad%4]=ui::LedStateEngine::encode({manager.error()==Error::None?ui::LedState::Assigned:ui::LedState::Warning,uint8_t(pad)},0,0,ui::Motion::Off,theme);
}
template<class Canvas> void draw_view(Canvas& c,const Manager& manager,const char* transport,unsigned partition,bool trial,unsigned running_version){
 using Font=ui::FontRole;const auto& t=c.theme;
 for(unsigned side=0;side<2;++side)c.fill(side,0,0,480,272,t.background);
 const char* buttons[]={"ETHERNET","WI-FI","SD","USB-C","TRY","CONFIRM","BOOT","CANCEL"};
 for(unsigned i=0;i<8;++i){int x=8+int(i%4)*117;c.rounded(i/4,x,4,113,24,ui::Geometry::button,t.surface);c.label(i/4,x+7,10,buttons[i],t.secondary,Font::Small,99);}
 c.label(0,12,44,"Running",t.secondary,Font::Small,80);c.value(0,81,39,52,running_version,t.text,Font::Body,0);c.label(0,170,44,"Confirmed",t.secondary,Font::Small,80);c.value(0,249,39,52,manager.state().slots[manager.state().active].version,t.teal,Font::Body,0);c.label(0,334,44,trial?"Trial":"Partition",t.secondary,Font::Small,70);c.value(0,410,39,47,partition,t.amber,Font::Body,0);
 const char* actions[]={"1  Ethernet","2  Wi-Fi","3  Offline SD","4  USB-C","6  Try update","7  Confirm","8  Boot","9  Rollback","16  Cancel"};
 for(unsigned i=0;i<9;++i){int x=12+int(i%3)*154,y=77+int(i/3)*36;c.rounded(0,x,y,146,29,ui::Geometry::button,t.surface);c.label(0,x+10,y+6,actions[i],i==8?t.amber:t.text,Font::Body,126);}
 c.label(0,12,192,transport,t.secondary,Font::Small,450);c.label(0,12,214,error_name(manager.error()),manager.error()==Error::None?t.teal:t.red,Font::Body,450);
 unsigned progress=manager.total()?unsigned(uint64_t(manager.received())*100/manager.total()):(manager.state().phase==Phase::Ready||manager.state().phase==Phase::Trial)?100:0;
 c.rounded(0,12,246,389,3,0,t.surface);if(progress)c.rounded(0,12,246,389*int(progress)/100,3,0,t.motion);c.value(0,413,232,49,progress,t.text,Font::Number,0);
 static const char* phases[]={"Stable","Installing","Ready for trial","Trial / not committed","Boot handoff","Recovery required"};
 c.rounded(1,12,43,456,28,ui::Geometry::panel,t.surface);c.label(1,24,49,phases[unsigned(manager.state().phase)],t.amber,Font::Body,420);c.label(1,12,80,"Pending version",t.secondary,Font::Small,170);c.value(1,202,76,60,manager.state().slots[manager.state().pending].version,t.text,Font::Body,0);
 static const char* events[]={"","Installed","Trial","Confirmed","Rolled back","Rejected","Boot failed"};unsigned count=manager.state().count,start=count>6?count-6:0;
 for(unsigned i=start;i<count;++i){const auto& entry=manager.state().history[i];int y=108+24*int(i-start);if((i-start)%2==0)c.rounded(1,12,y,456,23,ui::Geometry::cell,t.surface);c.value(1,22,y+1,31,entry.sequence,t.secondary,Font::Body,0);c.label(1,75,y+2,unsigned(entry.event)<=6?events[unsigned(entry.event)]:"Unknown",t.text,Font::Body,196);c.value(1,295,y+1,88,entry.version,t.teal,Font::Body,0);c.label(1,424,y+2,entry.slot?"B":"A",t.secondary,Font::Body,25);}
}
}}
