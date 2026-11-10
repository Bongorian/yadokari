#include "live_visual_analysis.h"
#include "engine.h"
#include "future_factory_presets.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
using namespace groove;
int main(){
 auto slot=std::make_unique<LiveVisualAnalysisSlot>();auto in=std::make_unique<VisualPcmFrame>();auto out=std::make_unique<VisualAnalysisFrame>();
 in->sample=12345;in->generation=8;in->track=7;in->source=53;
 for(unsigned i=0;i<512;++i){in->pcm[0][i]=.4f*sinf(6.283185307f*16*i/512);in->pcm[1][i]=.2f;}
 assert(!slot->collect(*out) && !slot->process_one());assert(slot->submit(*in));assert(!slot->submit(*in));assert(slot->process_one());assert(!slot->process_one() && !slot->submit(*in));assert(slot->collect(*out) && !slot->busy());assert(!slot->collect(*out));
 assert(out->sample==12345 && out->generation==8 && out->track==7 && out->source==53);
 unsigned peak=0;for(unsigned i=0;i<64;++i)if(out->spectrum[0][i]>out->spectrum[0][peak])peak=i;
 assert(peak==32 && fabsf(out->spectrum[0][peak]-.4f)<.002f);
 for(unsigned i=0;i<128;++i){float lo=1,hi=-1;for(unsigned j=0;j<4;++j){lo=fminf(lo,in->pcm[0][4*i+j]);hi=fmaxf(hi,in->pcm[0][4*i+j]);}assert(out->scope_low[0][i]==lo && out->scope_high[0][i]==hi);assert(out->scope_low[1][i]==.2f && out->scope_high[1][i]==.2f);assert(out->scope_low[2][i]==0 && out->scope_high[2][i]==0);}
 for(float x:out->spectrum[2])assert(x==0);
 *in=VisualPcmFrame{};in->pcm[0][12]=NAN;in->pcm[1][7]=INFINITY;assert(slot->submit(*in) && slot->process_one() && slot->collect(*out));for(auto& band:out->spectrum)for(float x:band)assert(isfinite(x) && x==0);
 // Switching memory taps and performing FFT must not alter a single audio sample.
 auto p=std::make_unique<ProjectState>();assert(make_future_factory_preset(*p,11));
 auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();assert(a->stage_project(*p) && b->stage_project(*p));
 Command note{CommandType::MidiInput,0,60,0x90};note.scalar=100;assert(a->submit(note) && b->submit(note));
 float audio[256],reference[256],input[256];auto frame=std::make_unique<AudioVisualFrame>();bool seen=false;double energy=0;
 for(unsigned block=0;block<80;++block){for(unsigned i=0;i<128;++i)input[2*i]=input[2*i+1]=.2f*sinf(6.283185307f*(block*128+i)/32);
  if(block==20)a->visual_focus(0,1);
  a->render_ports(audio,nullptr,input,128);b->render_ports(reference,nullptr,input,128);assert(!std::memcmp(audio,reference,sizeof audio));for(float x:audio)energy+=x*x;
  while(a->pop_visual(*frame)){if(frame->taps.source==1 && frame->taps.sample>4000){seen=true;assert(frame->taps.generation==a->staged_generation());assert(!std::memcmp(frame->taps.pcm[0],frame->taps.pcm[1],sizeof frame->taps.pcm[0]));assert(slot->submit(frame->taps) && slot->process_one() && slot->collect(*out));}}
  while(b->pop_visual(*frame)){}
 }
 assert(seen && energy>.001);std::puts("Visual analysis: ownership/busy, exact FFT bin, scope, silence/nonfinite, routed PCM and byte-identical audio passed");
}
