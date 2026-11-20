#include "update/data_growth.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <map>
using namespace groove::update;
using Sector=std::array<uint8_t,512>;
struct Disk:BlockDevice {
 std::map<unsigned,Sector> stored;
 unsigned writes=0,fail_lba=~0u;
 bool sector(unsigned lba,uint8_t* bytes,bool write)override{
  if(write){
   if(lba==fail_lba){fail_lba=~0u;return false;}
   ++writes;memcpy(stored[lba].data(),bytes,512);
  }else{
   const auto it=stored.find(lba);
   if(it==stored.end())memset(bytes,0,512);else memcpy(bytes,it->second.data(),512);
  }
  return true;
 }
};
constexpr unsigned base=2048+3*32768,old=66500,reserved=32,oldfat=520,old_end=2+old-reserved-oldfat;
static Disk image(bool full){
 Disk d;
 auto& m=d.stored[0];m[510]=0x55;m[511]=0xaa;
 for(unsigned i=0;i<4;++i){auto* p=m.data()+446+16*i;p[4]=0x0c;put32(p+8,2048+i*32768);put32(p+12,i==3?old:32768);}
 auto& marker=d.stored[8];memcpy(marker.data(),"GSD1",4);Sha256::hash(m.data()+446,64,marker.data()+4);
 auto& boot=d.stored[base];boot[12]=2;boot[13]=1;boot[14]=reserved;boot[16]=1;boot[48]=1;boot[50]=6;
 put32(boot.data()+32,old);put32(boot.data()+36,oldfat);
 d.stored[40]=m;d.stored[41]=boot;
 auto& intent=d.stored[42];memcpy(intent.data(),"GRW1",4);Sha256::hash(m.data(),512,intent.data()+4);
 Sha256::hash(boot.data(),512,intent.data()+36);seal_growth(intent.data());
 for(unsigned c=0;c<oldfat*128;++c){
  if(full || c<3 || c==4 || c==old_end-1 || c>=old_end)
   put32(d.stored[base+reserved+c/128].data()+4*(c%128),0x0fffffff);
 }
 d.stored[base+1].fill(0xff);d.stored[base+7].fill(0x77);
 d.stored[base+reserved+oldfat][0]=0xab;
 return d;
}
static void verify(Disk& d,unsigned size,bool full){
 const auto fat=get32(d.stored[base].data()+36),end=2+size-reserved-fat;
 const auto& info=d.stored[base+1];assert(info==d.stored[base+7] && d.stored[base]==d.stored[base+6]);
 assert(get32(info.data())==0x41615252 && get32(info.data()+484)==0x61417272 && get32(info.data()+508)==0xaa550000);
 const auto count=get32(info.data()+488),hint=get32(info.data()+492);
 assert(count==(full?0:old_end-2-3)+(end-old_end));
 assert(hint==(full?old_end:3) && hint<end);
 unsigned actual=0;
 for(unsigned c=2;c<end;++c){
  const bool empty=!(get32(d.stored[base+reserved+c/128].data()+4*(c%128))&0x0fffffff);
  if(c==hint)assert(empty);
  actual+=empty;
 }
 assert(count==actual && d.stored[base+reserved+fat][0]==0xab);
}
int main(){
 for(auto size:{old+10,old+5000})for(bool full:{false,true}){
  Disk d=image(full);assert(expand_data(d,base+size));verify(d,size,full);
  auto writes=d.writes;assert(expand_data(d,base+size) && d.writes==writes);
  // A completed legacy card can have user edits. Never replace its FSInfo
  // from the original provisioned geometry.
  d.stored[base+1].fill(0xff);assert(expand_data(d,base+size) && d.writes==writes);
 }
 for(auto failure:{base+7,base+1,base+6,base,0u,8u}){
  Disk d=image(false);d.fail_lba=failure;const unsigned size=old+5000;
  assert(!expand_data(d,base+size));assert(expand_data(d,base+size));verify(d,size,false);
 }
 std::puts("PASS growth: FAT census, primary/backup hints, publication replay and completed cards");
}
