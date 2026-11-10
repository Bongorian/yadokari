#pragma once
#include "project_files.h"
#include "soundfont_fixture.h"
#include "wav_writer.h"
#include <memory>
#include <vector>
namespace file_fixture {
using namespace groove;
inline std::vector<uint8_t> project(ContentKind kind){auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->content_kind=kind;auto t=track_at(*p,0,0);if(kind!=ContentKind::Song)t.preset_identity=builtin_factory_identity(unsigned(kind));t.instrument=kind==ContentKind::Kit?Instrument::DrumKit:Instrument::FM;
 if(kind==ContentKind::Song){for(unsigned i=0;i<4;++i){auto& s=t.steps[i*4];s.enabled=true;s.note=uint8_t(60+i*3);s.velocity=96;s.gate=72;}}
 ChunkBuffer out;if(!encode_project_buffer(out,*p))return {};return {out.data,out.data+out.size};}
inline std::vector<uint8_t> wav(){std::vector<uint8_t> out(300);wav_header(out.data(),64,48000);for(unsigned i=44;i<out.size();++i)out[i]=uint8_t(i*7);return out;}
inline std::vector<uint8_t> midi(){return {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,4,0,0xff,0x2f,0};}
inline std::vector<uint8_t> syx(){std::vector<uint8_t> out(4104);out[0]=0xf0;out[1]=0x43;out[3]=9;out[4]=0x20;out[4103]=0xf7;return out;}
inline bool files(IFileAccess& fs){if(!fs.directory("FIXTURES") || !fs.directory("PROJECTS") || !fs.directory("ASSETS") || !fs.directory("ASSETS/SAMPLES"))return false;
 const ContentKind kinds[]={ContentKind::Song,ContentKind::Preset,ContentKind::Kit};const char* names[]={"FIXTURES/Song.mgxs","FIXTURES/Preset.mgxp","FIXTURES/Kit.mgxk"};
 for(unsigned i=0;i<3;++i){auto bytes=project(kinds[i]);if(bytes.empty() || !fs.write(names[i],0,bytes.data(),bytes.size(),true))return false;}
 auto w=wav(),m=midi(),bank=syx();auto current=project(ContentKind::Song);
 return fs.write("FIXTURES/Tone.wav",0,w.data(),w.size(),true) && fs.write("FIXTURES/Pattern.mid",0,m.data(),m.size(),true) && fs.write("FIXTURES/Bank.syx",0,bank.data(),bank.size(),true) && fs.write("FIXTURES/Bank.sf2",0,kSoundFontFixture,sizeof kSoundFontFixture,true) && fs.write("PROJECTS/Open.mgxs",0,current.data(),current.size(),true) && fs.write("ASSETS/SAMPLES/Locked.wav",0,w.data(),w.size(),true);
}
}
