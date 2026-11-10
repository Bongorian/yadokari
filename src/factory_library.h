#pragma once
#include "project_codec.h"
#include "data_layout.h"
#include "update/sha256.h"
namespace groove {
struct FactoryLibraryEntry {const char* path;unsigned bytes,offset,packed;uint8_t sha256[32];};
#include "generated/factory_library_bank.inc"
static_assert(kFactoryLibraryPacking==1,"Unsupported factory packing");
// Boot/file-service owner only. Scratch is reserved by the caller; no audio I/O.
// GBL1: 0..127 = literal length+1; 128..255 = match length+3, u16 distance.
inline bool unpack_factory_library(const uint8_t* packed,unsigned size,uint8_t* out,unsigned bytes,const uint8_t expected[32]){
 if(!packed || !out || !expected)return false;
 unsigned in=0,at=0;
 while(in<size){unsigned token=packed[in++],length=(token&127)+(token<128?1:3);
  if(length>bytes-at)return false;
  if(token<128){if(length>size-in)return false;__builtin_memcpy(out+at,packed+in,length);in+=length;at+=length;}
  else{if(size-in<2)return false;unsigned distance=unsigned(packed[in])|(unsigned(packed[in+1])<<8);in+=2;if(!distance || distance>at)return false;for(unsigned n=0;n<length;++n){out[at]=out[at-distance];++at;}}
 }
 if(at!=bytes)return false;
 uint8_t actual[32];update::Sha256::hash(out,bytes,actual);return update::equal(actual,expected,32);
}
// One boot/file-service allocation, never on the audio owner. The additional
// lossless layer removes repetition between independently packed masters.
class FactoryLibraryBank {
 uint8_t* data_=nullptr;bool ready_=false;
public:
 FactoryLibraryBank(){if(!kFactoryLibraryCount){ready_=true;return;}data_=alloc_array<uint8_t>(kFactoryLibraryBankBytes);ready_=data_ && unpack_factory_library(kFactoryLibraryPacked,sizeof kFactoryLibraryPacked,data_,kFactoryLibraryBankBytes,kFactoryLibraryBankHash);}
 ~FactoryLibraryBank(){delete[] data_;}
 FactoryLibraryBank(const FactoryLibraryBank&)=delete;FactoryLibraryBank& operator=(const FactoryLibraryBank&)=delete;
 bool ready()const{return ready_;}
 bool unpack(const FactoryLibraryEntry& entry,uint8_t* out,unsigned capacity)const{
  return ready_ && data_ && entry.bytes<=capacity && entry.offset<=kFactoryLibraryBankBytes && entry.packed<=kFactoryLibraryBankBytes-entry.offset && unpack_factory_library(data_+entry.offset,entry.packed,out,entry.bytes,entry.sha256);
 }
};
}
