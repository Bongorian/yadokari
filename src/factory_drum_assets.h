#pragma once
#include "project.h"
#include "drum_gm.h"
#include "file_access.h"
#include "wav_writer.h"
namespace groove {
// Original procedural PCM, generated on the file-service owner before audio
// starts. Three distinct kits; no downloads, opaque binary blobs or live input.
constexpr unsigned kFactoryKitCount=3,kFactoryKitPads=6;
inline const char* factory_kit_name(unsigned kit){const char* n[]={"Circuit","Dust","Wood"};return n[kit%3];}
inline const char* factory_drum_name(unsigned pad){const char* n[]={"Kick","Snare","Closed-Hat","Open-Hat","Clap","Tom"};return n[pad%6];}
inline unsigned factory_drum_frames(unsigned pad){return pad==3?24000:pad==0?19200:pad==1?14400:pad==2?4800:12000;}
inline bool factory_drum_path(char* out,unsigned size,unsigned kit,unsigned pad){char name[80];if(!copy_text(name,sizeof name,factory_kit_name(kit)))return false;unsigned n=0;while(name[n])++n;name[n++]='-';if(!copy_text(name+n,sizeof name-n,factory_drum_name(pad)))return false;while(name[n])++n;if(!copy_text(name+n,sizeof name-n,".wav"))return false;return join_path(out,size,"FACTORY/DRUMS",name);}
inline unsigned factory_drum_pcm(uint8_t* out,unsigned kit,unsigned pad){
 unsigned frames=factory_drum_frames(pad);wav_header_ch(out,frames,48000,1);uint32_t rng=0x85ebca6bu+kit*8179+pad*3571;float previous=0,low=0,phase=0,peak=0;
 // Store quantized, unnormalized samples, then normalize once. Peak target is
 // shared; pad levels and velocity, rather than hidden generator gains, mix kits.
 for(unsigned i=0;i<frames;++i){float t=float(i)/48000;rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;float noise=float(int32_t(rng>>16)-32768)/32768;low+=(noise-low)*(kit==1?.24f:.65f);float high=(noise-previous)*.5f;previous=noise;float x=0;
  if(pad==0){float hz=(kit==0?48:kit==1?56:65)+(kit==2?95:150)*expf(-t*(kit==2?75:48));phase+=6.2831853f*hz/48000;x=.68f*sinf(phase)*expf(-t*(kit==1?22:13))+.11f*high*expf(-t*190);}
  else if(pad==1){float body=sinf(6.2831853f*(kit==0?182:kit==1?155:208)*t);x=.3f*body*expf(-t*32)+.52f*(kit==1?low:noise)*expf(-t*(kit==0?23:kit==1?34:20));}
  else if(pad==2 || pad==3){float metal=0;const float ratios[]={1,1.342f,1.913f,2.517f};for(float r:ratios)metal+=(sinf(6.2831853f*(kit==2?3270:4100)*r*t)>0?1.0f:-1.0f)*.10f;x=(high*.5f+metal*(kit==1?.25f:.65f))*expf(-t*(pad==2?(kit==2?70:100):(kit==1?20:11)));}
  else if(pad==4){float burst=t<.026f?(fmodf(t,.009f)<.004f?1.0f:.15f):expf(-(t-.026f)*32)*.55f;x=(kit==1?low:high)*burst*.75f;}
  else{x=.65f*sinf(6.2831853f*(kit==2?172:kit==1?118:230)*t+2*expf(-t*60))*expf(-t*21)+.08f*noise*expf(-t*70);}
  x*=fminf(1,float(frames-i)/96);int16_t v=int16_t(x*24000);out[44+2*i]=uint8_t(v);out[45+2*i]=uint8_t(uint16_t(v)>>8);peak=fmaxf(peak,fabsf(float(v)));
 }
 float gain=peak>0?25558/peak:0;for(unsigned i=0;i<frames;++i){int16_t v=int16_t(uint16_t(out[44+2*i])|(uint16_t(out[45+2*i])<<8));v=int16_t(float(v)*gain);out[44+2*i]=uint8_t(v);out[45+2*i]=uint8_t(uint16_t(v)>>8);}
 return 44+frames*2;
}
inline void factory_assign_kit(ProjectState& p,TrackView tr,unsigned kit){
 for(unsigned pad=0;pad<16;++pad)tr.kit.pads[pad].gm_note=kFactoryGmNotes[pad];
 tr.instrument=Instrument::DrumKit;tr.root_note=60;tr.level=.62f;
 const float levels[]={.8f,.72f,.46f,.36f,.48f,.65f};
 for(unsigned pad=0;pad<6;++pad){auto& asset=p.assets[pad];factory_drum_path(asset.path,sizeof asset.path,kit,pad);asset.metadata.frames=factory_drum_frames(pad);asset.metadata.rate=48000;asset.metadata.channels=1;asset.metadata.bits=16;asset.metadata.root=60;
  auto& voice=tr.kit.pads[pad];voice.sample[0]=uint8_t(pad);voice.level=levels[pad];voice.edit.choke=pad==2 || pad==3?1:0;voice.edit.fade_ms=.3f;voice.pan=pad==2?-.15f:pad==3?.18f:pad==5?.26f:0;voice.reverb_send=pad==1 || pad==4?.12f:0;
 }
}
}
