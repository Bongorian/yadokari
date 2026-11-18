#include "engine.h"
#include "project_codec.h"
#include "ui_renderer.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace groove;

static void note(Engine& engine,unsigned track,unsigned pitch,bool on){
 Command input{CommandType::MidiInput,uint8_t(track),uint8_t(pitch),uint16_t(on?0x90:0x80)};
 input.scalar=on?100:0;
 assert(engine.submit(input));
}

static float render(Engine& engine,unsigned frames,AudioVisualFrame& visual){
 assert(frames%24==0);
 float peak=0;
 for(unsigned i=0;i<frames;i+=24){
  float pcm[48];
  engine.render(pcm,24);
  for(float sample:pcm){assert(std::isfinite(sample));peak=std::fmax(peak,std::fabs(sample));}
  while(engine.pop_visual(visual)){}
 }
 return peak;
}

static uint64_t cutaway_pixels(const UiRenderer& display){
 uint64_t signature=1469598103934665603ull;
 for(unsigned y=103;y<200;++y)for(unsigned x=24;x<281;++x){
  signature=(signature^display.pixels[1][y*480+x])*1099511628211ull;
 }
 return signature;
}

int main(){
 auto project=std::make_unique<ProjectState>();
 make_empty_project(*project);
 project->tracks[0].instrument=project->tracks[1].instrument=Instrument::Chimera;
 project->tracks[0].engine_parameters[0]=.61f;
 project->tracks[0].engine_parameters[4]=.67f;
 ChunkBuffer encoded;
 assert(encode_project_buffer(encoded,*project));
 auto restored=std::make_unique<ProjectState>();
 ChunkSource input{encoded.data,encoded.size,0};
 assert(decode_project(input,*restored));
 assert(restored->tracks[0].instrument==Instrument::Chimera);
 assert(restored->tracks[0].engine_parameters[0]==.61f && restored->tracks[0].engine_parameters[4]==.67f);
 restored->ui.motion=0;

 auto engine=std::make_unique<Engine>();
 assert(engine->stage_project(*restored));
 AudioVisualFrame visual{};
 engine->visual_focus(0);
 assert(render(*engine,4800,visual)==0);
 assert(visual.chimera_available && visual.chimera.gate==0);
 const auto rest=visual.chimera;
 note(*engine,0,57,true);
 assert(render(*engine,9600,visual)>.001f);
 assert(visual.chimera.gate>.2f && !visual.invalid_audio);
 assert(std::fabs(visual.chimera.tongue[12].x-rest.tongue[12].x)>.01f);

 auto ui=std::make_unique<Performance>(*engine,*restored);
 float pcm[48];
 for(unsigned i=0;i<120;++i){engine->render(pcm,24);ui->poll();}
 assert(ui->visual_fresh() && ui->visual().chimera.gate>.2f);
 auto display=std::make_unique<UiRenderer>();
 bool found_cutaway=false;
 display->text_observer=[](const SoftwareCanvas::TextPlacement& text,void* context){
  if(text.side==1 && std::strcmp(text.text,"VOCAL TRACT")==0)*static_cast<bool*>(context)=true;
 };
 display->text_observer_context=&found_cutaway;
 constexpr UiMode views[]={UiMode::Synth,UiMode::Pad,UiMode::Keyboard,UiMode::Chord};
 uint64_t sounding[4]{};
 auto draw_cutaway=[&](UiMode mode){
  ui->set_mode(mode);
  found_cutaway=false;
  display->draw(*restored,*ui);
  assert(found_cutaway);
  return cutaway_pixels(*display);
 };
 for(unsigned i=0;i<4;++i)sounding[i]=draw_cutaway(views[i]);
 ui->set_mode(UiMode::Synth);
 assert(ui->knob_parameter(0)==engine_target(Instrument::Chimera,0));

 note(*engine,0,57,false);
 for(unsigned i=0;i<300;++i){engine->render(pcm,24);ui->poll();}
 assert(ui->visual_fresh() && ui->visual().chimera.gate==0);
 for(unsigned i=0;i<4;++i)assert(draw_cutaway(views[i])!=sounding[i]);

 note(*engine,0,57,true);
 assert(render(*engine,4800,visual)>.001f);
 engine->visual_focus(1);
 note(*engine,1,67,true);
 assert(render(*engine,4800,visual)>.001f);
 assert(visual.chimera_busy && visual.chimera.gate==0);
 note(*engine,0,57,false);
 render(*engine,4800,visual);
 assert(!visual.chimera_busy && render(*engine,4800,visual)==0);

 note(*engine,1,67,true);
 assert(render(*engine,4800,visual)>.001f && visual.chimera.gate>.2f);
 assert(engine->submit({CommandType::Panic,0,0,0}));
 assert(render(*engine,4800,visual)==0 && visual.chimera.gate==0);

 // Sweep the full physical control extremes with the firmware's allocator.
 // A port that tries to grow any workspace during rendering fails here.
 ChimeraVoice direct;
 direct.init();assert(direct.ready());direct.trigger(0);
 for(unsigned shape=0;shape<32;++shape){
  float controls[5];for(unsigned p=0;p<5;++p)controls[p]=float((shape>>p)&1);
  for(unsigned i=0;i<2048;++i)assert(std::isfinite(direct.next(196,.9f,true,controls)));
 }
 std::puts("Chimera: project round trip, note lifecycle, shared mono voice, live right-screen cutaway on four pages and articulation sweep passed");
}
