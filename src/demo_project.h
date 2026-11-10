#pragma once
#include "project.h"
namespace groove {
// Original repeatable session, shared by desktop and offline demos.
inline void make_demo(ProjectState& p,bool include_sample=true) {
 p.tempo=120;
 if(include_sample){const char* name="FACTORY/DEBUG/demo-slices.wav";unsigned i=0;do{p.assets[0].path[i]=name[i];}while(name[i++]);}
 for(unsigned pattern=0;pattern<kPatterns;++pattern) {
  TrackRange tracks{p,pattern};
  for(unsigned d=0;d<3;++d){tracks[d].instrument=Instrument::DaisyAnalogDrums;tracks[d].engine_parameters[0]=float(d)*.5f;tracks[d].root_note=d?60:48;const char* role=d==0?"Daisy Kick":d==1?"Daisy Snare":"Daisy Hat";unsigned n=0;do{tracks[d].track_name[n]=role[n];}while(role[n++]);}
  tracks[1].cutoff=0.35f;tracks[1].level=0.5f;tracks[2].level=0.18f;
  for(unsigned step=0;step<16;++step) {tracks[0].steps[step].enabled=step%4==0;tracks[1].steps[step].enabled=step%8==4;tracks[2].steps[step].enabled=step%2==0;}
  tracks[15].instrument=Instrument::DaisySynth;
  if(include_sample){tracks[5].instrument=Instrument::Sampler;tracks[5].level=0.35f;tracks[5].sample_slot=0;tracks[5].sample_slices=4;tracks[5].sample_slice=0;tracks[5].steps[5].locks[0]={301,float(pattern%4),true};tracks[5].steps[13].locks[0]={301,float(pattern%4),true};tracks[5].steps[5].enabled=true;tracks[5].steps[13].enabled=true;}
  tracks[3].instrument=Instrument::Acid;tracks[3].root_note=36;tracks[3].cutoff=0.25f;tracks[3].delay_send=0.2f;tracks[3].length=pattern&1?15:16;
  tracks[4].instrument=Instrument::FM;tracks[4].root_note=60;tracks[4].level=0.6f;tracks[4].delay_send=0.4f;tracks[4].length=pattern&1?7:16;
  const uint8_t notes[]={60,63,67,70};
  for(unsigned step=0;step<16;step+=4) {tracks[3].steps[step].enabled=true;tracks[3].steps[step].note=notes[(step/4+pattern)%4];tracks[4].steps[step+2].enabled=true;tracks[4].steps[step+2].note=notes[(step/4+pattern)%4];}
  tracks[3].steps[8].locks[0]={3,0.08f,true};tracks[2].steps[14].probability=60;
  for(unsigned t=0;t<kTracks;++t) {p.scenes[pattern].pattern_for_track[t]=uint8_t(pattern);p.morph_a[t].values[0]=tracks[t].level;p.morph_b[t].values[0]=tracks[t].level*0.65f;p.morph_b[t].values[2]=0.15f;p.morph_b[t].values[3]=0.25f;p.morph_b[t].values[4]=0.5f;}
 }
 for(unsigned t=0;t<kTracks;++t){capture_engine_morph(p.morph_a[t],track_at(p,p.clips[t].pattern,t));capture_engine_morph(p.morph_b[t],track_at(p,p.clips[t].pattern,t));}
 p.effects.delay_mix=0.3f;p.effects.delay_feedback=0.35f;for(unsigned t=0;t<16;++t)for(unsigned i=8;i<kCommonMorphParameters;++i)p.morph_a[t].values[i]=p.morph_b[t].values[i]=parameter_base(p,track_at(p,p.clips[t].pattern,t),morph_parameter_at(i));
}
// The same playable project is accepted by desktop, WAV renderer and Circle.
inline void make_generative_demo(ProjectState& p,bool include_sample=true){
 make_demo(p,include_sample);
 for(unsigned pattern=0;pattern<kPatterns;++pattern){
  TrackRange tracks{p,pattern};auto fm=tracks[4];fm.length=7;fm.lanes_enabled=true;fm.scale_mask=0x0ab5;fm.root_note=48;
  fm.lanes[0].length=5;const uint8_t notes[]={60,62,67,69,72};for(unsigned i=0;i<5;++i)fm.lanes[0].values[i]=notes[(i+pattern)%5];fm.lanes[0].duration[2]=2;
  fm.lanes[1].length=3;fm.lanes[1].values[0]=86;fm.lanes[1].values[1]=112;fm.lanes[1].values[2]=96;
  fm.lanes[2].length=4;fm.lanes[2].values[0]=24;fm.lanes[2].values[1]=48;fm.lanes[2].values[2]=72;fm.lanes[2].values[3]=36;
  fm.accumulator.pitch=1;fm.accumulator.minimum=0;fm.accumulator.maximum=4;fm.accumulator.mode=AccumulatorMode::Bounce;fm.accumulator.reset=8;fm.accumulator.scale_degrees=true;
  for(unsigned i=0;i<16;++i)tracks[2].steps[i].enabled=(i*5)%16<5;
  tracks[2].groove.preset=3;tracks[2].groove.amount=80;tracks[1].groove.preset=1;tracks[1].groove.amount=65;
  tracks[3].length=15;tracks[3].steps[12].ratchet=2;tracks[3].steps[12].gate=36;
  tracks[14]=fm;tracks[14].instrument=Instrument::ExternalMidi;tracks[14].midi_channel=10;
 }
 p.chain.length=4;p.chain.bars=2;p.chain.enabled=true;p.chain.mode=FollowMode::NoRepeat;p.chain.probability=85;
 for(unsigned i=0;i<4;++i)p.chain.scenes[i]=uint8_t(i);
}
}
