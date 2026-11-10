#pragma once
#include "project.h"
#include "file_access.h"
#include "update/sha256.h"
namespace groove {
constexpr unsigned kBenchmarkRevision=19; // Factory/debug asset namespace; new file identity, no comparison with revision18.
static constexpr uint8_t kBenchmarkFileSha256[32]={0xcc, 0x91, 0xc9, 0xaa, 0x73, 0x44, 0x9b, 0x9e, 0x93, 0x78, 0x99, 0xe2, 0xe2, 0x5b, 0xd8, 0x1c, 0x8c, 0x13, 0x37, 0x1a, 0x36, 0xbe, 0xa1, 0x10, 0x48, 0x17, 0x6f, 0x10, 0x1b, 0xc9, 0x74, 0x45};
constexpr uint64_t kBenchmarkFileBytes=1295533;
inline bool benchmark_file_matches(IFileAccess& fs,const char* name){
 const uint64_t size=fs.size(name);if(size!=kBenchmarkFileBytes)return false;
 update::Sha256 hash;FileScanBuffer storage;uint8_t* buffer=storage.data();if(!buffer)return false;uint64_t offset=0;
 while(offset<size){unsigned n=unsigned(size-offset>kFileScanBufferBytes?kFileScanBufferBytes:size-offset),got=0;if(!fs.read(name,offset,buffer,n,got)||got!=n)return false;hash.add(buffer,n);offset+=n;}
 uint8_t digest[32];hash.finish(digest);return update::equal(digest,kBenchmarkFileSha256,32);
}
// High-load 16-bar performance score. It uses the known demo-slices asset so
// an Ethernet firmware update can install the score before the new SD factory
// library is present. This migrated score is not a qualified performance baseline.
inline void make_benchmark_project(ProjectState& p) {
 make_empty_project(p);
 p.tempo=140;p.random_seed=0x30390943;
 const char* sample="FACTORY/DEBUG/demo-slices.wav";
 for(unsigned i=0;;++i){p.assets[0].path[i]=sample[i];if(!sample[i])break;}
 p.assets[0].metadata.rate=48000;p.assets[0].metadata.frames=48000;
 p.assets[0].metadata.bits=16;p.assets[0].metadata.channels=1;
 p.assets[0].metadata.content_crc=0x7c7e7b61u;
 const Instrument kinds[16]={Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,Instrument::DaisyAnalogDrums,
  Instrument::Sampler,Instrument::Acid,Instrument::FM,Instrument::PlaitsMacro,
  Instrument::PlaitsMacro,Instrument::PlaitsMacro,Instrument::DaisySynth,
  Instrument::DaisyAnalogDrums,Instrument::DaisySyntheticDrums,
  Instrument::PolySaw,Instrument::ChipClock,Instrument::WestCoast,
  Instrument::PlaitsMacro};
 const uint8_t chords[4][4]={{48,51,55,58},{44,48,51,55},{51,55,58,62},{46,50,53,57}};
 const uint8_t bass[4][4]={{36,36,43,39},{32,32,39,36},{39,39,46,43},{34,34,41,38}};
 for(unsigned scene=0;scene<16;++scene){
  p.chain.scenes[scene]=uint8_t(scene);
  for(unsigned t=0;t<16;++t){
   p.scenes[scene].pattern_for_track[t]=uint8_t(scene);
   auto track=track_at(p,scene,t);
   if(!scene){track.instrument=kinds[t];
   track.plaits_model=uint8_t((t*5)%24);
   if(t<3)track.engine_parameters[0]=float(t)*.5f;
   track.root_note=t==4?36:t==5?36:uint8_t(chords[scene%4][0]);
   track.level=t==0?0.35f:t==1?0.17f:t==2?0.10f:t==3?0.21f:t==4?0.16f:t==5?0.08f:t>=10&&t<=11?0.11f:0.065f;
   track.pan=float(int(t%5)-2)*0.14f;
   track.cutoff=t==4?0.50f:0.78f;track.delay_send=t>=6?0.10f:0.02f;track.reverb_send=t>=7?0.13f:0.04f;
   track.sample_slot=0;track.sample_slices=4;track.sample_slice=0;
   if(t==3){track.sample_edit.slice_count=4;for(unsigned i=0;i<=4;++i)track.sample_edit.markers[i]=float(i)/4;track.sample_edit.fade_ms=1;track.sample_edit.crossfade_ms=1;}
   if(t==4){track.engine_parameters[0]=float(6900)/16383.0f;track.engine_parameters[1]=float(12000)/16383.0f;track.engine_parameters[3]=float(10500)/16383.0f;}
   if(t==7 || t==12)track.engine_parameters[3]=float(10500)/16383.0f;}

   for(unsigned step=0;step<16;++step){
    auto& hit=track.steps[step];
    bool active=t==0?(step%4==0 || (scene%4==3 && step==14)):
     t==1?(step==4 || step==12 || (scene%4==3 && step==15)):
     t==2?step%2==0:t==3?(step==2 || step==6 || step==10 || step==14):
     t==4?(step==0 || step==3 || step==6 || step==8 || step==11 || step==14):
     t==5?(step==0 || step==7 || step==10):
     t==6?(step==0 || step==4 || step==8 || step==12):
     t==7?(step==0 || step==8):
     t==8?(step==3 || step==11):
     t==9?(step==0 || step==6 || step==12):
     t==10?step%4==0:t==11?(step==5 || step==13):
     t==12?(step==0 || step==8):t==13?(step==7 || step==15):
     t==14?(step==3 || step==11):(step==2 || step==10);
    hit.enabled=active;
    hit.note=t==4?bass[scene%4][(step/4)%4]:t==5?uint8_t(bass[scene%4][(step/4)%4]-12):t>=6&&t<=9?uint8_t(chords[scene%4][(step/4+t)%4]+(t==8?12:0)):t==12?uint8_t(chords[scene%4][(step/8)%4]+12):uint8_t(t==4?36:t==5?36:chords[scene%4][0]);
    hit.velocity=uint8_t(82+((step*3+t+scene)%5)*8);hit.probability=100;hit.gate=t==7||t==12?192:72;
    if(t==6 && active){hit.chord[0]=uint8_t(hit.note+4);hit.chord[1]=uint8_t(hit.note+7);hit.chord[2]=uint8_t(hit.note+11);}
    if(t==3 && active){hit.locks[0]={301,float((step/4+scene)%4),true};}
    if(t==4 && active){hit.locks[0]={3,0.30f+0.08f*float((step+scene)%5),true};}
   }
  }
 }
 p.chain.length=16;p.chain.bars=1;p.chain.enabled=true;p.chain.mode=FollowMode::Sequential;p.chain.probability=100;
 p.effects.delay_mix=0.17f;p.effects.delay_feedback=0.34f;p.effects.reverb_mix=0.18f;p.effects.reverb_decay=0.61f;
}
// A transient stress score; hardware load qualification is currently deferred.
inline void make_plaits_stress_project(ProjectState& p,unsigned tempo=120){
 make_benchmark_project(p);p.tempo=uint16_t(tempo);
 for(unsigned scene=0;scene<16;++scene)for(unsigned t=0;t<16;++t){
  auto track=track_at(p,scene,t);if(!scene){track.instrument=Instrument::PlaitsMacro;track.plaits_model=uint8_t(t%24);track.level=0.025f;}
  for(unsigned step=0;step<16;++step)track.steps[step].enabled=(step%4)==0;
 }
}

// Transient mixed workload: the same cached sample feeds the shared voice
// pool, while eight synths, pad inserts and two track inserts are active.
// The caller changes morph position during the run; this score is not saved.
inline void make_fx_stress_project(ProjectState& p){
 make_benchmark_project(p);p.tempo=140;p.chain.enabled=false;for(auto& phrase:p.patterns[0].tracks)phrase.sound_override=kNoPreset;p.effects.delay_mix=0.15f;p.effects.reverb_mix=0.15f;
 for(unsigned g=0;g<4;++g){auto& bus=p.routing.groups[g];bus.members=uint16_t(15u<<(4*g));bus.insert_fx[0]=float(g==3?13:16+g);bus.insert_fx[1]=0.4f;bus.insert_fx[2]=0.3f;bus.insert_fx[3]=0.5f;bus.send[0]=0.12f;bus.send[1]=0.1f;bus.parallel=g==0?0.7f:g==1?0.4f:0;}
 for(unsigned t=0;t<16;++t){auto track=track_at(p,0,t);track.level=0.025f;track.length=16;
  track.insert_fx[0]=t<8?1.0f:float(t<13?t+2:t+3);track.insert_fx[1]=0.7f;track.insert_fx[2]=0.2f;track.insert_fx[3]=0.5f;
  track.insert_fx[4]=5;track.insert_fx[5]=0.25f;track.insert_fx[7]=0.3f;track.delay_send=0.1f;track.reverb_send=0.1f;
  if(t<8){track.instrument=Instrument::DrumKit;
   for(auto& step:track.steps)step.enabled=false;
   for(unsigned pad=0;pad<4;++pad){auto& sound=track.kit.pads[pad];sound.sample[0]=0;sound.level=0.5f;sound.delay_send=0.15f;sound.reverb_send=0.1f;sound.insert_fx[0]=float(16+pad%3);sound.insert_fx[1]=0.45f;sound.insert_fx[2]=0.2f;sound.insert_fx[3]=0.6f;
    for(unsigned step=0;step<16;++step){auto& hit=track.kit.steps[pad][step];hit.enabled=step%4==pad;hit.velocity=90;hit.fx_lock[1]=(step%8<4)?48:80;}}
  }else{track.instrument=Instrument::PlaitsMacro;track.plaits_model=uint8_t((t*3)%24);for(unsigned step=0;step<16;++step){auto& hit=track.steps[step];hit.enabled=step%4==0;hit.velocity=90;hit.gate=90;}}
 }
 for(unsigned t=0;t<16;++t){capture_parameter_snapshot(p.morph_a[t],p,t);p.morph_b[t]=p.morph_a[t];}
 for(unsigned t=0;t<8;++t)for(unsigned pad=0;pad<4;++pad){p.morph_b[t].values[morph_index(uint16_t(320+pad*4+2))]=0.65f;p.morph_b[t].values[morph_index(kit_mix_parameter_id(pad,0))]=0.3f;p.morph_b[t].values[morph_index(kit_mix_parameter_id(pad,2))]=0.4f;p.morph_b[t].values[morph_index(kit_mix_parameter_id(pad,3))]=0.3f;}
 p.routing.groups[0].insert_fx[4]=11;p.routing.groups[1].insert_fx[4]=15;for(unsigned t=0;t<16;++t){p.morph_a[t].values[morph_index(416)]=p.morph_b[t].values[morph_index(416)]=11;p.morph_a[t].values[morph_index(420)]=p.morph_b[t].values[morph_index(420)]=15;}p.morph_b[0].values[morph_index(385)]=0.8f;p.morph_b[0].values[morph_index(448)]=0.4f;p.morph_b[0].values[morph_index(464)]=0.9f;track_at(p,0,0).steps[0].locks[0]={385,0.3f,true};track_at(p,0,0).steps[4].locks[0]={448,0.5f,true};track_at(p,0,0).steps[8].locks[0]={464,0.2f,true};p.macros[0].active=true;p.macros[0].value=0.5f;p.macros[0].routes[0]={true,390,0xffff,0.2f,0.6f,LayerMode::Replace};
 // Exercise a parameter-only trigger under the same live load. The shared
 // performance bus is automated only on this otherwise silent synth step;
 // manual stress FX still take priority until released.
 auto& parameter_trigger=track_at(p,0,8).steps[1];parameter_trigger.enabled=false;parameter_trigger.lock_only=true;parameter_trigger.locks[0]={472,1,true};parameter_trigger.locks[1]={473,0.7f,true};
 // One live macro independently moves all sixteen track filters.
 auto& stress_macro=p.macros[1];stress_macro.active=true;stress_macro.value=0.5f;for(unsigned t=0;t<16;++t)stress_macro.routes[t]={true,3,uint16_t(1u<<t),0.35f,0.95f,LayerMode::Replace,MacroCurve(t%4)};
 p.morph_enabled=true;p.morph_position=0;
}

}

namespace groove {
// Transient hardware audition: six published DaisySP drum generators, no DATA save.
inline void make_daisy_drum_demo_project(ProjectState& p){
 make_empty_project(p);p.tempo=120;
 for(unsigned t=0;t<16;++t){auto track=track_at(p,0,t);track.length=16;for(auto& step:track.steps)step.enabled=false;}
 for(unsigned t=0;t<6;++t){
  auto track=track_at(p,0,t);track.instrument=t<3?Instrument::DaisyAnalogDrums:Instrument::DaisySyntheticDrums;
  unsigned voice=t%3;track.root_note=voice==0?48:60;track.level=voice==0?0.6f:voice==1?0.27f:0.17f;
  track.engine_parameters[0]=float(uint16_t((float(voice)/3+0.05f)*16383))/16383.0f;
  track.engine_parameters[1]=float(8192)/16383.0f;track.engine_parameters[2]=float(8192)/16383.0f;track.engine_parameters[3]=float(10000)/16383.0f;track.engine_parameters[4]=float(12000)/16383.0f;
  for(unsigned step=0;step<16;++step){bool hit=voice==0?step%4==0:voice==1?step%8==4:step%2==0;track.steps[step].enabled=hit;track.steps[step].note=track.root_note;track.steps[step].velocity=110;}
 }
}
}
