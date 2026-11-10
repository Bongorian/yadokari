#pragma once
#include "file_access.h"
#include "wav_writer.h"
#include "native_stress_project.h"
namespace groove {
// Mixed workload v1: four independent 48-second streamed stereo files.
// File generation is incremental on the service owner, before timing begins.
class AudioStressAssets {
 char directory_[64]{};bool created_[4]{};IFileAccess& fs_;unsigned file_=0;uint32_t cursor_=0;bool ready_=false,failed_=false;uint8_t bytes_[16384]{};
public:
 static constexpr uint32_t Frames=48000*48;
 WavView views[64]{};SampleAssetState assets[4]{};
 explicit AudioStressAssets(IFileAccess& fs):fs_(fs){}
 bool start(){file_=cursor_=0;ready_=failed_=false;for(auto& c:created_)c=false;for(unsigned n=1;n<1000;++n){copy_text(directory_,sizeof directory_,"GROOVE/AUDIO-000");directory_[13]=char('0'+n/100);directory_[14]=char('0'+n/10%10);directory_[15]=char('0'+n%10);if(!fs_.exists(directory_))return fs_.directory(directory_);}directory_[0]=0;return false;}
 bool cleanup(){bool ok=true;for(unsigned i=0;i<4;++i)if(created_[i]){char path[96],name[]="STREAM0.WAV";name[6]=char('0'+i);ok=join_path(path,sizeof path,directory_,name) && fs_.remove(path) && ok;created_[i]=false;}if(directory_[0])ok=fs_.remove(directory_) && ok;directory_[0]=0;return ok;}
 bool service(ISampleLibrary& library){
  if(ready_ || failed_)return ready_;
  char path[96],name[]="STREAM0.WAV";name[6]=char('0'+file_);if(!join_path(path,sizeof path,directory_,name)){failed_=true;return false;}
  if(!cursor_){created_[file_]=true;uint8_t header[44];if(!wav_header(header,Frames,48000) || !fs_.write(path,0,header,44,true)){failed_=true;return false;}}
  unsigned count=Frames-cursor_;if(count>4096)count=4096;
  for(unsigned n=0;n<count;++n){int16_t l=((cursor_+n+file_*17)%240)<120?1638:-1638,r=((cursor_+n+file_*31)%320)<160?819:-819;bytes_[4*n]=uint8_t(l);bytes_[4*n+1]=uint8_t(uint16_t(l)>>8);bytes_[4*n+2]=uint8_t(r);bytes_[4*n+3]=uint8_t(uint16_t(r)>>8);}
  if(!fs_.write(path,44+uint64_t(cursor_)*4,bytes_,count*4,false)){failed_=true;return false;}cursor_+=count;
  if(cursor_==Frames){if(!library.load(path,views[file_]) || !views[file_].reader){failed_=true;return false;}copy_text(assets[file_].path,sizeof assets[file_].path,path);assets[file_].metadata.frames=Frames;assets[file_].metadata.rate=48000;assets[file_].metadata.channels=2;assets[file_].metadata.bits=16;cursor_=0;if(++file_==4)ready_=true;}
  return ready_;
 }
 bool failed()const{return failed_;}bool ready()const{return ready_;}
 uint32_t misses()const{uint32_t sum=0;for(unsigned i=0;i<4;++i)if(views[i].reader)sum+=views[i].reader->underruns();return sum;}
 void project(ProjectState& p,unsigned which)const{make_native_stress_project(p,which);p.song.enabled=true;p.song.length=1;p.song.sections[0].bars=64;
  for(unsigned i=0;i<4;++i){p.assets[i]=assets[i];auto& a=p.audio[12+i];a.kind=i<2?TrackKind::Looper:TrackKind::Audio;a.length=LoopLength::Free;a.source=0;a.gain=0.1f;a.clips[0]={0,0,Frames,1,uint8_t(i)};}
 }
};
}
