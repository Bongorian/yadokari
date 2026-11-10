#pragma once
#include "manager.h"
namespace groove { namespace update {
// Stop-and-wait framing bounds both CDC buffering and TCP memory. One response
// per signed header and each <=4096-byte payload block. Transport must flush the
// response before accepting the next block; host sender follows this contract.
class Receiver {
 Manager& manager_;uint8_t block_[4096]{};unsigned used_=0,expected_=HeaderSize;bool header_=true,done_=false;
public:
 explicit Receiver(Manager& manager):manager_(manager){}
 void reset(){if(manager_.receiving())manager_.abort();used_=0;expected_=HeaderSize;header_=true;done_=false;}
 // Returns 1 for READY, 2 for a block ACK, 3 for verified completion, -1 on error.
 int feed(const uint8_t* bytes,unsigned n){
  if(done_||!n||n>expected_-used_){if(manager_.receiving())manager_.abort();done_=true;return -1;}
  for(unsigned i=0;i<n;++i)block_[used_++]=bytes[i];
  if(used_!=expected_)return 0;
  used_=0;if(header_){header_=false;if(!manager_.begin(block_)){done_=true;return -1;}expected_=manager_.total()<4096?manager_.total():4096;return 1;}
  if(!manager_.append(block_,expected_)){done_=true;return -1;}
  unsigned remaining=manager_.total()-manager_.received();if(!remaining){done_=true;return manager_.finish()?3:-1;}expected_=remaining<4096?remaining:4096;return 2;
 }
 unsigned wanted()const{return expected_-used_;}bool done()const{return done_;}
};
}}
