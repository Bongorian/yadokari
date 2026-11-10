#include "session_files.h"
#include "ui_renderer.h"
#include "step_expression.h"
#include "fixtures/step_plain_reference.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <memory>
#include <vector>
#include <cstdio>
#include <unistd.h>
using namespace groove;
struct Rendered {std::vector<float> pcm;std::vector<AudioVisualFrame> visuals;};
static Rendered phrase(bool slide,bool accent,unsigned gate=96,bool rest=false,TrigCondition condition=TrigCondition::Always,PlaybackDirection direction=PlaybackDirection::Forward,bool chord=false,unsigned ratchet=1,Instrument instrument=Instrument::Acid,bool loop=false,bool lanes=false,bool faded=false){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tempo=120;auto t=track_at(*p,0,0);t.instrument=instrument;t.root_note=48;t.length=loop?2:4;t.direction=direction;t.engine_parameters[3]=faded?.5f:1;
 unsigned start=direction==PlaybackDirection::Reverse?3:0,arrival=direction==PlaybackDirection::Reverse?2:1;
 auto& a=t.steps[start];a.enabled=true;a.note=60;a.gate=uint16_t(gate);a.velocity=90;a.slide(loop);
 auto& b=t.steps[arrival];b.enabled=!rest;b.note=72;b.velocity=90;b.gate=96;b.slide(slide);b.accent(accent);b.condition=condition;b.ratchet=uint8_t(ratchet);if(chord)b.chord[0]=67;
 if(lanes){t.lanes_enabled=true;for(auto& lane:t.lanes)lane.length=2;t.lanes[0].values[0]=60;t.lanes[0].values[1]=72;t.lanes[1].values[0]=t.lanes[1].values[1]=90;t.lanes[2].values[0]=uint8_t(gate);t.lanes[2].values[1]=96;b.velocity=0;}
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));assert(e->submit({CommandType::Start,0,0,0}));Rendered out;float block[128];AudioVisualFrame frame;
 for(unsigned n=0;n<(loop?(faded?700u:300u):200u);++n){e->render(block,64);out.pcm.insert(out.pcm.end(),block,block+128);while(e->pop_visual(frame))out.visuals.push_back(frame);}
 assert(e->submit({CommandType::Stop,0,0,0}));for(unsigned n=0;n<50;++n)e->render(block,64);while(e->pop_visual(frame)){}for(float x:block)assert(x==0);
 return out;
}
static double power(const std::vector<float>& x,unsigned first,unsigned last,double hz){double re=0,im=0;for(unsigned i=first;i<last;++i){double a=6.283185307179586*hz*(i-first)/48000,w=.5-.5*cos(6.283185307179586*(i-first)/(last-first-1));re+=x[i*2]*w*cos(a);im+=x[i*2]*w*sin(a);}return re*re+im*im;}
static double energy(const std::vector<float>& x,unsigned first,unsigned last){double sum=0;for(unsigned i=first*2;i<last*2;++i){assert(__builtin_isfinite(x[i]));sum+=double(x[i])*x[i];}return sum;}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 const char* expected[]={"c16ad2dc6967d3cf528a77ac632704db01bc11f963aae36853e57a112e6d4f6b","441603f8519641a7c25d847664c2a45f14b1ccecba61c021faae90feb831200d","3e2ef5cbace6b27a5b31a6d5cc578e906185cd0c4953873f77423e9105305864","532e24870a6d84492f90895c74d3e590d79289511b19a5a5978872fdd02c7ae8"};
 const Instrument engines[]={Instrument::Acid,Instrument::PolySaw,Instrument::FM,Instrument::VirtualAnalog};
 for(unsigned i=0;i<4;++i){uint8_t hash[32];render_plain_phrase(engines[i],hash);char text[65];for(unsigned j=0;j<32;++j)std::snprintf(text+j*2,3,"%02x",hash[j]);assert(!__builtin_strcmp(text,expected[i]));}puts("plain: four 96000-frame float PCM hashes match pre-change master");
 StepPitchSlide motion;motion.start(110,220,48000);float f=110;for(unsigned n=0;n<3840;++n){float next=motion.next(f);assert(next>=f && next<=220.01f);f=next;}assert(f==220 && !motion.remaining);motion.start(220,110,48000);f=220;for(unsigned n=0;n<3840;++n){float next=motion.next(f);assert(next<=f && next>=109.99f);f=next;}assert(f==110);
 auto normal=phrase(false,false),slid=phrase(true,false),short_slid=phrase(true,false,24),accent=phrase(false,true),both=phrase(true,true),rest=phrase(true,false,24,true),skip=phrase(true,false,24,false,TrigCondition::Fill),reverse=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Reverse),chord=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,true),ratchet=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,2);
 auto loop=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,Instrument::Acid,true);unsigned loop_seen=0;for(const auto& frame:loop.visuals)if(frame.sample>12200 && frame.sample<16000){assert(frame.envelope[0]<.31f);++loop_seen;}assert(loop_seen);
 auto lane_slide=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,Instrument::Acid,false,true);assert(energy(lane_slide.pcm,3000,5900)>.01);
 auto faded=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,Instrument::Acid,true,false,true);float previous=1;unsigned faded_seen=0;for(const auto& frame:faded.visuals){if(frame.sample>2000){assert(frame.envelope[0]<=previous);previous=frame.envelope[0];}if(frame.sample>23000){assert(frame.envelope[0]<.00002f);++faded_seen;}}assert(faded_seen && energy(faded.pcm,30000,43000)<1e-8);puts("long connected gate: naturally faded envelope never retriggers");
 unsigned seen=0;for(const auto& frame:slid.visuals)if(frame.sample>6200 && frame.sample<10000){assert(frame.envelope[0]<.56f && frame.note[0]==60);++seen;}assert(seen); // root48 + phrase72 -60 =60
 seen=0;for(const auto& frame:normal.visuals)if(frame.sample>6200 && frame.sample<10000){assert(frame.envelope[0]>.60f);++seen;}assert(seen);
 assert(energy(short_slid.pcm,3000,5900)>.01);assert(energy(rest.pcm,4000,5900)<1e-8 && energy(skip.pcm,4000,5900)<1e-8);assert(energy(reverse.pcm,3000,5900)>.01);
 assert(energy(chord.pcm,4000,5900)<1e-8 && energy(ratchet.pcm,4000,5900)<1e-8);
 assert(short_slid.pcm==slid.pcm); // incoming tie overrides the old gate only
 double plain=energy(normal.pcm,6200,8500),strong=energy(accent.pcm,6200,8500),shape=0;printf("accent measured energies plain=%.9f accent=%.9f\n",plain,strong);assert(plain>0 && strong>plain*1.05);
 for(unsigned i=6200*2;i<8500*2;++i){double d=accent.pcm[i]-normal.pcm[i]*1.6;shape+=d*d;}assert(shape>plain*.005);double both_difference=0;for(unsigned i=6200*2;i<8500*2;++i){double d=both.pcm[i]-slid.pcm[i];both_difference+=d*d;}printf("combined accent waveform difference %.9f; energy %.9f vs %.9f\n",both_difference,energy(both.pcm,6200,8500),energy(slid.pcm,6200,8500));assert(both_difference>.001);assert(both.visuals.size()==slid.visuals.size());for(unsigned i=0;i<both.visuals.size();++i)assert(both.visuals[i].envelope[0]==slid.visuals[i].envelope[0]);
 for(auto inst:{Instrument::PolySaw,Instrument::FM}){auto a=phrase(false,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,inst),b=phrase(true,false,24,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,inst);assert(energy(a.pcm,4000,5900)<1e-8 && energy(b.pcm,4000,5900)>.0001);}
 auto va=phrase(false,false,96,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,Instrument::VirtualAnalog),va_flags=phrase(true,true,96,false,TrigCondition::Always,PlaybackDirection::Forward,false,1,Instrument::VirtualAnalog);assert(va.pcm==va_flags.pcm);
 double low=power(slid.pcm,10000,11800,130.81278265),high=power(slid.pcm,10000,11800,261.6255653);printf("slide arrival fundamental power %.9f vs old note %.9f\n",high,low);assert(high>low);
 printf("render: env preserved, 80ms ramp, short gate tie/rest/condition/reverse/chord/ratchet/stop; accent energy %.6f -> %.6f, non-gain residual %.6f; poly fallback identical\n",plain,strong,shape);
 auto p=std::make_unique<ProjectState>(),out=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Acid;auto t=track_at(*p,0,0);t.steps[0].enabled=true;t.steps[0].attributes=3;t.steps[255].accent(true);t.mutation.frozen=true;t.mutation_source[197].slide(true);
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));assert(bytes.data[4]==kFormatVersion);ChunkSource input{bytes.data,bytes.size,0};assert(decode_project(input,*out));assert(track_at(*out,0,0).steps[0].attributes==3 && track_at(*out,0,0).steps[255].accent() && track_at(*out,0,0).mutation_source[197].slide());bytes.data[4]=10;ChunkSource old{bytes.data,bytes.size,0};assert(!decode_project(old,*out));
 char dir[]="build/step-attributes-XXXXXX";assert(mkdtemp(dir)&&chdir(dir)==0);DesktopFiles fs;assert(save_recoverable(fs,"Expression.mgxs",*p) && read_project_file(fs,"Expression.mgxs",*out));assert(track_at(*out,0,0).steps[255].accent());
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float block[2]{};e->render(block,0);auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Step);auto renderer=std::make_unique<UiRenderer>();ui->set_shift(true);assert(!__builtin_strcmp(ui->knob_label(2),"ACCENT") && !__builtin_strcmp(ui->knob_label(3),"SLIDE"));assert(ui->knob(2,-1000));assert(!t.steps[0].accent() && t.steps[0].slide());assert(ui->undo());assert(t.steps[0].attributes==3);assert(ui->knob(3,-1000));assert(!t.steps[0].slide());assert(ui->redo()==false);assert(ui->undo());assert(t.steps[0].slide());assert(ui->copy_step());ui->set_shift(false);assert(ui->encoder(1));assert(ui->selected_step()==1 && ui->selected_track()==0);assert(ui->paste_step());assert(t.steps[1].attributes==3);
 ui->set_shift(false);
 auto shapes=[&](){renderer->invalidate();renderer->draw(*p,*ui);std::vector<uint32_t> body;for(unsigned side=0;side<2;++side)for(unsigned y=88;y<188;++y)for(unsigned x=0;x<480;++x)body.push_back(renderer->pixels[side][y*480+x]);uint8_t old=t.steps[1].attributes;t.steps[1].attributes=0;renderer->draw(*p,*ui);unsigned changed=0,n=0;for(unsigned side=0;side<2;++side)for(unsigned y=88;y<188;++y)for(unsigned x=0;x<480;++x)changed+=body[n++]!=renderer->pixels[side][y*480+x];t.steps[1].attributes=old;printf("attribute body pixels changed mode=%s count=%u\n",ui->tracker_view()?"tracker":"grid",changed);assert(changed>4);};shapes();
 ui->set_shift(true);assert(ui->soft_key(3));assert(ui->soft_key(3));assert(ui->tracker_view());shapes();assert(ui->knob(2,-1000)&&ui->knob(3,-1000));assert(t.steps[1].attributes==0);assert(ui->undo()&&ui->undo());assert(t.steps[1].attributes==3);e->render(block,0);ui->poll();assert(t.steps[1].attributes==3);
 puts("storage/edit: MGXC11/PSEQ4, sparse+frozen source, old rejection, real file roundtrip, STEP+TRACKER shared ON/OFF/Undo/copy passed");
}
