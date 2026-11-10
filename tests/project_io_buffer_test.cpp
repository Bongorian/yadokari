#include "project_files.h"
#include "benchmark_project.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <memory>
#include <string>
struct MemoryFiles final:groove::IFileAccess {
 std::vector<uint8_t> bytes;unsigned reads=0,writes=0,fail_write=0,fail_read=0,opened=0,closed=0,active=0;
 bool begin_read_session(const char*)override{if(active)return false;++opened;++active;return true;}void end_read_session()override{assert(active==1);++closed;--active;}
 unsigned list(const char*,groove::FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t at,void* data,unsigned count,unsigned& got)override{++reads;if(reads==fail_read || at>bytes.size())return false;got=unsigned(std::min<uint64_t>(count,bytes.size()-at));if(got)std::memcpy(data,bytes.data()+at,got);return true;}
 bool write(const char*,uint64_t at,const void* data,unsigned count,bool truncate)override{++writes;if(writes==fail_write)return false;if(truncate)bytes.clear();if(at!=bytes.size())return false;auto p=static_cast<const uint8_t*>(data);bytes.insert(bytes.end(),p,p+count);return true;}
 bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 uint64_t size(const char*)override{return bytes.size();}bool exists(const char*)override{return true;}
};
struct MappedFiles final:groove::IFileAccess {
 std::map<std::string,std::vector<uint8_t>> files;unsigned reads=0,writes=0;
 unsigned list(const char*,groove::FileEntry*,unsigned)override{return 0;}
 bool read(const char* name,uint64_t at,void* data,unsigned count,unsigned& got)override{++reads;auto it=files.find(name);if(it==files.end() || at>it->second.size())return false;got=unsigned(std::min<uint64_t>(count,it->second.size()-at));if(got)std::memcpy(data,it->second.data()+at,got);return true;}
 bool write(const char* name,uint64_t at,const void* data,unsigned count,bool truncate)override{++writes;auto& bytes=files[name];if(truncate)bytes.clear();if(at!=bytes.size())return false;auto p=static_cast<const uint8_t*>(data);bytes.insert(bytes.end(),p,p+count);return true;}
 bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 uint64_t size(const char* name)override{auto it=files.find(name);return it==files.end()?0:it->second.size();}bool exists(const char* name)override{return files.count(name)!=0;}
};
int main(){using namespace groove;constexpr unsigned n=kProjectIoBufferBytes*2+137;std::vector<uint8_t> expected(n),actual(n);for(unsigned i=0;i<n;++i)expected[i]=uint8_t((i*73u+19u)&255);MemoryFiles fs;{FileWriter writer(fs,"test");for(unsigned i=0;i<333;++i)assert(writer.write(&expected[i],1));assert(writer.write(expected.data()+333,n-333));assert(writer.finish());}assert(fs.bytes==expected && fs.writes==3);{FileReader reader(fs,"test");assert(reader.read(actual.data(),19));assert(reader.read(actual.data()+19,n-19));assert(reader.at_end());uint8_t byte;assert(!reader.read(&byte,1));}assert(actual==expected && fs.reads==4 && fs.opened==1 && fs.closed==1 && !fs.active);MemoryFiles broken;broken.bytes=expected;broken.fail_read=1;uint32_t failed_crc=0;assert(!file_checksum(broken,"test",failed_crc) && broken.opened==1 && broken.closed==1 && !broken.active);{FileReader reader(broken,"test");broken.fail_read=broken.reads+1;uint8_t byte;assert(!reader.read(&byte,1));}assert(broken.opened==2 && broken.closed==2 && !broken.active);MemoryFiles failing;failing.fail_write=2;{FileWriter writer(failing,"test");assert(!writer.write(expected.data(),n));}assert(failing.writes==2 && failing.bytes.size()==kProjectIoBufferBytes);
 MappedFiles files;files.files["source"]=expected;assert(copy_file(files,"source","copy"));assert(files.files["copy"]==expected && files.writes==3);assert(same_file(files,"source","copy"));assert(cached_file_matches(files,"copy",expected.data(),expected.size()));uint32_t crc=0;ProjectCrc c;for(uint8_t b:expected)c.add(b);assert(file_checksum(files,"source",crc) && crc==c.value());assert(copy_file_prefix(files,"source","prefix",40001));assert(files.files["prefix"].size()==40001);files.files["copy"][40000]^=1;assert(!same_file(files,"source","copy") && !cached_file_matches(files,"copy",expected.data(),expected.size()));MemoryFiles wrong_benchmark;wrong_benchmark.bytes.resize(kBenchmarkFileBytes);assert(!benchmark_file_matches(wrong_benchmark,"benchmark"));assert(wrong_benchmark.reads==(kBenchmarkFileBytes+kFileScanBufferBytes-1)/kFileScanBufferBytes);auto project=std::make_unique<ProjectState>();make_benchmark_project(*project);MemoryFiles valid_benchmark;{FileWriter writer(valid_benchmark,"benchmark");assert(encode_project(writer,*project) && writer.finish());}assert(valid_benchmark.bytes.size()==kBenchmarkFileBytes && benchmark_file_matches(valid_benchmark,"benchmark"));assert(valid_benchmark.reads==(kBenchmarkFileBytes+kFileScanBufferBytes-1)/kFileScanBufferBytes);
 puts("project buffering, scans, copies, boundaries and write failure passed");}
