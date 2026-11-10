#pragma once
#include <stdint.h>
namespace groove {
struct DisplayWireFacts {unsigned commands=0,literal_pairs=0,repeated_pairs=0;bool valid=false;};
// Observe a complete MK3 packet without changing its byte stream.
inline DisplayWireFacts display_wire_facts(const uint8_t* packet,unsigned bytes){
 DisplayWireFacts out;if(!packet || bytes<28)return out;unsigned at=16;
 while(at<bytes-8){if(bytes-8-at<4)return out;unsigned op=packet[at],pairs=(unsigned(packet[at+1])<<16)|(unsigned(packet[at+2])<<8)|packet[at+3];at+=4;if(!pairs || op>1)return out;
  unsigned payload=op==1?4:pairs*4;if(payload>bytes-8-at)return out;
  if(op)out.repeated_pairs+=pairs;else out.literal_pairs+=pairs;++out.commands;at+=payload;
 }
 out.valid=at==bytes-8 && packet[at]==3 && packet[at+4]==0x40;return out;
}
}
