#pragma once
#include <stdint.h>
#include <stddef.h>
namespace groove {
// AP-only DHCPv4, eight leases for the fixed 192.168.76.0/24 local link.
// No relay, routing or persistent lease database; lifetime is transfer mode.
class FileDhcp {
 uint8_t clients_[8][6]{};bool used_[8]{};uint64_t expires_[8]{};
public:
 unsigned reply(const uint8_t* input,unsigned n,uint8_t* output,unsigned capacity,uint64_t now){
  if(n<240 || capacity<320 || input[0]!=1 || input[1]!=1 || input[2]!=6 || input[3] || input[236]!=99 || input[237]!=130 || input[238]!=83 || input[239]!=99)return 0;
  for(unsigned i=24;i<28;++i)if(input[i])return 0;
  unsigned type=0;bool other=false;const uint8_t* requested=nullptr;for(unsigned at=240;at<n;){unsigned tag=input[at++];if(tag==255)break;if(!tag)continue;if(at==n)return 0;unsigned length=input[at++];if(length>n-at)return 0;if(tag==53 && length==1)type=input[at];if(tag==54 && length==4)other=__builtin_memcmp(input+at,"\xc0\xa8\x4c\x01",4)!=0;if(tag==50 && length==4)requested=input+at;at+=length;}
  if(other || (type!=1 && type!=3 && type!=7))return 0;
  for(unsigned i=0;i<8;++i)if(used_[i] && now>=expires_[i])used_[i]=false;
  unsigned client=8;for(unsigned i=0;i<8;++i)if(used_[i] && !__builtin_memcmp(clients_[i],input+28,6)){client=i;break;}
  if(type==7){if(client<8)used_[client]=false;return 0;}
  if(client==8){for(unsigned i=0;i<8;++i)if(!used_[i]){client=i;break;}}if(client==8)return 0;
  uint8_t address[]={192,168,76,uint8_t(client+2)};bool nak=type==3 && requested && __builtin_memcmp(requested,address,4);if(!nak){uint64_t until=now+(type==3?600000000ull:30000000ull);if(!used_[client] || until>expires_[client])expires_[client]=until;used_[client]=true;__builtin_memcpy(clients_[client],input+28,6);}__builtin_memset(output,0,320);output[0]=2;output[1]=1;output[2]=6;__builtin_memcpy(output+4,input+4,4);output[10]=0x80;if(!nak)__builtin_memcpy(output+16,address,4);__builtin_memcpy(output+20,"\xc0\xa8\x4c\x01",4);__builtin_memcpy(output+28,input+28,16);__builtin_memcpy(output+236,input+236,4);unsigned at=240;
  auto option=[&](unsigned tag,const uint8_t* value,unsigned bytes){output[at++]=uint8_t(tag);output[at++]=uint8_t(bytes);__builtin_memcpy(output+at,value,bytes);at+=bytes;};
  uint8_t response=nak?6:type==1?2:5;option(53,&response,1);option(54,reinterpret_cast<const uint8_t*>("\xc0\xa8\x4c\x01"),4);if(!nak){option(1,reinterpret_cast<const uint8_t*>("\xff\xff\xff\x00"),4);option(3,reinterpret_cast<const uint8_t*>("\xc0\xa8\x4c\x01"),4);option(51,reinterpret_cast<const uint8_t*>("\x00\x00\x02\x58"),4);}output[at++]=255;return at<300?300:at;
 }
};
}
