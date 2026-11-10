#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdio>
using namespace groove;
struct Buffer {std::vector<uint8_t> bytes;size_t at=0;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(at+n>bytes.size())return false;__builtin_memcpy(p,bytes.data()+at,n);at+=n;return true;}bool at_end()const{return at==bytes.size();}};
int main(){
 for(unsigned i=0;i<kCommonMorphParameters;++i)assert(morph_index(morph_parameter_at(i))==i);
 assert(morph_index(300)==kCommonMorphParameters);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::PlaitsMacro;t.engine_parameters[0]=.2f;
 for(unsigned i=0;i<kTracks;++i){capture_parameter_snapshot(p->morph_a[i],*p,i);p->morph_b[i]=p->morph_a[i];}
 p->morph_b[0].engine_values[5]=2;p->morph_b[0].engine_values[0]=.8f;p->morph_enabled=true;p->morph_position=.75f;
 assert(std::fabs(resolve_parameter(*p,t,0,32,nullptr,0).value-.65f)<.00001f);
 float locked[5]={.3f,.5f,.5f,.5f,.5f};
 assert(resolve_parameter(*p,t,0,32,nullptr,0,false,0,0,locked,true,nullptr,0,nullptr,nullptr,nullptr,nullptr,nullptr,Instrument::FM).value==.3f);
 assert(resolve_target_value(*p,t,0,engine_target(t.instrument,5),0,nullptr,0).value==2); // Model cost and voice must use this same note-on value.
 for(unsigned n=0;n<3;++n){p->morph_extra_a[n][0]=p->morph_a[0];p->morph_extra_b[n][0]=p->morph_b[0];p->morph_extra_b[n][0].engine_values[0]=.7f+.05f*n;}
 p->morph_a[0].values[morph_index(384)]=3;p->morph_b[0].values[morph_index(384)]=11;
 auto prior=std::make_unique<TrackMorphState>();capture_track_morph(*prior,*p,0);
 auto engine=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*engine,*p);float scratch[2]{};
 auto flush=[&](){engine->render(scratch,0);ui->poll();};assert(engine->stage_project(*p));ui->project_loaded();flush();
 assert(ui->set_instrument(Instrument::FM));flush();assert(p->morph_a[0].engine==Instrument::FM && p->morph_b[0].engine==Instrument::FM);
 for(unsigned n=0;n<3;++n)assert(p->morph_extra_a[n][0].engine==Instrument::FM && p->morph_extra_b[n][0].engine_values[0]==.2f);
 assert(p->morph_a[0].values[morph_index(384)]==3 && p->morph_b[0].values[morph_index(384)]==11);
 assert(ui->undo());flush();assert(p->tracks[0].instrument==Instrument::PlaitsMacro && p->morph_b[0].engine_values[0]==.8f);
 for(unsigned n=0;n<3;++n)assert(p->morph_extra_b[n][0].engine_values[0]==prior->b[n+1].engine_values[0]);
 assert(ui->redo());flush();assert(p->morph_a[0].engine==Instrument::FM && p->morph_b[0].engine_values[0]==.2f);
 assert(ui->undo());flush();
 // Preset attachment follows the same owner reset and transaction Undo.
 auto sound=std::make_unique<SoundState>();sound->instrument=Instrument::FM;sound->parameters[0]=.4f;
 assert(ui->apply_sound(*sound));flush();assert(p->morph_b[0].engine_values[0]==.4f);assert(ui->undo());flush();assert(p->morph_b[0].engine_values[0]==.8f);
 auto source=std::make_unique<TrackState>(track_at(*p,0,0));source->instrument=Instrument::DaisySynth;source->daisy_adsr[0]=.02f;
 assert(ui->apply_preset_track(*source));flush();assert(p->morph_b[0].engine==Instrument::DaisySynth && p->morph_b[0].engine_values[0]==.02f);
 assert(ui->undo());flush();assert(p->tracks[0].instrument==Instrument::PlaitsMacro && p->morph_b[0].engine_values[0]==.8f);
 // Group/Master snapshots remain outside a track's undo ownership.
 p->morph_b[0].values[morph_index(384)]=7;restore_track_morph(*p,0,*prior);assert(p->morph_b[0].values[morph_index(384)]==7);
 // GUID/declaration serialization, including bounded unknown-owner retention.
 auto& unknown=p->morph_extra_a[2][4];unknown.engine=Instrument::Unknown;__builtin_memcpy(unknown.unknown_guid,"vendor.future",14);unknown.unknown_count=2;unknown.engine_values[0]=.3f;unknown.engine_values[1]=42;
 assert(valid_project(*p));Buffer encoded;assert(encode_project(encoded,*p));auto out=std::make_unique<ProjectState>();assert(decode_project(encoded,*out));
 assert(out->morph_b[0].engine==Instrument::PlaitsMacro && out->morph_b[0].engine_values[0]==.8f);
 assert(!__builtin_strcmp(out->morph_extra_a[2][4].unknown_guid,"vendor.future") && out->morph_extra_a[2][4].engine_values[1]==42);
 Buffer again;assert(encode_project(again,*out) && encoded.bytes==again.bytes);
 std::puts("Engine Morph: local declarations/model, four-set reset, command/preset/transaction Undo, global ownership and GUID round trip passed");
}
