#pragma once
#include "project.h"
#include "file_access.h"
namespace groove {
inline constexpr unsigned kSynthDrumCount=6;
inline const char* synth_drum_name(unsigned i){static const char* names[]={"VA Deep Kick","FM Metal Snare","FM Tick Hat","VA Air Hat","FM Glass Rim","VA Round Tom"};return i<kSynthDrumCount?names[i]:"";}
inline unsigned synth_drum_frames(unsigned i){constexpr unsigned frames[]={19200,14400,4800,24000,12000,12000};return frames[i%kSynthDrumCount];}
inline bool make_synth_drum_preset(ProjectState& p,unsigned i){
 if(i>=kSynthDrumCount)return false;
 make_empty_project(p);p.content_kind=ContentKind::Preset;p.random_seed=0x53445231+i;
 auto t=track_at(p,0,0);t.preset_identity=builtin_factory_identity(0x400+i);copy_text(t.preset_name,96,synth_drum_name(i));t.level=.75f;t.delay_send=t.reverb_send=0;
 constexpr uint8_t notes[]={36,55,100,60,76,48};t.root_note=notes[i];
 if(i==0 || i==3 || i==5){t.instrument=Instrument::VirtualAnalog;auto& v=t.virtual_analog.values;v[0]=v[1]=i==3?2:3;v[2]=0;v[3]=0;v[4]=i==3?7500:2200;v[5]=.12f;v[6]=i==0?.25f:0;v[7]=i==3?1:0;v[9]=0;v[11]=.001f;v[12]=i==3?.16f:i==5?.14f:.2f;v[13]=0;v[14]=.025f;v[15]=0;v[20]=i==0?24:i==5?12:0;v[21]=i==0?.035f:.06f;if(i==3){v[23]=1;v[24]=1;}}
 else{t.instrument=Instrument::FM;t.engine_parameters[0]=i==1?.71f:i==2?.96f:.83f;t.engine_parameters[1]=i==1?.86f:i==2?.95f:.64f;}
 return valid_project(p);
}
}
