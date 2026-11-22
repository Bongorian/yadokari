#pragma once
#include "project_files.h"
#include "data_layout.h"

namespace groove {
// File-task only. Validate the assets of the frozen save, yielding between
// checksum blocks, directory listings and entries so recording can drain.
class SaveAssetResolver {
public:
 enum class Result { Working, Ready, Failed };
 explicit SaveAssetResolver(IFileAccess& files):fs_(files){}
 ~SaveAssetResolver(){reset();}
 void reset(){
  close_read();
  while(depth_)pop_directory();
  assets_=nullptr;missing_=0;slot_=files_seen_=folders_seen_=0;scanning_=false;
 }
 void start(SampleAssetState* assets){
  reset();assets_=assets;
  for(unsigned i=0;i<kSampleAssets;++i)if(assets_[i].path[0])missing_|=uint64_t(1)<<i;
 }
 bool needed()const{return missing_!=0;}
 Result service(uint8_t* buffer){
  if(!missing_)return Result::Ready;
  if(!buffer)return Result::Failed;
  if(reading_)return read_block(buffer);
  if(!scanning_){
   while(slot_<kSampleAssets){
    unsigned i=slot_++;
    if(!(missing_&(uint64_t(1)<<i)))continue;
    char path[192];
    if(canonical_path(path,sizeof path,"",assets_[i].path) && !__builtin_strcmp(path,assets_[i].path) && !data_unpublished_asset(path))begin_read(path);
    return Result::Working;
   }
   scanning_=true;
   return push_directory("");
  }
  if(!depth_)return Result::Failed;
  auto& dir=dirs_[depth_-1];
  FileEntry entry{};
  if(dir.count>=4096 || !dir.cursor->next(entry)){pop_directory();return Result::Working;}
  ++dir.count;char path[192];
  if(!entry.name[0] || entry.name[0]=='.' || !join_path(path,sizeof path,dir.path,entry.name))return Result::Working;
  if(entry.directory){
   if(data_equal(path,"SYSTEM") || data_equal(path,"UPDATES") || data_equal(path,"GROOVE") || data_equal(path,"firmware"))return Result::Working;
   if(data_stem_staging_name(entry.name,__builtin_strlen(entry.name)))return Result::Working;
   return push_directory(path);
  }
  if(++files_seen_>32768)return Result::Failed;
  if(data_unpublished_asset(path) || __builtin_strlen(path)>=sizeof assets_[0].path)return Result::Working;
  uint8_t header[12];unsigned got=0;
  if(fs_.read(path,0,header,sizeof header,got) && got==sizeof header && wav_tag(header,"RIFF") &&
     (wav_tag(header+8,"WAVE") || wav_tag(header+8,"sfbk")))begin_read(path);
  return Result::Working;
 }
private:
 struct Directory {char path[192]{};DirectoryCursor* cursor=nullptr;unsigned count=0;};
 Result push_directory(const char* path){
  if(depth_==9)return Result::Working;
  if(++folders_seen_>4096)return Result::Failed;
  auto& dir=dirs_[depth_];dir.cursor=fs_.open_directory_cursor(path);
  if(!dir.cursor)return depth_?Result::Working:Result::Failed;
  ++depth_;copy_text(dir.path,sizeof dir.path,path);
  return Result::Working;
 }
 void pop_directory(){auto& dir=dirs_[--depth_];delete dir.cursor;dir=Directory{};}
 void close_read(){if(owned_)fs_.end_read_session();owned_=reading_=false;}
 void begin_read(const char* path){
  size_=fs_.size(path);if(!size_)return;
  copy_text(path_,sizeof path_,path);at_=0;crc_=ProjectCrc{};
  has_revision_=fs_.revision(path_,revision_);owned_=fs_.begin_read_session(path_);reading_=true;
 }
 Result read_block(uint8_t* buffer){
  unsigned count=unsigned(size_-at_>kFileScanBufferBytes?kFileScanBufferBytes:size_-at_),got=0;
  if(!fs_.read(path_,at_,buffer,count,got) || got!=count){close_read();return Result::Working;}
  for(unsigned n=0;n<got;++n)crc_.add(buffer[n]);
  at_+=got;if(at_<size_)return Result::Working;
  FileRevision current;close_read();
  if(fs_.size(path_)!=size_ || (has_revision_ && (!fs_.revision(path_,current) || !same_file_revision(current,revision_))))return Result::Working;
  if(!scanning_){
   // A stable direct read verifies every slot pointing to these exact bytes.
   for(unsigned i=slot_-1;i<kSampleAssets;++i){
    if(assets_[i].metadata.content_crc==crc_.value() && !__builtin_strcmp(assets_[i].path,path_))
     missing_&=~(uint64_t(1)<<i);
   }
  }else for(unsigned i=0;i<kSampleAssets;++i){
   if((missing_&(uint64_t(1)<<i)) && assets_[i].metadata.content_crc==crc_.value()){
    copy_text(assets_[i].path,sizeof assets_[i].path,path_);missing_&=~(uint64_t(1)<<i);
   }
  }
  return missing_?Result::Working:Result::Ready;
 }
 IFileAccess& fs_;SampleAssetState* assets_=nullptr;Directory dirs_[9]{};
 uint64_t missing_=0,at_=0,size_=0;unsigned slot_=0,depth_=0,files_seen_=0,folders_seen_=0;
 bool scanning_=false,reading_=false,owned_=false,has_revision_=false;
 char path_[192]{};ProjectCrc crc_;FileRevision revision_{};
};
}
