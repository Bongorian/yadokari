#include <ff.h>
#include <diskio.h>
#include "../firmware/fat_browser.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace groove;

// A formatted RAM disk exercises the same f_readdir/LFN path as the board.
static constexpr unsigned sectors=131072;
static std::vector<BYTE> disk(sectors*512);
extern "C" {
PARTITION VolToPart[FF_VOLUMES]{};
DSTATUS disk_initialize(BYTE drive){return drive?STA_NOINIT:0;}
DSTATUS disk_status(BYTE drive){return disk_initialize(drive);}
DRESULT disk_read(BYTE drive,BYTE* out,LBA_t sector,UINT count){
 if(drive || sector>=sectors || count>sectors-sector)return RES_PARERR;
 std::memcpy(out,disk.data()+sector*512,count*512);return RES_OK;
}
DRESULT disk_write(BYTE drive,const BYTE* in,LBA_t sector,UINT count){
 if(drive || sector>=sectors || count>sectors-sector)return RES_PARERR;
 std::memcpy(disk.data()+sector*512,in,count*512);return RES_OK;
}
DRESULT disk_ioctl(BYTE drive,BYTE cmd,void* out){
 if(drive)return RES_PARERR;
 switch(cmd){
 case CTRL_SYNC:return RES_OK;
 case GET_SECTOR_COUNT:*static_cast<LBA_t*>(out)=sectors;return RES_OK;
 case GET_BLOCK_SIZE:*static_cast<DWORD*>(out)=1;return RES_OK;
 default:return RES_PARERR;
 }
}
}
static void touch(const char* name){
 FIL f;assert(f_open(&f,name,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);assert(f_close(&f)==FR_OK);
}
int main(){
 BYTE work[4096];MKFS_PARM format{FM_FAT32|FM_SFD,0,0,0,0};FATFS fs{};
 assert(f_mkfs("0:",&format,work,sizeof work)==FR_OK && f_mount(&fs,"0:",1)==FR_OK);
 assert(f_mkdir("PROJECTS")==FR_OK && f_mkdir("GROOVE")==FR_OK);
 touch("PROJECTS/Final take.MGXS.TMP");touch("PROJECTS/Other.mgxs.tmp");
 touch("PROJECTS/Extra.tmp");touch("PROJECTS/Kit.mgxk.tmp");touch("PROJECTS/Hidden.mgxs.tmp");
 assert(f_chmod("PROJECTS/Hidden.mgxs.tmp",AM_HID,AM_HID)==FR_OK);
 touch("GROOVE/Session.mgxs.tmp");touch("BENCH.MGXS.tmp");
 FileEntry entries[2];unsigned count=0;
 assert(!fat_browser_entries("0:/PROJECTS","PROJECTS",entries,1,count) && count==1);
 assert(fat_browser_entries("0:/PROJECTS","PROJECTS",entries,2,count) && count==2);
 bool upper=false,lower=false;
 for(const auto& entry:entries){upper|=!std::strcmp(entry.name,"Final take.MGXS.TMP");lower|=!std::strcmp(entry.name,"Other.mgxs.tmp");}
 assert(upper && lower && f_stat("PROJECTS/Final take.MGXS.tmp",nullptr)==FR_OK);
 assert(fat_browser_entries("0:/GROOVE","GROOVE",entries,2,count) && !count);
 assert(fat_browser_entries("0:/","",entries,2,count) && count==1 && !std::strcmp(entries[0].name,"PROJECTS"));
 assert(f_mount(nullptr,"0:",0)==FR_OK);
 std::puts("FatFs browser PASS: staged Songs, case, hidden/system files and capacity retry");
}
