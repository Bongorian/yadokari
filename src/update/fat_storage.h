#pragma once
#include "manager.h"
#include <ff.h>
namespace groove { namespace update {
class BlockDevice {public:virtual ~BlockDevice(){};virtual bool sector(unsigned lba,uint8_t* bytes,bool write)=0;};
// Fixed, provisioned MBR layout. Sector 8 marker prevents interpreting an
// ordinary single-partition SD card as an update target. No online repartition.
inline bool valid_layout(BlockDevice& disk){
 uint8_t mbr[512],marker[512];if(!disk.sector(0,mbr,false)||!disk.sector(8,marker,false)||mbr[510]!=0x55||mbr[511]!=0xaa||marker[0]!='G'||marker[1]!='S'||marker[2]!='D'||marker[3]!='1')return false;
 uint8_t digest[32];Sha256::hash(mbr+446,64,digest);if(!equal(digest,marker+4,32))return false;
 uint32_t previous=2048;for(unsigned i=0;i<4;++i){const auto* p=mbr+446+i*16;uint32_t start=get32(p+8),size=get32(p+12);if((p[4]!=0x0c&&p[4]!=0x0e&&p[4]!=0x06)||start<previous||size<32768||start>0xffffffff-size)return false;previous=start+size;}return true;
}
class FatStorage:public Storage {
 BlockDevice& disk_;const char* drives_[3];FATFS fs_[3]{};FIL file_{};bool open_=false,writing_=false;
 bool path(unsigned slot,char out[32]){if(slot>1)return false;unsigned n=0;const char* d=drives_[slot+1];while(*d&&n<18)out[n++]=*d++;if(*d)return false;const char* f="/KERNEL.IMG";while(*f)out[n++]=*f++;out[n]=0;return true;}
public:
 FatStorage(BlockDevice& disk,const char* recovery,const char* a,const char* b):disk_(disk){drives_[0]=recovery;drives_[1]=a;drives_[2]=b;}
 bool mount(){if(!valid_layout(disk_))return false;bool recovery=false;for(unsigned i=0;i<3;++i){bool mounted=f_mount(&fs_[i],drives_[i],1)==FR_OK;if(i==0)recovery=mounted;}return recovery;}
 bool read_journal(unsigned copy,uint8_t out[512])override{return copy<2&&disk_.sector(copy?32:16,out,false);}
 bool write_journal(unsigned copy,const uint8_t in[512])override {uint8_t aligned[512];for(unsigned i=0;i<512;++i)aligned[i]=in[i];return copy<2&&disk_.sector(copy?32:16,aligned,true);}
 bool open_image(unsigned slot,bool write)override{char name[32];if(open_||!path(slot,name))return false;open_=f_open(&file_,name,write?(FA_WRITE|FA_CREATE_ALWAYS):(FA_READ|FA_OPEN_EXISTING))==FR_OK;writing_=write;return open_;}
 bool write_image(const uint8_t* p,unsigned n)override{UINT done=0;return open_&&writing_&&f_write(&file_,p,n,&done)==FR_OK&&done==n;}
 bool read_image(uint8_t* p,unsigned n,unsigned& got)override{UINT done=0;bool ok=open_&&!writing_&&f_read(&file_,p,n,&done)==FR_OK;got=done;return ok;}
 bool close_image()override{if(!open_)return false;bool ok=!writing_||f_sync(&file_)==FR_OK;if(f_close(&file_)!=FR_OK)ok=false;open_=false;return ok;}
};
}}
