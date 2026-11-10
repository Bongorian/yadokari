#include "engine.h"
#include "synth_drum_recipes.h"
#include "project_files.h"
#include "drum_gm.h"
#include "wav_writer.h"
#include "../../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
int main(){DesktopFiles fs;const char* root="assets/factory-synth-drums";assert(fs.directory(root) && fs.directory("assets/factory-synth-drums/PRESETS") && fs.directory("assets/factory-synth-drums/DRUMS") && fs.directory("assets/factory-synth-drums/KITS"));auto p=std::make_unique<ProjectState>(),kit=std::make_unique<ProjectState>();make_empty_project(*kit);kit->content_kind=ContentKind::Kit;auto k=track_at(*kit,0,0);k.instrument=Instrument::DrumKit;k.preset_identity=builtin_factory_identity(0x410);copy_text(k.preset_name,96,"VA + FM Percussion");k.level=.7f;
 for(unsigned i=0;i<kSynthDrumCount;++i){assert(make_synth_drum_preset(*p,i));char master[192],target[192],wav[192],canonical[128];std::snprintf(master,sizeof master,"%s/PRESETS/%s.mgxp",root,synth_drum_name(i));std::snprintf(target,sizeof target,"FACTORY/PRESETS/DRUMS/%s.mgxp",synth_drum_name(i));ChunkBuffer encoded;assert(project_paths(*p,target,false) && encode_project_buffer(encoded,*p));{FileWriter writer(fs,master);assert(writer.write(encoded.data,encoded.size) && writer.finish());}
  auto e=std::make_unique<Engine>();p->effects.delay_mix=p->effects.reverb_mix=0;assert(e->stage_project(*p));float block[48]{};e->render(block,0);assert(e->submit({CommandType::Trigger,0,0,110}));const unsigned frames=synth_drum_frames(i);std::vector<float> audio(frames);float peak=0;for(unsigned at=0;at<frames;at+=24){if(at==frames-480)assert(e->submit({CommandType::NoteOff,0,0,0}));const unsigned n=frames-at<24?frames-at:24;e->render(block,n);for(unsigned f=0;f<n;++f){float v=(block[2*f]+block[2*f+1])*.5f;const unsigned left=frames-at-f;if(left<96)v*=float(left)/96;audio[at+f]=v;peak=fmaxf(peak,fabsf(v));}}assert(peak>1e-5f);std::vector<uint8_t> pcm(44+2*frames);wav_header_ch(pcm.data(),frames,48000,1);for(unsigned f=0;f<frames;++f){int16_t value=int16_t(audio[f]*(24575/peak));pcm[44+2*f]=uint8_t(value);pcm[45+2*f]=uint8_t(uint16_t(value)>>8);}std::snprintf(wav,sizeof wav,"%s/DRUMS/%s.wav",root,synth_drum_name(i));assert(fs.write(wav,0,pcm.data(),pcm.size(),true));std::snprintf(canonical,sizeof canonical,"FACTORY/DRUMS/VA-FM/%s.wav",synth_drum_name(i));auto& a=kit->assets[i];copy_text(a.path,96,canonical);a.metadata.frames=frames;a.metadata.rate=48000;a.metadata.channels=1;a.metadata.bits=16;a.metadata.root=60;assert(file_checksum(fs,wav,a.metadata.content_crc));auto& pad=k.kit.pads[i];pad.sample[0]=uint8_t(i);pad.gm_note=kFactoryGmNotes[i];pad.level=i==0?.8f:i==1?.72f:i==2?.46f:.6f;pad.edit.choke=i==2 || i==3?1:0;std::printf("%s engine=%s note=%u frames=%u raw-peak=%.8f crc=%08x\n",synth_drum_name(i),engine_guid(p->tracks[0].instrument),p->tracks[0].root_note,frames,peak,a.metadata.content_crc);
 }
 assert(project_paths(*kit,"FACTORY/KITS/VA + FM Percussion.mgxk",false));FileWriter writer(fs,"assets/factory-synth-drums/KITS/VA + FM Percussion.mgxk");assert(encode_project(writer,*kit) && writer.finish());
}
