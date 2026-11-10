#pragma once
#include "file_access.h"
#include "engine.h"
namespace groove {
// Task-owned Standard MIDI File writer. The audio owner only writes a bounded
// SPSC queue; disk access and finalization never run in the render callback.
// Format 0, SMPTE -25 fps x 40 ticks = one millisecond per tick.
class MidiCapture {
public:
 enum class State:uint8_t { Idle, Waiting, Recording, Stopping, Closing, FailDrain, Failed };
 MidiCapture(IFileAccess& fs,Engine& engine):fs_(fs),engine_(engine){}
 State state()const{return state_;}bool busy()const{return state_!=State::Idle && state_!=State::Failed;}
 const char* path()const{return final_;}bool internal()const{return internal_;}
 bool start(const char* directory="RECORDINGS",bool internal=false){
  if(busy() || !fs_.directory(directory))return false;
  MidiCaptureEvent stale;while(engine_.pop_midi_capture(stale)){}
  bool found=false;for(unsigned n=0;n<100;++n){char name[]="MIDI00.MID",part[]="MIDI00.PRT";name[4]=part[4]=char('0'+n/10);name[5]=part[5]=char('0'+n%10);if(!join_path(final_,sizeof final_,directory,name)||!join_path(temp_,sizeof temp_,directory,part))return false;if(!fs_.exists(final_)&&!fs_.exists(temp_)){found=true;break;}}
  if(!found)return false;
  static const uint8_t header[]={ 'M','T','h','d',0,0,0,6,0,0,0,1,0xe7,40,'M','T','r','k',0,0,0,0 };
  if(!fs_.write(temp_,0,header,sizeof header,true))return false;
  used_=0;offset_=sizeof header;last_tick_=0;origin_=0;close_index_=0;stop_sent_=false;end_written_=false;closing_delta_=0;for(auto& n:active_)n=0;
  internal_=internal;if(!engine_.start_midi_capture(internal)){state_=State::Failed;return false;}state_=State::Waiting;return true;
 }
 bool stop(){if(state_!=State::Waiting && state_!=State::Recording)return false;if(!engine_.stop_midi_capture())return false;state_=State::Stopping;return true;}
 // One bounded disk write per invocation, plus bounded queue work.
 void service(){
  if(!busy())return;
  if(engine_.midi_capture_failed()){MidiCaptureEvent event;while(engine_.pop_midi_capture(event)){}state_=State::Failed;return;}
  if(state_==State::FailDrain){if(!stop_sent_)stop_sent_=engine_.stop_midi_capture();MidiCaptureEvent event;for(unsigned n=0;n<64 && engine_.pop_midi_capture(event);++n)if(event.kind==2){state_=State::Failed;return;}return;}
  if(used_>=4096){if(!flush(4096))fail();return;}
  MidiCaptureEvent event;for(unsigned n=0;n<64 && used_+16<sizeof buffer_ && engine_.pop_midi_capture(event);++n){
   if(event.kind==1){origin_=event.event.sample;if(state_==State::Waiting)state_=State::Recording;continue;}
   if(event.kind==2){if(event.event.sample<origin_){fail();return;}uint64_t elapsed=event.event.sample-origin_;uint64_t tick=(elapsed/engine_.sample_rate())*1000+(elapsed%engine_.sample_rate())*1000/engine_.sample_rate();if(tick<last_tick_ || tick-last_tick_>0x0fffffffu){fail();return;}closing_delta_=uint32_t(tick-last_tick_);state_=State::Closing;break;}
   if(event.kind || !event.event.length || event.event.bytes[0]<0x80 || event.event.bytes[0]>=0xf0){fail();return;}
   if(event.event.sample<origin_){fail();return;}uint64_t elapsed=event.event.sample-origin_;uint64_t ticks=(elapsed/engine_.sample_rate())*1000+(elapsed%engine_.sample_rate())*1000/engine_.sample_rate();if(ticks<last_tick_ || ticks-last_tick_>0x0fffffffu){fail();return;}
   unsigned status=event.event.bytes[0],channel=status&15,note=event.event.bytes[1];uint8_t delta[4];unsigned count=variable(uint32_t(ticks-last_tick_),delta);for(unsigned i=0;i<count;++i)buffer_[used_++]=delta[i];for(unsigned i=0;i<event.event.length;++i)buffer_[used_++]=event.event.bytes[i];last_tick_=ticks;
   if((status&0xf0)==0x90 && event.event.bytes[2]){auto& value=active_[channel*128+note];if(value==255){fail();return;}++value;}else if((status&0xf0)==0x80 || ((status&0xf0)==0x90 && !event.event.bytes[2])){auto& value=active_[channel*128+note];if(value)--value;}else if((status&0xf0)==0xb0 && (note==120 || note==123))for(unsigned i=0;i<128;++i)active_[channel*128+i]=0;
  }
  if(state_==State::Closing){unsigned count=0;while(close_index_<2048 && count<64 && used_+8<sizeof buffer_){auto& notes=active_[close_index_];if(notes){put_closing_delta();buffer_[used_++]=uint8_t(0x80|(close_index_/128));buffer_[used_++]=uint8_t(close_index_%128);buffer_[used_++]=0;--notes;++count;}else ++close_index_;}if(close_index_==2048 && !end_written_){put_closing_delta();buffer_[used_++]=0xff;buffer_[used_++]=0x2f;buffer_[used_++]=0;end_written_=true;}}
  if(used_ && !flush(used_>4096?4096:used_)){fail();return;}
  if(state_==State::Closing && end_written_ && !used_){uint64_t length=offset_-22;if(length>0xffffffffu){fail();return;}uint8_t bytes[]={uint8_t(length>>24),uint8_t(length>>16),uint8_t(length>>8),uint8_t(length)};if(!fs_.write(temp_,18,bytes,4,false) || fs_.size(temp_)!=offset_ || fs_.exists(final_) || !fs_.move(temp_,final_)){fail();return;}state_=State::Idle;}
 }
private:
 static unsigned variable(uint32_t value,uint8_t* out){uint8_t reversed[4];unsigned n=0;reversed[n++]=uint8_t(value&0x7f);while((value>>=7))reversed[n++]=uint8_t((value&0x7f)|0x80);for(unsigned i=0;i<n;++i)out[i]=reversed[n-i-1];return n;}
 void put_closing_delta(){uint8_t bytes[4];unsigned n=variable(closing_delta_,bytes);for(unsigned i=0;i<n;++i)buffer_[used_++]=bytes[i];closing_delta_=0;}
 bool flush(unsigned n){if(!fs_.write(temp_,offset_,buffer_,n,false))return false;offset_+=n;used_-=n;__builtin_memmove(buffer_,buffer_+n,used_);return true;}
 void fail(){if(state_==State::Closing)state_=State::Failed;else{state_=State::FailDrain;stop_sent_=false;}}
 IFileAccess& fs_;Engine& engine_;State state_=State::Idle;char temp_[160]{},final_[160]{};uint8_t buffer_[8192]{},active_[2048]{};unsigned used_=0,close_index_=0;uint32_t closing_delta_=0;uint64_t offset_=0,origin_=0,last_tick_=0;bool stop_sent_=false,end_written_=false,internal_=false;
};
}
