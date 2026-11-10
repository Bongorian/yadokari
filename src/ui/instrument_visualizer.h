#pragma once
#include "widgets.h"
#include "visual_adapters.h"
#include "../engine_pages.h"
namespace groove { namespace ui {
inline const char* plaits_model_name(unsigned model){const auto* p=engine_parameter(Instrument::PlaitsMacro,5);return p && model<p->choice_count?p->choices[model]:"Plaits";}
// Compatibility entry points share the same primitives; no per-engine artwork.
class InstrumentVisualizer {
public:
 static void scope(Widgets& c,unsigned side,int x,int y,int w,int h,const AudioVisualFrame& frame,unsigned track,uint32_t color,unsigned expected=255,unsigned pattern=255,unsigned model=255){VisualData d;d.count=64;bool match=frame.sample && (expected==255 || frame.instrument[track]==expected) && (pattern==255 || frame.pattern[track]==pattern) && (model==255 || frame.model[track]==model);if(match)for(unsigned i=0;i<64;++i)d.low[i]=d.values[i]=frame.waveform[track][i];c.visual_scope(side,x,y,w,h,d,color);if(!match)c.label(side,x+6,y+h/2,"Play to view output",c.theme.dim,FontRole::Small,w-12);}
 static void filter(Widgets& c,unsigned side,int x,int y,int w,int h,float cutoff,uint32_t color,float resonance=-1){auto d=response_visual(ResponseModel::LowPass,cutoff,resonance<0?0:resonance);c.visual_response(side,x,y,w,h,d,color);}
 static void draw(Widgets& c,ConstTrackView track,const Performance& perf,unsigned side=1){const auto* descriptor=engine_descriptor(unsigned(track.instrument));if(!descriptor)return;auto data=engine_visual_data(descriptor->visual,track,perf);c.visual_element(side,24,82,430,118,descriptor->visual,data,c.theme.accent);}
};
}}
