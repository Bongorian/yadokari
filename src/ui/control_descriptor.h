#pragma once
#include <stdint.h>
#include "icons.h"
#include "screen_grammar.h"
namespace groove { namespace ui {
// Action-specific overloads live beside the control declarations (ADL).
template<class Action> constexpr uint8_t icon_for(Action){return icons::None;}
 template<class Action> constexpr TopWork top_work(Action a){return unsigned(a)?TopWork::Execute:TopWork::Empty;}
// Control-path metadata only: labels, dispatch identity and bounded values.
template<class Action> struct ControlDescriptor {
 const char* label="";Action action={};bool enabled=false;
 int minimum=0,maximum=1,shift_step=1;
 bool selected=false;uint8_t icon=0;
 constexpr ControlDescriptor()=default;
 constexpr ControlDescriptor(const char* text,Action command,bool available,int lo=0,int hi=1,int step=1,bool chosen=false,uint8_t symbol=0)
 :label(text),action(command),enabled(available),minimum(lo),maximum(hi),shift_step(step),selected(chosen),icon(symbol?symbol:icon_for(command)){}
 int adjusted(int value,int delta,bool shift=false)const{
  int64_t next=int64_t(value)+int64_t(delta)*(shift?shift_step:1);
  return next<minimum?minimum:next>maximum?maximum:int(next);
 }
 float normalized(float value)const{
  if(maximum<=minimum)return 0;
  float n=(value-minimum)/float(maximum-minimum);return n<0?0:n>1?1:n;
 }
};
// Display-only projection: keeps label and icon on one resolution path.
struct ControlPresentation {
 const char* label="";uint8_t icon=0;TopWork work=TopWork::Empty;
 template<class Action> ControlPresentation(const ControlDescriptor<Action>& d):label(d.label),icon(d.icon),work(top_work(d.action)){}
};
}}
