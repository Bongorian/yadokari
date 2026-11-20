#pragma once
#include "engine.h"
#include "file_access.h"
#include "wav_writer.h"

namespace groove {
// File-task side of the single-pass Song recorder. A partial directory never
// takes the final name; media removed during export can leave a .tmp folder.
class SongStems {
public:
 bool busy()const{return busy_;}
 const char* message()const{return message_;}
 bool start(IFileAccess& fs,Engine& engine,const char* root){
  if(busy_)return false;
  message_="Cannot create stem folder";
  if(!fs.directory(root))return false;
  bool found=false;
  for(unsigned i=1;i<=999 && !found;++i){
   char name[]="Stems-000";
   name[6]=char('0'+i/100);name[7]=char('0'+i/10%10);name[8]=char('0'+i%10);
   if(!join_path(final_,sizeof final_,root,name) || !copy_text(temp_,sizeof temp_,final_))return false;
   unsigned n=__builtin_strlen(temp_);
   if(!copy_text(temp_+n,sizeof temp_-n,".tmp"))return false;
   found=!fs.exists(final_) && !fs.exists(temp_);
  }
  if(!found || !fs.directory(temp_))return false;
  for(auto& path:paths_)path[0]=0;
  frames_=0;
  uint8_t header[44];wav_header(header,0,engine.sample_rate());
  for(unsigned b=0;b<StemCapture::Buses;++b){
   if(!join_path(paths_[b],sizeof paths_[b],temp_,names_[b]) ||
      !fs.write(paths_[b],0,header,sizeof header,true)){
    discard(fs);return false;
   }
  }
  if(!engine.start_stem_song()){message_="Stem capture unavailable";discard(fs);return false;}
  busy_=true;message_="Recording Song stems / STOP cancels";return true;
 }
 // Returns true once per completed or failed job.
 bool service(IFileAccess& fs,Engine& engine,bool& success){
  if(!busy_)return false;
  success=false;
  auto& capture=engine.stems();
  const auto state=capture.state();
  if(state==StemCapture::State::Overrun || state==StemCapture::State::Cancelled){
   message_=state==StemCapture::State::Overrun?"Stem export failed / SD too slow":"Stem export cancelled";
   discard(fs);return true;
  }
  if(const auto* block=capture.front()){
   const unsigned bytes=block->frames*4;
   const uint64_t offset=44+uint64_t(frames_)*4;
   bool ok=offset+bytes<=0xffffffffu;
   for(unsigned b=0;ok && b<StemCapture::Buses;++b)ok=fs.append_audio(paths_[b],offset,block->pcm[b],bytes,cursors_[b]);
   if(!ok){capture.request_cancel();message_="Stem export failed / SD write or WAV limit";discard(fs);return true;}
   frames_+=block->frames;capture.release();return false;
  }
  if(state!=StemCapture::State::Complete)return false;
  uint8_t header[44];
  bool ok=close_streams(fs) && frames_ && wav_header(header,frames_,engine.sample_rate());
  for(unsigned b=0;ok && b<StemCapture::Buses;++b){
   ok=fs.size(paths_[b])==44+uint64_t(frames_)*4 && fs.write(paths_[b],0,header,sizeof header,false);
  }
  if(!ok || fs.exists(final_) || !fs.move(temp_,final_)){
   message_="Stem export failed / cannot finish WAV set";discard(fs);return true;
  }
  busy_=false;success=true;message_=final_;return true;
 }
 void abort(IFileAccess& fs,Engine& engine){
  if(busy_){engine.stems().request_cancel();discard(fs);}
 }
private:
 bool close_streams(IFileAccess& fs){
  bool ok=true;
  for(auto& cursor:cursors_)if(!fs.close_audio_append(cursor))ok=false;
  return ok;
 }
 void discard(IFileAccess& fs){
  close_streams(fs);
  for(auto& path:paths_)if(path[0]){fs.remove(path);path[0]=0;}
  fs.remove(temp_);busy_=false;
 }
 inline static constexpr const char* names_[StemCapture::Buses]={
  "Group1.wav","Group2.wav","Group3.wav","Group4.wav","Group5.wav","Group6.wav","Group7.wav","Group8.wav",
  "Direct.wav","Returns.wav","Mix.wav"
 };
 char final_[144]{},temp_[152]{},paths_[StemCapture::Buses][176]{};
 void* cursors_[StemCapture::Buses]{};
 const char* message_="";unsigned frames_=0;bool busy_=false;
};
}
