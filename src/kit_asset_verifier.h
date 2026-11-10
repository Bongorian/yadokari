#pragma once
#include "file_access.h"
#include "soundfont.h"
#include "project_codec.h"
namespace groove {
// Main-loop owned. Large checksums are split into bounded SD reads; WAV
// headers are still inspected synchronously by the existing RIFF parser.
class KitAssetVerifier {
public:
 enum class Result:uint8_t {Working,Done,Failed};
 KitAssetVerifier(IFileAccess& fs,const SampleAssetState* assets,ISampleLibrary* library=nullptr):fs_(fs),assets_(assets),library_(library){}
 Result service(){
  if(!buffer_.data() || !assets_)return Result::Failed;
  if(slot_>=kSampleAssets)return Result::Done;
  const auto& asset=assets_[slot_];if(!asset.path[0]){++slot_;return Result::Working;}
  if(phase_==Phase::Start){size_=fs_.size(asset.path);if(!size_)return Result::Failed;offset_=0;crc_=ProjectCrc{};has_revision_=fs_.revision(asset.path,revision_);phase_=asset.metadata.content_crc && !(library_ && library_->supports_incremental_load() && library_->incremental_validates_crc(asset.path,size_))?Phase::Read:Phase::Header;return Result::Working;}
  if(phase_==Phase::Read){unsigned want=unsigned(size_-offset_>kFileScanBufferBytes?kFileScanBufferBytes:size_-offset_),got=0;if(!want || !fs_.read(asset.path,offset_,buffer_.data(),want,got) || got!=want)return Result::Failed;for(unsigned i=0;i<got;++i)crc_.add(buffer_.data()[i]);offset_+=got;if(offset_==size_){if(crc_.value()!=asset.metadata.content_crc)return Result::Failed;phase_=Phase::Header;}return Result::Working;}
  FileRevision current;WavView wav;uint64_t offset=0;if(fs_.size(asset.path)!=size_ || (has_revision_ && (!fs_.revision(asset.path,current) || !same_file_revision(revision_,current))) || (!sample_file_header(fs_,asset.path,wav,offset) && !soundfont_file_header(fs_,asset.path)))return Result::Failed;
  revisions_[slot_]=revision_;has_revisions_[slot_]=has_revision_;++slot_;phase_=Phase::Start;return Result::Working;
 }
 bool revision(unsigned slot,FileRevision& value)const{if(slot>=kSampleAssets || !has_revisions_[slot])return false;value=revisions_[slot];return true;}
private:
 enum class Phase:uint8_t {Start,Read,Header};
 IFileAccess& fs_;const SampleAssetState* assets_;ISampleLibrary* library_;FileScanBuffer buffer_;ProjectCrc crc_{};FileRevision revision_{},revisions_[kSampleAssets]{};bool has_revisions_[kSampleAssets]{};uint64_t size_=0,offset_=0;unsigned slot_=0;bool has_revision_=false;Phase phase_=Phase::Start;
};
inline bool kit_same_asset(const SampleAssetState& a,const SampleAssetState& b){return !__builtin_strcmp(a.path,b.path) && same_sample_metadata(a.metadata,b.metadata);}
}
