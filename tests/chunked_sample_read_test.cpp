#include "chunked_sample_read.h"
#include "wav_writer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>
using namespace groove;
struct Files final:IFileAccess{
 std::vector<uint8_t> data;unsigned reads=0,largest=0;uint64_t rev=1;bool fail=false;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t at,void* p,unsigned n,unsigned& got)override{++reads;largest=std::max(largest,n);if(fail || at>data.size())return false;got=unsigned(std::min<uint64_t>(n,data.size()-at));std::memcpy(p,data.data()+at,got);return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}uint64_t size(const char*)override{return data.size();}bool exists(const char*)override{return true;}bool revision(const char*,FileRevision& r)override{r.size=data.size();r.modified=rev;return true;}
};
int main(){Files fs;fs.data.resize(44+48000*4);assert(wav_header(fs.data.data(),48000,48000));for(unsigned i=44;i<fs.data.size();++i)fs.data[i]=uint8_t(i*13);ProjectCrc crc;for(auto b:fs.data)crc.add(b);ChunkedSampleRead loader(fs);assert(loader.start("sample.wav",nullptr,0,true,crc.value()));ChunkedSampleRead::Result result;do{unsigned before=fs.reads;result=loader.service();assert(fs.reads-before<=1);}while(result==ChunkedSampleRead::Result::Working);assert(result==ChunkedSampleRead::Result::Loaded && fs.largest<=kFileScanBufferBytes);uint8_t* bytes=nullptr;unsigned size=0;WavView view;assert(loader.take(bytes,size,view) && size==fs.data.size() && view.frames==48000 && !std::memcmp(bytes,fs.data.data(),size));assert(loader.start("sample.wav",bytes,size,true,crc.value()));while((result=loader.service())==ChunkedSampleRead::Result::Working){}assert(result==ChunkedSampleRead::Result::Matched);assert(loader.start("sample.wav",bytes,size,true,crc.value()^1u));while((result=loader.service())==ChunkedSampleRead::Result::Working){}assert(result==ChunkedSampleRead::Result::Failed);
 fs.data[100]^=1;assert(loader.start("sample.wav",nullptr,0,true,crc.value()));while((result=loader.service())==ChunkedSampleRead::Result::Working){}assert(result==ChunkedSampleRead::Result::Failed);assert(loader.start("sample.wav",bytes,size,false));assert(loader.service()==ChunkedSampleRead::Result::Failed);assert(loader.start("sample.wav",bytes,size));while((result=loader.service())==ChunkedSampleRead::Result::Working){}assert(result==ChunkedSampleRead::Result::Loaded);uint8_t* replacement=nullptr;assert(loader.take(replacement,size,view) && replacement[100]==fs.data[100]);delete[] replacement;
 assert(loader.start("sample.wav",bytes,size));assert(loader.service()==ChunkedSampleRead::Result::Working);++fs.rev;while((result=loader.service())==ChunkedSampleRead::Result::Working){}assert(result==ChunkedSampleRead::Result::Failed);fs.fail=true;assert(loader.start("sample.wav"));assert(loader.service()==ChunkedSampleRead::Result::Failed);delete[] bytes;}
