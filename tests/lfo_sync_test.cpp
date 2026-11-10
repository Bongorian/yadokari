#include "engine.h"
#include "project_codec.h"
#include "performance.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
using namespace groove;
static bool near(float a,float b){return fabsf(a-b)<2e-5f;}
int main(){
 const unsigned bpms[]={73,120,173,300};const double beats[]={8,4,3,2,4.0/3,1.5,1,2.0/3,.75,.5,1.0/3,.25};
 for(unsigned bpm:bpms)for(unsigned division=0;division<12;++division){
  TempoLfo lfo;lfo.request(1,float(division));uint64_t beat=0,fraction=0,period=48000*60;
  for(unsigned n=0;n<150000;++n){float phase=lfo.next(0,1,1.f/48000,true,beat,fraction,period);double cycles=(double(beat)+double(fraction)/period)/beats[division];float expected=float(cycles-floor(cycles));assert(near(phase,expected) || near(fabsf(phase-expected),1));fraction+=bpm;if(fraction>=period){fraction-=period;++beat;}}
 }
 TempoLfo free;float reference=0,actual=0;for(unsigned i=0;i<100000;++i){reference+=3.75f*(1.f/48000);if(reference>=1)reference-=1;actual=free.next(actual,3.75f,1.f/48000,true,i/24000,0,2880000);assert(actual==reference);}
 TempoLfo quantized;quantized.request(1,6);quantized.next(0,1,1.f/48000,true,0,0,2880000);quantized.request(1,9);quantized.next(0,1,1.f/48000,true,0,1440000,2880000);assert(quantized.division==6);quantized.next(0,1,1.f/48000,true,1,0,2880000);assert(quantized.division==9);
 auto p=std::make_unique<ProjectState>();auto q=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=120;p->tracks[0].lfo_sync=1;p->tracks[0].lfo_division=9;
 assert(parameter_index(11)<kMorphParameters && morph_index(12)<kCommonMorphParameters);
 p->patterns[0].tracks[0].automation[0].enabled=true;p->patterns[0].tracks[0].automation[0].parameter=12;p->patterns[0].tracks[0].automation[0].values[0]=6;
 p->macros[0].active=true;auto& route=p->macros[0].routes[0];route.enabled=true;route.parameter=3;route.source=ModSource::Lfo;
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource input{bytes.data,bytes.size,0};assert(decode_project(input,*q));assert(q->tracks[0].lfo_sync==1 && q->tracks[0].lfo_division==9 && q->patterns[0].tracks[0].automation[0].parameter==12);
 p->patterns[0].tracks[0].automation[0].enabled=false;auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[128]{};e->render(audio,0);assert(e->submit({CommandType::Start,0,0,0}));e->render(audio,64);
 assert(near(e->test_lfo_phase(0),float(63*120)/2880000*2));assert(near(e->test_mod_runtime(0).lfo,.5f+.5f*sinf(e->test_lfo_phase(0)*6.2831853f)));
 assert(e->submit({CommandType::Stop,0,0,0}));e->render(audio,64);float stopped=e->test_lfo_phase(0);e->render(audio,64);assert(e->test_lfo_phase(0)==stopped);assert(e->submit({CommandType::Continue,0,0,0}));e->render(audio,64);assert(e->test_lfo_phase(0)>stopped);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Modulation);assert(ui->knob_parameter(1)==12 && ui->knob_parameter(4)==11);
 printf("LFO sync: 4 BPM x 12 divisions, exact free-run, beat-quantized rate, song roundtrip, transport and shared macro phase passed\n");
}
