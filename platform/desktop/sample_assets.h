#pragma once
#include "project.h"
#include "project_files.h"
#include "wav_sampler.h"
#include "file_access.h"
#include "sample_stream.h"
#include "soundfont.h"
#include "file_access.h"
#include <cstdio>
#include <vector>
#include <memory>
#include <string>
#include <sys/stat.h>
namespace groove {
class DesktopSampleAssets:public ISampleLibrary {
 struct Asset {std::string path;std::vector<uint8_t> bytes;WavView view;timespec modified{};};
public:
 const char* failure_reason()const override{return soundfonts_.failure()[0]?soundfonts_.failure():"Assets unavailable / sound kept";}
 void files(IFileAccess* f){files_=f;}
 void service()override{for(auto& s:streams_)s->service();}
 bool load(const char* path,WavView& result) override{return load_impl(path,result,false);}
 bool load_twin(const char* path,WavView& result) override{return load_impl(path,result,true);}
 bool load_impl(const char* path,WavView& result,bool resident) {
  if(files_ && soundfont_file_header(*files_,path))return soundfonts_.load(*files_,path,result);
  struct stat info{};if(stat(path,&info)!=0 || info.st_size<44 || info.st_size>1024LL*1024*1024)return false;
  for(const auto& s:stream_paths_)if(!resident && s.first==path && static_cast<SampleStream*>(s.second.reader)->current()){result=s.second;return true;}
  if(!resident && info.st_size>8*1024*1024 && files_){if(streams_.size()>=64)return false;auto stream=std::make_unique<SampleStream>(*files_);if(!stream->open(path,result))return false;stream_paths_.push_back({path,result});streams_.push_back(std::move(stream));return true;}
  for(const auto& asset:assets_)if(asset->path==path && asset->modified.tv_sec==info.st_mtim.tv_sec && asset->modified.tv_nsec==info.st_mtim.tv_nsec){result=asset->view;return true;}
  if(total_+size_t(info.st_size)>256*1024*1024)return false;
  FILE* file=std::fopen(path,"rb");if(!file)return false;auto asset=std::make_unique<Asset>();asset->path=path;asset->bytes.resize(size_t(info.st_size));bool ok=std::fread(asset->bytes.data(),1,asset->bytes.size(),file)==asset->bytes.size();if(std::fclose(file)!=0)ok=false;
  if(!ok || !parse_wav(asset->bytes.data(),asset->bytes.size(),asset->view))return false;
  asset->modified=info.st_mtim;result=asset->view;total_+=asset->bytes.size();assets_.push_back(std::move(asset));return true;
 }
 WavView lookup(const char* path)const override {auto sf=soundfonts_.lookup(path);if(sf.soundfont)return sf;for(auto i=stream_paths_.rbegin();i!=stream_paths_.rend();++i)if(i->first==path)return i->second;for(auto i=assets_.rbegin();i!=assets_.rend();++i)if((*i)->path==path)return (*i)->view;return {};}
 bool prepare(const ProjectState& project,WavView result[kSampleAssets]) {for(unsigned i=0;i<kSampleAssets;++i){result[i]={};if(project.assets[i].path[0] && !(twin_material(project,i)?load_twin(project.assets[i].path,result[i]):load(project.assets[i].path,result[i])))return false;}return true;}
 // Authoring boundary for built-in development examples, never a file-loader
 // compatibility path: record real resource identity before the first save.
 bool prepare_authored(ProjectState& project,WavView result[kSampleAssets]){if(!files_ || !prepare(project,result))return false;for(unsigned i=0;i<kSampleAssets;++i)if(project.assets[i].path[0]){auto& m=project.assets[i].metadata;if(!file_checksum(*files_,project.assets[i].path,m.content_crc))return false;if(!result[i].soundfont){m.rate=result[i].rate;m.frames=result[i].frames;m.channels=uint8_t(result[i].channels);m.bits=result[i].bits;}m.missing=false;}return true;}
private:SoundFontCache soundfonts_;IFileAccess* files_=nullptr;std::vector<std::unique_ptr<SampleStream>> streams_;std::vector<std::pair<std::string,WavView>> stream_paths_;std::vector<std::unique_ptr<Asset>> assets_;size_t total_=0;
};
}
