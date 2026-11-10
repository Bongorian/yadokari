#include "engine.h"
#include "native_stress_project.h"
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
#include <cstring>
using namespace groove;
int main(int argc,char** argv){
 bool state_only=argc==2 && !std::strcmp(argv[1],"--state-only");assert(argc==1 || state_only);
 auto project=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>(48000);
 auto reference=std::make_unique<TextureVoice>();float main[96]{},input[96]{};double maximum=0;float peak=0;
 make_empty_project(*project);track_at(*project,0,0).instrument=Instrument::Texture;
 if(!state_only)for(unsigned mode=0;mode<4;++mode)for(unsigned quality=0;quality<4;++quality){
  make_empty_project(*project);project->random_seed=0x12345678;
  auto track=track_at(*project,0,0);track.instrument=Instrument::Texture;
  float case_peak=0;track.texture.values[4]=.8f;track.texture.values[0]=float(mode);track.texture.values[11]=float(quality);track.texture.values[6]=1; // Include the exact crossfade-table endpoint.
  assert(engine->stage_project(*project));engine->render(main,0);reference->init(project->random_seed);
  assert(engine->start_live_record(false,192000,19));
  for(unsigned frame=0;frame<192000;frame+=48){
   for(unsigned i=0;i<48;++i){input[2*i]=.4f*sinf(float(frame+i)*.075f);input[2*i+1]=.3f*sinf(float(frame+i)*.039f);}
   engine->render_ports(main,nullptr,input,48);
   for(unsigned i=0;i<48;++i){float l,r;reference->next(track.texture.values,input[2*i],input[2*i+1],l,r);LiveRecordFrame received;assert(engine->pop_live_record(received));
    maximum=fmax(maximum,fmax(fabs(double(received.l)-l),fabs(double(received.r)-r)));peak=fmaxf(peak,fabsf(l));case_peak=fmaxf(case_peak,fmaxf(fabsf(l),fabsf(r)));
   }
  }
  std::fprintf(stderr,"Texture mode %u quality %u peak %.6f\n",mode,quality,case_peak);assert(case_peak>0);assert(engine->live_record_done() && !engine->live_record_failed());engine->finish_live_record();
 }
 if(!state_only){std::printf("Texture Engine line-in to dry capture: 16 cases max error %.9g peak %.6f\n",maximum,peak);assert(maximum==0 && peak>0);std::fflush(stdout);}
 for(unsigned id=620;id<635;++id){auto t=track_at(*project,0,0);const auto* d=parameter_descriptor(id);assert(set_track_parameter(t,id,d->maximum));assert(track_parameter(t,id)==d->maximum);assert(parameter_at(parameter_index(id))==id);}
 snapshot_sound(project->sounds[3],track_at(*project,0,0)); // INPUT stays on Track, outside the preset.
 assert(project->sounds[3].texture.values[12]==1 && track_at(*project,0,0).texture.values[12]==60);
 ChunkBuffer encoded;assert(encode_project_buffer(encoded,*project));ChunkSource view{encoded.data,encoded.size,0};auto restored=std::make_unique<ProjectState>();assert(decode_project(view,*restored));
 for(unsigned i=0;i<15;++i){assert(track_at(*restored,0,0).texture.values[i]==track_at(*project,0,0).texture.values[i]);assert(restored->sounds[3].texture.values[i]==project->sounds[3].texture.values[i]);}
 Performance perf(*engine,*project);perf.set_mode(UiMode::Synth);assert(perf.set_instrument(Instrument::Texture));assert(perf.core_parameter(0)==engine_target(Instrument::Texture,0));
 assert(perf.set_parameter(engine_target(Instrument::Texture,0),1));perf.set_shift(true);assert(perf.knob_parameter(0)==engine_target(Instrument::Texture,0));assert(perf.knob(0,1000));assert(perf.knob_value(0)==2);perf.set_shift(false);
 assert(perf.soft_key(2));assert(perf.core_parameter(0)==engine_target(Instrument::Texture,6) && perf.knob_parameter(6)==0);
 assert(perf.soft_key(3));assert(perf.core_parameter(0)==632 && perf.core_parameter(4)==engine_target(Instrument::Texture,13));assert(!perf.knob(7,1000));
 assert(perf.soft_key(5));assert(perf.mode()==UiMode::Modulation);
 // High-cost input processors are refused explicitly, without altering saved
 // quality, mode or track count. The same DSP owner publishes the masks.
 make_native_stress_project(*project,15);assert(engine->stage_project(*project));float budget_audio[96]{};engine->render(budget_audio,48);assert(engine->native_active_mask()==1 && engine->native_refused_mask()==65534);for(const auto& t:project->tracks)assert(t.texture.values[0]==3 && t.texture.values[11]==0);
 std::puts("Texture Track/preset INPUT separation, codec and scoped controls passed");
}
