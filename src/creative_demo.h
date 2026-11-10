#pragma once
#include "project.h"
namespace groove {
inline void make_creative_demo(ProjectState& p){
 make_empty_project(p);p.tempo=112;p.effects.delay_mix=0.3f;p.effects.delay_feedback=0.3f;
 const char* path="FACTORY/DEBUG/demo-slices.wav";for(unsigned i=0;path[i];++i)p.assets[0].path[i]=path[i];
 for(unsigned pattern=0;pattern<2;++pattern){for(unsigned t=0;t<5;++t){auto v=track_at(p,pattern,t);static constexpr Instrument kinds[]={Instrument::WaveSequence,Instrument::ChipClock,Instrument::TapeMicrosound,Instrument::WestCoast,Instrument::SpectralFreeze};v.instrument=kinds[t];v.root_note=uint8_t(t==1?48:60);v.level=t==4?0.28f:0.52f;v.delay_send=0.15f;v.pan=(float(t)-2)*0.18f;v.engine_parameters[0]=float(t==2?13000:t==4?2500:8000)/16383.0f;v.engine_parameters[1]=float(t==1?16000:8500)/16383.0f;v.engine_parameters[3]=float(t==4?14000:10000)/16383.0f;v.engine_parameters[4]=float(t==2?0:7500)/16383.0f;v.creative.seed=0x471b35+t+pattern*111;v.creative.mutation=uint8_t(pattern?12:0);for(unsigned step=0;step<16;step+=4){v.steps[step].enabled=true;v.steps[step].gate=384;v.steps[step].note=uint8_t(60+(step==8?7:0));v.steps[step].velocity=uint8_t(80+(t*7)%30);}v.steps[0].locks[0]={3,0.7f,true};}
 for(unsigned d=0;d<3;++d){auto v=track_at(p,pattern,5+d);v.instrument=Instrument::DaisyAnalogDrums;v.engine_parameters[0]=float(d)*.5f;v.root_note=d?60:48;v.level=0.65f;for(unsigned s=0;s<16;++s)v.steps[s].enabled=d==0?s%4==0:d==1?s%8==4:s%2==0;}}
 for(unsigned t=0;t<16;++t){p.scenes[0].pattern_for_track[t]=0;p.scenes[1].pattern_for_track[t]=1;}for(unsigned s=0;s<5;++s){auto& sound=p.sounds[s];const auto t=track_at(p,0,s);sound.instrument=t.instrument;sound.creative=t.creative;for(unsigned i=0;i<5;++i)sound.parameters[i]=t.engine_parameters[i];}
}
}
