#pragma once
#include <stddef.h>
#include <stdint.h>
namespace groove {
// A validated Yamaha 32-voice bulk dump; underlying bytes must stay immutable
// until the audio consumer no longer references the bank.
struct Dx7BankView { const uint8_t* data=nullptr; };
inline bool parse_dx7_bulk(const uint8_t* bytes,size_t n,Dx7BankView& result) {
 if(!bytes || n!=4104 || bytes[0]!=0xf0 || bytes[1]!=0x43 || (bytes[2]&0xf0)!=0 || bytes[3]!=0x09 || bytes[4]!=0x20 || bytes[5]!=0 || bytes[4103]!=0xf7) return false;
 unsigned sum=0; for(size_t i=6;i<4102;++i) { if(bytes[i]&0x80) return false; sum+=bytes[i]; }
 if(bytes[4102]&0x80 || ((sum+bytes[4102])&0x7f)!=0) return false;
 result.data=bytes+6; return true;
}
}
