#pragma once
#include "fat_storage.h"
#include <string.h>

namespace groove { namespace update {
// Only provisioned single-FAT FAT32 volumes opt in. Sectors 40/41 hold
// immutable original MBR/BPB copies. Sector 42 binds them to this layout.
// Progress alternates between 43 and 44; BOOT/A/B and the update journal
// never move. DATA must be unmounted while a growth transaction is active.
inline void seal_growth(uint8_t* b){Sha256::hash(b,480,b+480);}
inline bool growth_sealed(const uint8_t* b){uint8_t sum[32];Sha256::hash(b,480,sum);return equal(sum,b+480,32);}
inline unsigned growth_word(const uint8_t* b){return b[0]+256u*b[1];}
inline bool expand_data(BlockDevice& disk,uint32_t capacity){
 uint8_t intent[512],original[512],boot[512],mbr[512],buffer[512],fat[512];
 if(!disk.sector(42,intent,false))return false;
 if(memcmp(intent,"GRW1",4))return true;
 if(!growth_sealed(intent) || !disk.sector(40,original,false) || !disk.sector(41,boot,false) || !disk.sector(0,mbr,false))return false;
 uint8_t hash[32];Sha256::hash(original,512,hash);if(!equal(hash,intent+4,32))return false;
 Sha256::hash(boot,512,hash);if(!equal(hash,intent+36,32))return false;
 constexpr unsigned part=446+3*16;
 const uint32_t base=get32(original+part+8),old=get32(original+part+12),oldfat=get32(boot+36);
 const unsigned reserved=growth_word(boot+14),spc=boot[13],backup=growth_word(boot+50),info=growth_word(boot+48);
 if(!base || base>=capacity || old>capacity-base || old!=get32(boot+32) || !spc || (spc&(spc-1)) || boot[16]!=1 ||
    growth_word(boot+11)!=512 || !oldfat || reserved<8 || backup!=6 || info!=1 || uint64_t(reserved)+oldfat>=old ||
    original[510]!=0x55 || original[511]!=0xaa || memcmp(original+446,mbr+446,60))return false;
 // The MBR may already reflect the new size after interruption in commit.
 uint32_t size=capacity-base;
 uint8_t record[2][512];bool valid[2]{};
 for(unsigned i=0;i<2;++i){
  if(!disk.sector(43+i,record[i],false))return false;
  valid[i]=!memcmp(record[i],"GPR1",4) && equal(record[i]+24,intent+4,32) && growth_sealed(record[i]);
 }
 unsigned index=valid[1] && (!valid[0] || get32(record[1]+4)>get32(record[0]+4));
 uint32_t seq=0,phase=0,cursor=old;
 if(valid[index]){
  seq=get32(record[index]+4);phase=get32(record[index]+8);cursor=get32(record[index]+12);size=get32(record[index]+16);
  if(phase>3 || size<old || size>capacity-base)return false;
  if(phase==3)return valid_layout(disk) && get32(mbr+part+12)==size;
 }else if(!valid_layout(disk) || get32(mbr+part+12)!=old || !disk.sector(base,buffer,false) || memcmp(buffer,boot,512))return false;
 if(size==old)return true;
 if(get32(mbr+part+12)!=old && get32(mbr+part+12)!=size)return false;
 const uint32_t required=uint32_t((uint64_t(size-reserved)+uint64_t(spc)*2+127u*spc)/(128u*spc+1))+1;
 const uint32_t newfat=required>oldfat?required:oldfat,shift=newfat-oldfat;
 if(uint64_t(reserved)+newfat>=size || (size-reserved-newfat)/spc>0x0ffffff5u || shift>size-old)return false;
 const uint32_t old_end=2+(old-reserved-oldfat)/spc,new_end=2+(size-reserved-newfat)/spc;
 if(old_end>uint64_t(oldfat)*128 || new_end>uint64_t(newfat)*128)return false;
 auto checkpoint=[&](uint32_t next,uint32_t offset){
  uint8_t r[512]{};memcpy(r,"GPR1",4);put32(r+4,++seq);put32(r+8,next);put32(r+12,offset);put32(r+16,size);
  memcpy(r+24,intent+4,32);seal_growth(r);index=1-index;
  if(!disk.sector(43+index,r,true))return false;
  phase=next;cursor=offset;return true;
 };
 if(!seq && !checkpoint(0,old))return false;
 const uint32_t data=reserved+oldfat;
 if(phase==0){
  if(cursor>old || cursor<data)return false;
  uint32_t cached=~0u;
  while(shift && cursor>data){
   // Replay never reads a source overwritten in this checkpoint interval.
   uint32_t length=cursor-data;if(length>4096)length=4096;if(length>shift)length=shift;
   for(uint32_t pos=cursor;pos>cursor-length;){
    uint32_t cluster=(pos-1-data)/spc+2,sector=cluster/128;
    uint32_t start=data+(cluster-2)*spc;
    if(start<cursor-length)start=cursor-length;
    if(pos-start>8)start=pos-8;
    if(sector<oldfat){
     if(sector!=cached){if(!disk.sector(base+reserved+sector,fat,false))return false;cached=sector;}
     if(get32(fat+4*(cluster%128))&0x0fffffffu){
      uint8_t block[4096];
      if(!disk.sectors(base+start,block,pos-start,false) || !disk.sectors(base+start+shift,block,pos-start,true))return false;
     }
    }
    pos=start;
   }
   if(!checkpoint(0,cursor-length))return false;
  }
  if(!checkpoint(1,oldfat))return false;
 }
 if(phase==1){
  if(cursor<oldfat || cursor>newfat)return false;
  uint8_t zero[4096]{};
  while(cursor<newfat){
   uint32_t end=newfat-cursor>4096?cursor+4096:newfat;
   for(uint32_t n=cursor;n<end;){
    unsigned count=end-n>8?8:end-n;
    if(!disk.sectors(base+reserved+n,zero,count,true))return false;
    n+=count;
   }
   if(!checkpoint(1,end))return false;
  }
  if(!checkpoint(2,0))return false;
 }
 // Publish only after the complete new FAT and data area are durable.
 put32(boot+36,newfat);put32(boot+32,size);
 // Phase 2 may replay after either FSInfo copy was written. Derive the
 // original free count from the original FAT extent, never from those copies.
 // Phase 1 zeroed the newly allocated FAT sectors. Padding in the old FAT is
 // only now becoming addressable and may contain nonzero bytes.
 uint32_t free_count=new_end-old_end,next_free=0xffffffff;
 for(uint32_t sector=0;sector<oldfat && sector*128u<new_end;++sector){
  if(!disk.sector(base+reserved+sector,fat,false))return false;
  bool dirty=false;
  for(unsigned i=0;i<128;++i){
   const uint32_t cluster=sector*128u+i;
   if(cluster<2 || cluster>=new_end)continue;
   auto* entry=fat+i*4;
   const uint32_t value=get32(entry);
   if(cluster<old_end){
    if(value&0x0fffffffu)continue;
    ++free_count;
   }else if(value&0x0fffffffu){
    put32(entry,value&0xf0000000u);
    dirty=true;
   }
   if(next_free==0xffffffff)next_free=cluster;
  }
  if(dirty && !disk.sector(base+reserved+sector,fat,true))return false;
 }
 if(next_free==0xffffffff && new_end>old_end)next_free=old_end;
 memset(buffer,0,512);
 put32(buffer,0x41615252);put32(buffer+484,0x61417272);
 put32(buffer+488,free_count);put32(buffer+492,next_free);put32(buffer+508,0xaa550000);
 if(!disk.sector(base+backup+info,buffer,true) || !disk.sector(base+info,buffer,true) ||
    !disk.sector(base+backup,boot,true) || !disk.sector(base,boot,true))return false;
 put32(original+part+12,size);
 uint8_t marker[512]={'G','S','D','1'};Sha256::hash(original+446,64,marker+4);
 return disk.sector(0,original,true) && disk.sector(8,marker,true) && valid_layout(disk) && checkpoint(3,0);
}
}}
