#pragma once
#include "spsc_queue.h"
#include "wav_sampler.h"
#include "project.h"
#include "file_access.h"
namespace groove {
// Eight immutable-at-read PCM pages. Disk access and replacement run in service().
// CAS protects a page from replacement for the duration of one audio read.
class SampleStream:public ISampleReader {
 static constexpr unsigned Frames=2048,Pages=8;
 struct Page {uint32_t state=0,number=0;unsigned frames=0;uint8_t prefetch=0;float data[Frames*2]{};};
 IFileAccess& files_;char path_[160]{};FileRevision revision_{};bool revision_available_=false;uint32_t stale_=0;WavView format_{};uint64_t offset_=0;Page pages_[Pages];SpscQueue<uint32_t,65> requests_;uint32_t misses_=0;unsigned next_=0,service_checks_=0;uint32_t request_page_=~0u,retries_=0;uint8_t bytes_[Frames*8]{};
 bool fetch(uint32_t number,FileReadSession* session=nullptr){uint64_t first=uint64_t(number)*Frames;if(first>=format_.frames)return false;for(auto& p:pages_)if(__atomic_load_n(&p.state,__ATOMIC_ACQUIRE)==1 && p.number==number)return true;if(!current())return false;for(unsigned n=0;n<Pages;++n){Page& p=pages_[(next_+n)%Pages];unsigned expected=__atomic_load_n(&p.state,__ATOMIC_ACQUIRE);if(expected!=0 && expected!=1)continue;if(!__atomic_compare_exchange_n(&p.state,&expected,2,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;unsigned count=format_.frames-first<Frames?unsigned(format_.frames-first):Frames;unsigned got=0;if(session)session->begin(path_);bool ok=files_.read(path_,offset_+first*wav_frame_bytes(format_),bytes_,count*wav_frame_bytes(format_),got) && got==count*wav_frame_bytes(format_) && current();if(ok){WavView v=format_;v.data=bytes_;v.frames=count;v.reader=nullptr;for(unsigned i=0;i<count;++i){p.data[2*i]=wav_sample(v,i,0);p.data[2*i+1]=wav_sample(v,i,v.channels==2?1:0);}p.number=number;p.frames=count;p.prefetch=0;next_=(next_+n+1)%Pages;}++page_reads_;page_bytes_+=got;page_failures_+=!ok;__atomic_store_n(&p.state,ok?1u:0u,__ATOMIC_RELEASE);return ok;}return false;}
 uint64_t page_reads_=0,page_bytes_=0,page_failures_=0; // file-service owner only
public:
 struct Counters{uint64_t reads=0,bytes=0,failures=0;unsigned misses=0;};
 Counters counters()const{return {page_reads_,page_bytes_,page_failures_,underruns()};} // file-service owner
 explicit SampleStream(IFileAccess& f):files_(f){}
 bool open(const char* path,WavView& view){if(!copy_text(path_,sizeof path_,path))return false;revision_available_=files_.revision(path,revision_);if(!sample_file_header(files_,path,format_,offset_) || !current())return false;view=format_;view.reader=this;for(unsigned i=0;i<4;++i)fetch(i);unsigned last=(format_.frames-1)/Frames;fetch(last);return current();}
 bool current(){if(__atomic_load_n(&stale_,__ATOMIC_ACQUIRE))return false;if(!revision_available_)return true;FileRevision now;if(files_.revision(path_,now) && same_file_revision(revision_,now))return true;__atomic_store_n(&stale_,1,__ATOMIC_RELEASE);return false;}
 bool read_frame(uint32_t frame,unsigned ch,float& result)override{if(__atomic_load_n(&stale_,__ATOMIC_ACQUIRE)){__atomic_add_fetch(&misses_,1,__ATOMIC_RELAXED);result=0;return false;}uint32_t number=frame/Frames;for(auto& p:pages_){unsigned state=1;if(!__atomic_compare_exchange_n(&p.state,&state,3,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;bool match=p.number==number && frame%Frames<p.frames;if(match){result=p.data[(frame%Frames)*2+(ch?1:0)];if(!(p.prefetch&1) && requests_.free_space()>=3){p.prefetch|=1;requests_.push(number+1);requests_.push(number+2);requests_.push(number+3);}if(!(p.prefetch&2) && frame%Frames<128 && requests_.free_space()>=3){p.prefetch|=2;if(number)requests_.push(number-1);if(number>1)requests_.push(number-2);requests_.push(number+1);}}__atomic_store_n(&p.state,1,__ATOMIC_RELEASE);if(match)return true;}__atomic_add_fetch(&misses_,1,__ATOMIC_RELAXED);if(request_page_!=number || (++retries_%512)==0){request_page_=number;requests_.push(number);requests_.push(number+1);if(number)requests_.push(number-1);}result=0;return false;}

 bool read_stereo(uint32_t frame,float& left,float& right)override{
  uint32_t number=frame/Frames;
  if(!__atomic_load_n(&stale_,__ATOMIC_ACQUIRE))for(auto& p:pages_){unsigned state=1;if(!__atomic_compare_exchange_n(&p.state,&state,3,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;bool match=p.number==number && frame%Frames<p.frames;
   if(match){left=p.data[(frame%Frames)*2];right=p.data[(frame%Frames)*2+1];if(!(p.prefetch&1) && requests_.free_space()>=3){p.prefetch|=1;requests_.push(number+1);requests_.push(number+2);requests_.push(number+3);}if(!(p.prefetch&2) && frame%Frames<128 && requests_.free_space()>=3){p.prefetch|=2;if(number)requests_.push(number-1);if(number>1)requests_.push(number-2);requests_.push(number+1);}}
   __atomic_store_n(&p.state,1,__ATOMIC_RELEASE);if(match)return true;
  }
  __atomic_add_fetch(&misses_,1,__ATOMIC_RELAXED);if(request_page_!=number || (++retries_%512)==0){request_page_=number;requests_.push(number);requests_.push(number+1);if(number)requests_.push(number-1);}left=right=0;return false;
 }

 // One page claim per contiguous span; the DSP remains the sole request producer.
 unsigned read_block(uint32_t frame,unsigned ch,float* out,unsigned count)override{
  unsigned done=0;
  while(done<count){uint32_t at=frame+done,number=at/Frames;bool found=false;
   if(__atomic_load_n(&stale_,__ATOMIC_ACQUIRE)){__atomic_add_fetch(&misses_,1,__ATOMIC_RELAXED);return done;}
   for(auto& p:pages_){unsigned state=1;if(!__atomic_compare_exchange_n(&p.state,&state,3,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;
    if(p.number==number && at%Frames<p.frames){unsigned n=p.frames-at%Frames;if(n>count-done)n=count-done;for(unsigned i=0;i<n;++i)out[done+i]=p.data[(at%Frames+i)*2+(ch?1:0)];done+=n;found=true;
     if(!(p.prefetch&1) && requests_.free_space()>=3){p.prefetch|=1;requests_.push(number+1);requests_.push(number+2);requests_.push(number+3);}
     if(!(p.prefetch&2) && at%Frames<128 && requests_.free_space()>=3){p.prefetch|=2;if(number)requests_.push(number-1);if(number>1)requests_.push(number-2);requests_.push(number+1);}
    }
    __atomic_store_n(&p.state,1,__ATOMIC_RELEASE);if(found)break;
   }
   if(!found){float value;if(!read_frame(at,ch,value))return done;out[done++]=value;}
  }return done;
 }

 void service(){if((++service_checks_ & 63u)==0 && !current())return;FileReadSession session(files_);uint32_t n;for(unsigned budget=0;budget<8 && requests_.pop(n);++budget)fetch(n,&session);}
 uint32_t underruns()const{return __atomic_load_n(&misses_,__ATOMIC_RELAXED);}
};
}
