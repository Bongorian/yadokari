#pragma once
#include "theme.h"
#include "../mk3_led_transport.h"
namespace groove { namespace ui {
enum class LedState:uint8_t { Off, Idle, Assigned, Playing, Selected, Editing, Muted, PerformanceActive, Queued, Triggered, Recording, Warning, Armed, Finalizing, MutedPattern, MutedBoth };
enum class Motion:uint8_t { Off, Reduced, Full };
enum class StripProfile:uint8_t { Uncalibrated, PaletteHypothesis, MonoHypothesis };
struct VisualSettings { Motion motion=Motion::Full;uint8_t theme=Theme::kDefault,pixel_order=0;StripProfile strip=StripProfile::PaletteHypothesis;bool preview565=true; };
struct LedIntent {LedState state=LedState::Off;uint8_t hue=17;};
struct LedFrame {uint8_t pads[16]{},buttons[62]{},strip[25]{},strip_level[25]{};LedState pad_state[16]{};uint32_t pad_rgb[16]{},button_rgb[62]{},strip_rgb[25]{};};
class LedStateEngine {
public:
 // Explicit priority: enum ordinals are not a visual stacking contract.
 // A held pad must not hide mute, a pending launch, or recording ownership.
 static unsigned priority(LedState state){
  switch(state){
   case LedState::Off:return 0;case LedState::Idle:return 1;
   case LedState::Assigned:return 2;case LedState::Selected:return 3;
   case LedState::Editing:return 4;case LedState::Playing:return 5;
   case LedState::Triggered:return 6;case LedState::Muted:return 7;
   case LedState::MutedPattern:return 8;case LedState::MutedBoth:return 9;
   case LedState::PerformanceActive:return 10;case LedState::Queued:return 11;
   case LedState::Armed:return 12;case LedState::Finalizing:return 13;
   case LedState::Recording:return 14;case LedState::Warning:return 15;
  }
  return 0;
 }
 static void overlay(LedIntent& led,LedState state,unsigned hue=17){if(priority(state)>=priority(led.state))led={state,uint8_t(hue)};}
 // Palette brightness is controlled separately; compare hue/chroma, not raw
 // RGB lightness, so a bright violet accent cannot collapse onto muted blue.
 static void hue_saturation(uint32_t color,int& hue,int& saturation){
  int r=(color>>16)&255,g=(color>>8)&255,b=color&255;
  int hi=r>g?r:g;if(b>hi)hi=b;int lo=r<g?r:g;if(b<lo)lo=b;
  int chroma=hi-lo;saturation=hi?chroma*255/hi:0;hue=0;
  if(chroma){hue=hi==r?(g-b)*256/chroma:hi==g?512+(b-r)*256/chroma:1024+(r-g)*256/chroma;if(hue<0)hue+=1536;}
 }
 static unsigned token_hue(uint32_t token,const Theme&,uint32_t reserved=0){
  int hue,saturation;hue_saturation(token,hue,saturation);unsigned best=17;int distance=0x7fffffff;
  for(unsigned i=1;i<18;++i){if(reserved&(1u<<i))continue;int h,s;hue_saturation(mk3::indexed_appearance(i),h,s);int d=hue-h;if(d<0)d=-d;if(d>768)d=1536-d;int ds=saturation-s;
   int score=saturation<90?s*s:d*d+ds*ds/16;
   if(score<distance){distance=score;best=i;}
  }return best;
 }
 static uint8_t shortcut(const Theme& theme,bool upper,bool selected,bool muted,LedState recording,float beat,uint64_t ms,Motion motion,unsigned brightness=3){
  uint32_t color=muted?theme.dim:selected?theme.accent:theme.secondary;
  unsigned level=brightness==1?(selected || upper?1:0):brightness==2?(upper?2:1):selected?(upper?3:2):(upper?2:1);
  if(muted && !selected)level=upper?1:0;
  if(recording==LedState::Recording || recording==LedState::Armed || recording==LedState::Finalizing){color=theme.danger;level=recording==LedState::Recording?3:recording==LedState::Finalizing?1:pulse_high(recording,beat,ms,motion)?3:1;}
  if(level>brightness)level=brightness;
  return level?mk3::indexed(token_hue(color,theme),level):0;
 }
 static bool pulse_high(LedState state,float beat,uint64_t ms,Motion motion){
  if(motion==Motion::Off)return true;
  if(state==LedState::Queued && motion==Motion::Full)return beat<0.45f;
  const unsigned half_period=state==LedState::Warning && motion==Motion::Full?250:500;
  return (ms/half_period)%2==0;
 }
 static uint8_t encode(LedIntent led,float beat,uint64_t ms,Motion motion,const Theme& t=Theme::by_index(Theme::kDefault)){
  if(led.state==LedState::Off || led.state==LedState::Idle)return 0;
  uint32_t color=t.track[led.hue%16];unsigned level=led.state==LedState::Assigned?2:led.state==LedState::Muted || led.state==LedState::MutedPattern || led.state==LedState::MutedBoth?1:3;
  switch(led.state){
   case LedState::Playing:case LedState::Triggered:case LedState::PerformanceActive:color=t.motion;break;
   case LedState::Queued:case LedState::Finalizing:case LedState::Warning:color=t.warn;break;
   case LedState::Armed:case LedState::Recording:color=t.danger;break;
   default:break;
  }
  if((led.state==LedState::Queued || led.state==LedState::Armed || led.state==LedState::Warning) && !pulse_high(led.state,beat,ms,motion))level=1;
  return mk3::indexed(token_hue(color,t),level);
 }
 // Monochrome lamps keep the same temporal vocabulary as indexed pads.
 static uint8_t mono_state(LedState state,float beat,uint64_t ms,Motion motion){
  if(state==LedState::Off)return 0;
  const bool pulsed=state==LedState::Armed || state==LedState::Queued || state==LedState::Warning;
  return mono(true,state==LedState::Finalizing || (pulsed && !pulse_high(state,beat,ms,motion)));
 }
 static uint32_t preview(uint8_t code,const Theme& t){
  if(!code)return t.background;
  unsigned hue=code>>2,level=code&3;uint32_t color=hue<=17?mk3::indexed_appearance(hue):t.text;
  return mix(t.background,color,level==3?255:level==2?130:50);
 }
 static uint8_t mono(bool active,bool dim=false){return active?(dim?mk3::mono_dim:mk3::mono_active):0;}
};
}}
