#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <memory>
#include "rings/dsp/part.h"
#include "stmlib/utils/random.h"
#ifdef GROOVE_ADAPTER
#include "resonator_dsp.h"
#endif
// Identical declared stimulus; the reference executable never includes the adapter.
int main(int argc,char** argv){
 if(argc!=2)return 2;
 FILE* f=std::fopen(argv[1],"wb");if(!f)return 2;
 constexpr unsigned frames=24000, block=24;
#ifdef GROOVE_ADAPTER
 auto dsp=std::make_unique<groove::ResonatorDsp>();
#else
 auto dsp=std::make_unique<rings::Part>();
 auto reverb=std::make_unique<uint16_t[]>(32768);
#endif
 float in[block]{},out[block]{},aux[block]{},stereo[block*2];
 unsigned cases=0;
 for(unsigned model=0;model<3;++model)for(unsigned pitch=36;pitch<=84;pitch+=12)
 for(unsigned poly: {1u,2u,4u})for(unsigned sweep=0;sweep<5;++sweep){
  const uint32_t seed=0x12345678u;
#ifdef GROOVE_ADAPTER
  dsp->init(seed);
#else
  std::memset(static_cast<void*>(dsp.get()),0,sizeof *dsp);
  std::memset(reverb.get(),0,32768*sizeof(uint16_t));
  stmlib::Random::Seed(seed);dsp->Init(reverb.get());
  dsp->set_model(rings::ResonatorModel(model));if(poly!=1)dsp->set_polyphony(poly);
#endif
  for(unsigned frame=0;frame<frames;frame+=block){
   rings::Patch patch{0.5f,0.5f,0.5f,0.5f};
   float value=float(frame)/float(frames-block);
   if(sweep==1)patch.structure=value*0.9995f;
   if(sweep==2)patch.brightness=value;
   if(sweep==3)patch.damping=value;
   if(sweep==4)patch.position=value;
   // Four strums form a held resonator chord; no external amplitude envelope.
   const float intervals[]={0,4,7,12};unsigned step=frame/4800;
   rings::PerformanceState state{};state.internal_exciter=true;
   state.strum=frame%4800==0;state.internal_strum=false;state.internal_note=false;
   state.tonic=12;state.note=float(pitch)-12+intervals[step%4];
   state.fm=sweep==0?2*std::sin(float(frame)*0.0001f):0;
   state.chord=int(patch.structure*10+0.5f);
#ifdef GROOVE_ADAPTER
   dsp->process(model,poly,patch,state,in,out,aux);
#else
   dsp->Process(state,patch,in,out,aux,block);
#endif
   for(unsigned i=0;i<block;++i){if(!std::isfinite(out[i]) || !std::isfinite(aux[i]))return 3;stereo[i*2]=out[i];stereo[i*2+1]=aux[i];}
   if(std::fwrite(stereo,sizeof(float),block*2,f)!=block*2)return 4;
  }
  ++cases;
 }
 std::fclose(f);std::printf("%u cases; 3 models x 5 pitches (C2-C6) x 3 polyphony x 5 sweeps; %u frames/case\n",cases,frames);
}
