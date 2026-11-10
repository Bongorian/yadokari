#include "sample_stream.h"
#include "wav_writer.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <unistd.h>
using namespace groove;
struct CountingFiles:DesktopFiles {
 unsigned reads=0,revisions=0,begins=0,ends=0,leased_reads=0;bool leased=false;
 bool begin_read_session(const char*)override{assert(!leased);leased=true;++begins;return true;}
 void end_read_session()override{assert(leased);leased=false;++ends;}
 bool read(const char* path,uint64_t at,void* dst,unsigned n,unsigned& got)override{++reads;leased_reads+=leased;return DesktopFiles::read(path,at,dst,n,got);}
 bool revision(const char* path,FileRevision& r)override{++revisions;return DesktopFiles::revision(path,r);}
};
int main(){
 char path[]="/tmp/mk3-stream-cache-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 CountingFiles fs;std::vector<uint8_t> wav(44+10000*4);assert(wav_header(wav.data(),10000,48000));assert(fs.write(path,0,wav.data(),wav.size(),true));
 SampleStream stream(fs);WavView v;assert(stream.open(path,v));fs.reads=fs.revisions=0;
 // This queues the next three pages, which are already resident. Servicing
 // those requests must not hit the filesystem or delay other streams/UI.
 float l,r;assert(stream.read_stereo(2047,l,r));stream.service();assert(!fs.reads && !fs.revisions && !fs.begins && !fs.ends);
 // A 95ms owner stall must be covered by requests made at page entry,
 // rather than waiting until only 256 frames remain. Exercise every DSP API.
 wav.resize(44+30000*4);assert(wav_header(wav.data(),30000,48000));assert(fs.write(path,0,wav.data(),wav.size(),true));
 for(unsigned api=0;api<3;++api){SampleStream ahead(fs);WavView av;assert(ahead.open(path,av));
  auto read=[&](unsigned frame){float a=0,b=0;if(api==0)return ahead.read_frame(frame,0,a);if(api==1)return ahead.read_stereo(frame,a,b);return ahead.read_block(frame,0,&a,1)==1;};
  assert(read(6144));unsigned begins=fs.begins,ends=fs.ends,leased=fs.leased_reads;ahead.service();assert(fs.begins==begins+1 && fs.ends==ends+1 && fs.leased_reads>leased && !fs.leased);
  for(unsigned frame=6144;frame<6144+4560;++frame)assert(read(frame));
  assert(ahead.underruns()==0);
 }
 // The optimization must not disable revision invalidation for old voices.
 uint8_t changed=1;assert(fs.write(path,wav.size()-1,&changed,1,false));assert(!stream.current());assert(!stream.read_stereo(0,l,r));assert(stream.underruns()==1);
 assert(fs.remove(path));std::puts("stream cache: resident prefetch has no file I/O; modified source invalidates voice");
}
