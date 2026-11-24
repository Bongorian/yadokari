#pragma once
#include "file_access.h"
namespace groove {
// YGR1 stores absolute offsets from the straight grid, including source swing.
// The destination's swing is subtracted by paste_groove when applying the template.
struct GrooveTemplate {
 int16_t timing[16]{};
 int8_t velocity[16]{};
 bool valid()const{
  for(unsigned i=0;i<16;++i)
   if(timing[i]<-192 || timing[i]>288 || velocity[i]<-64 || velocity[i]>64)return false;
  return true;
 }
};
inline bool read_groove_template(IFileAccess& fs,const char* path,GrooveTemplate& out){
 uint8_t bytes[52];unsigned got=0;
 if(fs.size(path)!=sizeof bytes || !fs.read(path,0,bytes,sizeof bytes,got) ||
    got!=sizeof bytes || __builtin_memcmp(bytes,"YGR1",4))return false;
 GrooveTemplate candidate;
 for(unsigned i=0;i<16;++i){
  unsigned at=4+i*3,raw=bytes[at]|unsigned(bytes[at+1])<<8;
  candidate.timing[i]=int16_t(raw<32768?int(raw):int(raw)-65536);
  candidate.velocity[i]=int8_t(bytes[at+2]<128?int(bytes[at+2]):int(bytes[at+2])-256);
 }
 if(!candidate.valid())return false;
 out=candidate;return true;
}
inline bool save_groove_template(IFileAccess& fs,const GrooveTemplate& input,char (&path)[24]){
 if(!input.valid() || !fs.directory("GROOVES"))return false;
 uint8_t bytes[52]={'Y','G','R','1'};
 for(unsigned i=0;i<16;++i){
  unsigned at=4+i*3,time=uint16_t(input.timing[i]);
  bytes[at]=uint8_t(time);bytes[at+1]=uint8_t(time>>8);bytes[at+2]=uint8_t(input.velocity[i]);
 }
 constexpr char name[]="GROOVES/Groove-000.grv";
 constexpr unsigned digits=sizeof name-sizeof "000.grv";
 for(unsigned n=1;n<=999;++n){
  copy_text(path,sizeof path,name);
  path[digits]=char('0'+n/100);path[digits+1]=char('0'+n/10%10);path[digits+2]=char('0'+n%10);
  char temp[sizeof name+4];copy_text(temp,sizeof temp,path);copy_text(temp+sizeof name-1,5,".tmp");
  if(fs.exists(path) || fs.exists(temp))continue;
  if(fs.write(temp,0,bytes,sizeof bytes,true) && fs.move(temp,path))return true;
  fs.remove(temp);return false;
 }
 return false;
}
}
