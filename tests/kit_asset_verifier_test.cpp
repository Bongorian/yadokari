#include "kit_asset_verifier.h"
#include "wav_writer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>
#include <memory>
using namespace groove;
struct Files final:IFileAccess{
 std::vector<uint8_t> bytes;unsigned reads=0,largest=0;uint64_t rev=1;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t at,void* p,unsigned want,unsigned& got)override{++reads;largest=std::max(largest,want);if(at>bytes.size())return false;got=unsigned(std::min<uint64_t>(want,bytes.size()-at));std::memcpy(p,bytes.data()+at,got);return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}uint64_t size(const char*)override{return bytes.size();}bool exists(const char*)override{return true;}bool revision(const char*,FileRevision& r)override{r.size=bytes.size();r.modified=rev;return true;}
};
struct CrcLoadingLibrary final:ISampleLibrary{
 bool load(const char*,WavView&)override{return false;}WavView lookup(const char*)const override{return {};}bool prepare(const ProjectState&,WavView*)override{return false;}bool supports_incremental_load()const override{return true;}bool incremental_validates_crc(const char*,uint64_t size) override{return size<=8u*1024u*1024u;}
};
int main(){Files fs;fs.bytes.resize(44+48000*4);assert(wav_header(fs.bytes.data(),48000,48000));for(unsigned i=44;i<fs.bytes.size();++i)fs.bytes[i]=uint8_t(i*23);ProjectCrc crc;for(auto b:fs.bytes)crc.add(b);SampleAssetState assets[kSampleAssets]{};assert(copy_text(assets[0].path,sizeof assets[0].path,"a.wav"));assets[0].metadata.content_crc=crc.value();KitAssetVerifier verifier(fs,assets);KitAssetVerifier::Result state=KitAssetVerifier::Result::Working;while(state==KitAssetVerifier::Result::Working)state=verifier.service();assert(state==KitAssetVerifier::Result::Done && fs.largest<=kFileScanBufferBytes);FileRevision rev;assert(verifier.revision(0,rev) && rev.modified==1);
 CrcLoadingLibrary library;unsigned before=fs.reads;KitAssetVerifier deferred(fs,assets,&library);while((state=deferred.service())==KitAssetVerifier::Result::Working){}assert(state==KitAssetVerifier::Result::Done && fs.reads-before<10);
 KitAssetVerifier changed(fs,assets);assert(changed.service()==KitAssetVerifier::Result::Working);assert(changed.service()==KitAssetVerifier::Result::Working);++fs.rev;while((state=changed.service())==KitAssetVerifier::Result::Working){}assert(state==KitAssetVerifier::Result::Failed);
 fs.bytes[100]^=1;KitAssetVerifier corrupt(fs,assets);while((state=corrupt.service())==KitAssetVerifier::Result::Working){}assert(state==KitAssetVerifier::Result::Failed);
}
