#pragma once
// Include the platform FatFs header before this adapter.
#include "../src/data_layout.h"

namespace groove {
inline bool fat_browser_entries(const char* path,const char* relative,FileEntry* out,unsigned capacity,unsigned& count){
 count=0;
 DIR directory;
 if(f_opendir(&directory,path)!=FR_OK)return false;
 FILINFO info;
 bool complete=false;
 while(f_readdir(&directory,&info)==FR_OK){
  if(!info.fname[0]){complete=true;break;}
  const bool folder=(info.fattrib&AM_DIR)!=0;
  if((info.fattrib&AM_HID) || !data_browser_candidate(relative,info.fname,folder,false))continue;
  if(count==capacity)break;
  FileEntry entry{};
  if(!copy_text(entry.name,sizeof entry.name,info.fname))continue;
  entry.directory=folder;entry.bytes=info.fsize;
  out[count++]=entry;
 }
 return f_closedir(&directory)==FR_OK && complete;
}
}
