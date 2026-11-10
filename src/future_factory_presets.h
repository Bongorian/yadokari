#pragma once
#include "project.h"
namespace groove {
constexpr unsigned kFutureFactoryPresets=22;
inline const char* future_factory_preset_name(unsigned i){static const char* names[]={"WT Glass Motion","WT Reed Drift","WT Digital Sweep","WT Soft Choir","VA Wide Saw","VA Pulse Bass","VA PWM Keys","VA Slow Brass","FM6 Electric Piano","FM6 Wineglass","FM6 Metal Pad","ADD Organ","ADD Moving Glass","PD Pulse Bass","PD Curved Pad","Layer Glass Duo","Daisy Analog Kick","Daisy Analog Snare","Daisy Analog Hat","Daisy Synth Kick","Daisy Synth Snare","Daisy Synth Hat"};return i<kFutureFactoryPresets?names[i]:"";}
inline bool make_future_factory_preset(ProjectState& p,unsigned index){if(index>=kFutureFactoryPresets)return false;make_empty_project(p);p.content_kind=ContentKind::Preset;auto t=track_at(p,0,0);t.preset_identity=builtin_factory_identity(0x100+index);const char* name=future_factory_preset_name(index);unsigned n=0;while(name[n] && n<95){t.preset_name[n]=name[n];++n;}t.preset_name[n]=0;
 if(index<4){t.instrument=Instrument::Wavetable;auto& v=t.wavetable.values;v[0]=index==0?27:index==1?40:index==2?88:48;v[1]=index==2?14:3;v[2]=index==0?.7f:index==1?.2f:index==2?1.6f:.1f;v[3]=index==3?.35f:.005f;v[4]=index==0?1.2f:index==3?2:.3f;v[6]=index==0?.4f:.7f;}
 else if(index<8){t.instrument=Instrument::VirtualAnalog;auto& v=t.virtual_analog.values;
  if(index==4){v[0]=v[1]=0;v[2]=18;v[4]=6000;v[6]=.1f;v[7]=0;v[14]=.5f;v[15]=.5f;}
  if(index==5){v[0]=1;v[1]=0;v[2]=4;v[4]=500;v[5]=.55f;v[6]=.4f;v[7]=0;v[8]=.35f;v[9]=0;v[12]=.18f;v[13]=.2f;v[14]=.1f;v[15]=3;}
  if(index==6){v[0]=v[1]=1;v[2]=9;v[4]=3800;v[9]=.28f;v[10]=.45f;v[12]=.6f;v[13]=.45f;v[14]=.9f;v[15]=1;}
  if(index==7){v[0]=v[1]=0;v[2]=12;v[4]=750;v[5]=.3f;v[7]=.04f;v[11]=.4f;v[12]=.8f;v[13]=.8f;v[14]=1.2f;v[15]=3.5f;}
 }
 else if(index<11){t.instrument=Instrument::Fm6;t.fm6.values[0]=index==8?32:index==9?23:74;if(index==10){t.fm6.values[2]=2;t.fm6.values[3]=2;}}
 if(index==11 || index==12){t.instrument=Instrument::Additive;auto& v=t.additive.values;if(index==11){v[0]=.85f;v[1]=.1f;v[8]=.12f;for(unsigned i=0;i<16;++i)v[9+i]=i<5?1:0;}else{v[2]=5;v[3]=2;v[4]=.7f;v[5]=.15f;v[8]=1.2f;}}
 if(index==13 || index==14){t.instrument=Instrument::PhaseDist;auto& v=t.phase_dist.values;v[0]=index==13?2:3;v[2]=.5f;v[3]=index==13?0:.6f;v[5]=index==13?.005f:.4f;v[7]=index==13?.2f:.8f;v[8]=index==13?.1f:1.4f;}
 if(index==15){t.instrument=Instrument::Layer;t.layer.count=2;auto& a=t.layer.parts[0];a.sound.instrument=Instrument::Additive;a.sound.additive.values[4]=.3f;a.sound.additive.values[8]=1;a.pan=-.35f;a.detune=-7;auto& b=t.layer.parts[1];b.sound.instrument=Instrument::PhaseDist;b.sound.phase_dist.values[0]=3;b.sound.phase_dist.values[5]=.1f;b.sound.phase_dist.values[8]=1.2f;b.pan=.35f;b.detune=7;}
 if(index>=16){const unsigned voice=(index-16)%3;t.instrument=index<19?Instrument::DaisyAnalogDrums:Instrument::DaisySyntheticDrums;t.engine_parameters[0]=float(voice)*.5f;t.engine_parameters[1]=.5f;t.engine_parameters[2]=voice==2?.8f:.55f;t.engine_parameters[3]=voice==2?.18f:.45f;t.engine_parameters[4]=.9f;t.root_note=voice?60:48;t.level=voice==2?.4f:.65f;}
 return valid_project(p);
}
}
