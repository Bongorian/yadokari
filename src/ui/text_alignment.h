#pragma once
#include "content.h"
#include "font_renderer.h"
namespace groove { namespace ui {
// Actual pixel coordinates, independent of marquee time and page names.
template<class Placement> inline unsigned alignment_errors(const Placement& p){
 unsigned errors=0;const auto& face=font_face(p.role,p.numeric);
 if(p.band_h){int delta=2*cap_top(face,p.y)+face.cap_height-(2*p.band_y+p.band_h);if(delta < -2 || delta>2)errors|=1;}
 if(p.field_w>0 && p.kind!=TextKind::Plain){
  int width=text_width(p.text,p.role,p.numeric);if(width>p.field_w)width=p.field_w;
  int expected=p.field_x+(p.align==TextAlign::Right?p.field_w-width:p.align==TextAlign::Center?(p.field_w-width)/2:0);
  if(p.x!=expected)errors|=2;
 }
 if(p.kind==TextKind::Hero && (p.align!=TextAlign::Center || p.band_h==0))errors|=4;
 if(p.kind==TextKind::Section && (p.field_x!=24 || p.band_h==0))errors|=8;
 return errors;
}
}}
