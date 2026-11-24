#include "engine.h"
#include "performance.h"
#include "project_codec.h"
#include <memory>
#include <cstring>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(!prepare_wavetable_bank() && !wavetable_bank_ready());allocation_admission_hook=nullptr;assert(prepare_wavetable_bank() && wavetable_bank_ready());
 ProjectCrc bank_crc;for(unsigned i=0;i<458752;++i)bank_crc.add(reinterpret_cast<const uint8_t*>(kWavetableData)[i]);assert(bank_crc.value()==0x1db3d4c9u);

 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::Wavetable;t.wavetable.values[0]=42.5f;t.wavetable.values[1]=9;t.wavetable.values[4]=.01f;
 snapshot_sound(p->sounds[255],t);t.wavetable.values[0]=80;assert(p->sounds[255].wavetable.values[0]==42.5f);mount_sound(t,p->sounds[255]);assert(t.wavetable.values[0]==42.5f);
 t.steps[0].locks[0]={engine_target(Instrument::Wavetable,0),63,true};capture_parameter_snapshot(p->morph_a[0],*p,0);p->morph_b[0]=p->morph_a[0];p->morph_b[0].engine_values[0]=100;
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource source{bytes.data,bytes.size,0};auto out=std::make_unique<ProjectState>();assert(decode_project(source,*out));assert(out->tracks[0].instrument==Instrument::Wavetable && out->tracks[0].wavetable.values[0]==42.5f && out->sounds[255].wavetable.values[1]==9);assert(out->morph_b[0].engine_values[0]==100);ChunkBuffer again;assert(encode_project_buffer(again,*out));assert(bytes.size==again.size && !__builtin_memcmp(bytes.data,again.data,bytes.size));
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[256]{};e->render(audio,0);Command note{CommandType::MidiInput,0,69,0x90};note.scalar=110;assert(e->submit(note));double energy=0;
 for(unsigned b=0;b<40;++b){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.001);
 note.value=0x80;note.scalar=0;assert(e->submit(note));for(unsigned b=0;b<20;++b)e->render(audio,128);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->knob_parameter(0)==engine_target(Instrument::Wavetable,0));assert(ui->knob(0,1000));e->render(audio,0);assert(t.wavetable.values[0]>42.5f);assert(ui->undo());e->render(audio,0);assert(t.wavetable.values[0]==42.5f);
 // Edit through the hardware page, then check the amounts reach the sounding
 // voice. Identical engines and notes isolate the edit from note-envelope motion.
 assert(ui->soft_key(3) && ui->instrument_view()==kFirstSynthDetailView);
 for(unsigned k=0;k<7;++k)assert(ui->knob_parameter(k)==engine_target(Instrument::Wavetable,WavetableWarpMode+k));
 assert(ui->knob(0,1000) && ui->knob(2,3000));e->render(audio,0);
 ui.reset();auto reference=std::make_unique<Engine>();e=std::make_unique<Engine>();
 auto baseline=std::make_unique<ProjectState>(*p);
 assert(reference->stage_project(*baseline) && e->stage_project(*p));
 reference->render(audio,0);e->render(audio,0);
 ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->soft_key(3));
 note.value=0x90;note.scalar=110;
 assert(reference->submit(note) && e->submit(note));
 float unchanged[256]{};
 for(unsigned b=0;b<32;++b){
  reference->render(unchanged,128);e->render(audio,128);assert(!std::memcmp(unchanged,audio,sizeof audio));
 }
 for(unsigned knob:{1u,3u}){
  assert(ui->knob(knob,30000));
  double difference=0;
  for(unsigned b=0;b<40;++b){
   reference->render(unchanged,128);e->render(audio,128);
   for(unsigned i=0;i<256;++i)difference+=std::abs(audio[i]-unchanged[i]);
  }
  assert(difference>1);
  assert(ui->undo());reference->render(unchanged,128);e->render(audio,128);
 }
 // The complete bank and mip interpolation remain finite over note/scan extremes.
 WavetableVoice voice;voice.init(48000);WavetableState state;state.values[1]=63.5f;state.values[0]=127;state.values[4]=.005f;
 for(float hz:{20.f,440.f,12000.f,20000.f}){voice.trigger(hz,1,state.values);for(unsigned n=0;n<2000;++n)assert(__builtin_isfinite(voice.next(hz,n<500)));assert(!voice.active());}
 // Upper-register qualification must retain audible warp and PM control.
 auto high_difference=[](const WavetableState& a,const WavetableState& b){
  WavetableVoice x,y;x.init(48000);y.init(48000);
  x.trigger(3200,1,a.values);y.trigger(3200,1,b.values);
  double energy=0;
  for(unsigned n=0;n<4096;++n){const double d=x.next(3200,true)-y.next(3200,true);energy+=d*d;}
  return energy;
 };
 WavetableState upper;upper.values[0]=42.25f;upper.values[7]=1;
 for(unsigned mode=0;mode<4;++mode){
  upper.values[WavetableWarpMode]=float(mode);upper.values[WavetableWarpAmount]=.04f;
  auto changed=upper;changed.values[WavetableWarpAmount]=.7f;assert(high_difference(upper,changed)>.01);
 }
 upper.values[WavetablePmAmount]=.55f;
 auto ratio=upper;ratio.values[WavetablePmRatio]=1.63f;assert(high_difference(upper,ratio)>.01);
 upper.values[WavetablePmAmount]=0;auto bright=upper;bright.values[7]=0;
 assert(high_difference(upper,bright)>.001);
 // The mono branch remains sample-exact; stereo uses symmetric detuning,
 // with real left/right separation only when spread is nonzero.
 for(unsigned count=1;count<=7;++count)for(float spread:{0.f,1.f}){
  float sum_l=0,sum_r=0;const float gain=1.f/sqrtf(float(count));
  for(unsigned i=0;i<count;++i){
   const auto tap=wavetable_unison_tap(count,i,35,spread,gain);
   const auto mirror=wavetable_unison_tap(count,count-1-i,35,spread,gain);
   assert(fabsf(tap.pitch*mirror.pitch-1)<1e-6f && tap.left==mirror.right);
   if(i==count-1-i)assert(tap.pitch==1);
   sum_l+=tap.left*tap.left;sum_r+=tap.right*tap.right;
  }
  assert(fabsf(sum_l-1)<1e-6f && fabsf(sum_r-1)<1e-6f);
  WavetableState preset;preset.values[WavetableUnisonCount]=float(count);
  preset.values[WavetableDetune]=35;preset.values[WavetableSpread]=spread;
  WavetableVoice mono,primary;WavetableUnison bank;mono.init(48000);primary.init(48000);
  mono.trigger(440,1,preset.values);primary.trigger(440,1,preset.values);
  bank.parameters(primary,preset.values,count);
  double stereo=0,energy=0;
  for(unsigned i=0;i<4096;++i){
   float l,r;bank.next(primary,440,true,l,r);const float raw=mono.next(440,true);
   if(count==1)assert(l==raw && r==raw);
   stereo+=(l-r)*(l-r);energy+=l*l+r*r;
  }
  assert(energy>.01 && std::isfinite(energy));
  if(count>1 && spread>0)assert(stereo>.01);else assert(stereo==0);
 }
 std::printf("Wavetable: bank/render/release, scoped UI/Undo, pool independence, preset/Track/Morph codec passed; energy %.6f\n",energy);
}
