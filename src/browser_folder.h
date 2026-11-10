#pragma once
#include "file_access.h"
#include "alloc_policy.h"
namespace groove {
// File-owner only. Read a complete directory before filtering/sorting/windowing.
// Failure is explicit; a 64-entry viewport is never an enumeration limit.
class BrowserFolder {
 FileEntry* entries_=nullptr;
 static bool less(const FileEntry& a,const FileEntry& b){if(a.directory!=b.directory)return a.directory;return __builtin_strcmp(a.name,b.name)<0;}
 void sift(unsigned root,unsigned end){for(;;){unsigned child=root*2+1;if(child>=end)return;if(child+1<end && less(entries_[child],entries_[child+1]))++child;if(!less(entries_[root],entries_[child]))return;auto tmp=entries_[root];entries_[root]=entries_[child];entries_[child]=tmp;root=child;}}
public:
 unsigned count=0;bool memory_failure=false;
 ~BrowserFolder(){delete[] entries_;}
 template<class Filter>bool read(IFileAccess& fs,const char* directory,Filter keep){
  for(unsigned capacity=64;;){delete[] entries_;entries_=nullptr;if(!allocation_admitted(sizeof(FileEntry),capacity) || !(entries_=alloc_array<FileEntry>(capacity))){memory_failure=true;return false;}
   count=0;if(fs.list_browser(directory,entries_,capacity,count))break;
   if(count!=capacity)return false;
   if(capacity>unsigned(-1)/2/sizeof(FileEntry)){memory_failure=true;return false;}capacity*=2;
  }
  unsigned n=0;for(unsigned i=0;i<count;++i)if(keep(entries_[i]))entries_[n++]=entries_[i];count=n;
  for(unsigned i=count/2;i>0;--i)sift(i-1,count);
  for(unsigned end=count;end>1;){--end;auto tmp=entries_[0];entries_[0]=entries_[end];entries_[end]=tmp;sift(0,end);}return true;
 }
 const FileEntry& entry(unsigned i)const{return entries_[i];}
 unsigned page(unsigned cursor,FileEntry* out,unsigned capacity)const{unsigned offset=capacity?cursor/capacity*capacity:0,n=0;while(offset+n<count && n<capacity){out[n]=entries_[offset+n];++n;}return n;}
};
}
