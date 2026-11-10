#pragma once
#include "file_access.h"
#include "project_codec.h"
#include "alloc_policy.h"
namespace groove {
// Main-loop-owned resident WAV reader. Cached bytes are immutable and stay
// owned by the caller; new bytes transfer to the caller only after validation.
class ChunkedSampleRead {
public:
 enum class Result:uint8_t {Idle,Working,Matched,Loaded,Failed};
 explicit ChunkedSampleRead(IFileAccess& files):fs_(files){}
 ~ChunkedSampleRead(){delete[] bytes_;}
 ChunkedSampleRead(const ChunkedSampleRead&)=delete;ChunkedSampleRead& operator=(const ChunkedSampleRead&)=delete;
 bool start(const char* path,const uint8_t* cached=nullptr,unsigned cached_size=0,bool allow_new=true,uint32_t expected_crc=0){
  if(result_==Result::Working || !path || !copy_text(path_,sizeof path_,path) || !scratch_.data()){return false;}clear();uint64_t length=fs_.size(path_);if(length<44 || length>0xffffffffu)return false;size_=unsigned(length);has_revision_=fs_.revision(path_,revision_);cached_=cached && cached_size==size_?cached:nullptr;allow_new_=allow_new;expected_crc_=expected_crc;crc_=ProjectCrc{};result_=Result::Working;if(!cached_ && !allocate())return false;return true;
 }
 Result service(){
  if(result_!=Result::Working)return result_;
  unsigned count=size_-offset_>kFileScanBufferBytes?kFileScanBufferBytes:size_-offset_,got=0;uint8_t* dest=cached_?scratch_.data():bytes_+offset_;
  if(!count || !fs_.read(path_,offset_,dest,count,got) || got!=count){fail();return result_;}
  if(cached_ && __builtin_memcmp(dest,cached_+offset_,count)){cached_=nullptr;offset_=0;crc_=ProjectCrc{};if(!allocate())return result_;return result_;}
  if(expected_crc_){for(unsigned i=0;i<count;++i)crc_.add(dest[i]);}offset_+=count;if(offset_!=size_)return result_;
  FileRevision now;if((expected_crc_ && crc_.value()!=expected_crc_) || fs_.size(path_)!=size_ || (has_revision_ && (!fs_.revision(path_,now) || !same_file_revision(revision_,now)))){fail();return result_;}
  if(cached_){result_=Result::Matched;return result_;}
  if(!parse_wav(bytes_,size_,view_)){fail();return result_;}result_=Result::Loaded;return result_;
 }
 bool take(uint8_t*& bytes,unsigned& size,WavView& view){if(result_!=Result::Loaded)return false;bytes=bytes_;size=size_;view=view_;bytes_=nullptr;result_=Result::Idle;return true;}
 void clear(){delete[] bytes_;bytes_=nullptr;cached_=nullptr;size_=offset_=0;view_={};result_=Result::Idle;}
private:
 bool allocate(){if(!allow_new_){fail();return false;}bytes_=alloc_array<uint8_t>(size_);if(!bytes_){fail();return false;}return true;}
 void fail(){delete[] bytes_;bytes_=nullptr;result_=Result::Failed;}
 IFileAccess& fs_;FileScanBuffer scratch_;char path_[192]{};uint8_t* bytes_=nullptr;const uint8_t* cached_=nullptr;unsigned size_=0,offset_=0;FileRevision revision_{};ProjectCrc crc_{};uint32_t expected_crc_=0;bool has_revision_=false,allow_new_=true;WavView view_{};Result result_=Result::Idle;
};
}
