#pragma once
#include "project.h"
namespace groove {
// Native workload v1. Transient: never replaces BENCH.MGXS or writes DATA.
// Cases 0..2: 16 resonators, 4 internal voices, synchronized four-note strums.
// Cases 3..18: 16 textures, 4 modes x 4 qualities, full density/feedback/reverb.
inline void make_native_stress_project(ProjectState& p,unsigned which){
 make_empty_project(p);p.random_seed=0x20261103;p.tempo=300;
 p.routing.main=.08f;p.routing.cue=0;
 for(unsigned t=0;t<16;++t){auto tr=track_at(p,0,t);tr.level=.025f;tr.cutoff=1;tr.length=16;
  tr.instrument=which<3?Instrument::Resonator:Instrument::Texture;
  tr.resonator.values[0]=float(which%3);tr.resonator.values[1]=2;tr.resonator.values[2]=.9995f;tr.resonator.values[3]=1;tr.resonator.values[4]=1;tr.resonator.values[5]=.5f;
  tr.texture.values[0]=float(which<3?0:(which-3)/4);tr.texture.values[11]=float(which<3?0:(which-3)%4);tr.texture.values[12]=52;
  tr.texture.values[2]=1;tr.texture.values[4]=1;tr.texture.values[7]=1;tr.texture.values[8]=1;tr.texture.values[9]=1;
  for(unsigned i=0;i<16;++i){auto& st=tr.steps[i];st.enabled=i%4==0;st.note=uint8_t(36+(t%5)*12);st.velocity=100;st.probability=100;st.gate=48;st.chord[0]=st.note+4;st.chord[1]=st.note+7;st.chord[2]=st.note+11;}
 }
}
}
