#pragma once
#include "load_sf2_metadata.h"
#include "../src/file_access.h"
#include "../src/project_files.h"
#include <math.h>
namespace groove {
class LoadSf2Generator {
 static constexpr unsigned Frames=1048576,Stride=Frames+46;
 uint32_t cursor_=0;unsigned sample_=0,period_=0;bool started_=false,ready_=false,failed_=false;
 int16_t cycle_[8192]{};uint8_t bytes_[32768]{};
public:
 void reset(){cursor_=sample_=period_=0;started_=ready_=failed_=false;}
 bool ready()const{return ready_;}bool failed()const{return failed_;}
 bool service(IFileAccess& fs){
  const char* path="GROOVE/LOADTEST.SF2";if(ready_ || failed_)return ready_;
  if(!started_){started_=true;
   // Reuse only this exact diagnostic asset; never truncate an existing file.
   // A collision with any other content is a test failure, not permission to overwrite.
   if(fs.exists(path)){uint32_t crc=0;ready_=fs.size(path)==33557548 && file_checksum(fs,path,crc) && crc==0x09ffdf02;failed_=!ready_;return ready_;}
   if(!fs.directory("GROOVE") || !fs.write(path,0,kLoadSf2Prefix,sizeof kLoadSf2Prefix,true)){failed_=true;return false;}}
  if(!cursor_){period_=kLoadSf2Periods[sample_];if(period_>8192){failed_=true;return false;}for(unsigned i=0;i<period_;++i){float phase=float(i)/period_;float value=sample_%4==0?sinf(6.283185307f*phase):sample_%4==1?1-4*fabsf(phase-.5f):sample_%4==2?.7f*sinf(6.283185307f*phase)+.3f*sinf(12.566370614f*phase):2*phase-1;cycle_[i]=int16_t(value*5000);}}
  unsigned n=Stride-cursor_;if(n>sizeof bytes_/2)n=sizeof bytes_/2;
  for(unsigned i=0;i<n;++i){int16_t v=cursor_+i<Frames?cycle_[(cursor_+i)%period_]:0;bytes_[2*i]=uint8_t(v);bytes_[2*i+1]=uint8_t(uint16_t(v)>>8);}
  uint64_t offset=sizeof kLoadSf2Prefix+uint64_t(sample_*Stride+cursor_)*2;
  if(!fs.write(path,offset,bytes_,n*2,false)){failed_=true;return false;}cursor_+=n;
  if(cursor_==Stride){cursor_=0;if(++sample_==16){ready_=fs.write(path,sizeof kLoadSf2Prefix+uint64_t(16)*Stride*2,kLoadSf2Suffix,sizeof kLoadSf2Suffix,false);failed_=!ready_;}}
  return ready_;
 }
};
}
