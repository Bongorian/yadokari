#include "drum_gm.h"
#include "soundfont.h"
#include "file_access.h"
#include "project_codec.h"
#include <stdlib.h>
#include <string.h>
namespace {
struct alignas(16) SfAllocation {size_t bytes;bool high;};
size_t sf_bytes=0;unsigned sf_allocations=0;
void* sf_alloc(size_t n){constexpr size_t budget=512u*1024u*1024u,largest=512u*1024u*1024u;if(n>largest-sizeof(SfAllocation) || n>budget-__atomic_load_n(&sf_bytes,__ATOMIC_RELAXED))return nullptr;bool high=false;unsigned char* raw=nullptr;
#if !defined(__linux__)
 if(n>=1024u*1024u){high=true;raw=static_cast<unsigned char*>(CMemorySystem::HeapTryAllocate(n+sizeof(SfAllocation),HEAP_HIGH));}
 else
#endif
 raw=groove::alloc_uninitialized_bytes(n+sizeof(SfAllocation));
 if(!raw){return nullptr;}auto* h=reinterpret_cast<SfAllocation*>(raw);h->bytes=n;h->high=high;__atomic_add_fetch(&sf_bytes,n,__ATOMIC_RELAXED);__atomic_add_fetch(&sf_allocations,1u,__ATOMIC_RELAXED);return h+1;}
void sf_free(void* p){if(!p)return;auto* h=static_cast<SfAllocation*>(p)-1;__atomic_sub_fetch(&sf_bytes,h->bytes,__ATOMIC_RELAXED);
#if !defined(__linux__)
 if(h->high){CMemorySystem::HeapFree(h);return;}
#endif
 delete[] reinterpret_cast<unsigned char*>(h);}
void* sf_realloc(void* p,size_t n){if(!p)return sf_alloc(n);auto* h=static_cast<SfAllocation*>(p)-1;if(n<=h->bytes)return p;void* out=sf_alloc(n);if(out){__builtin_memcpy(out,p,h->bytes);sf_free(p);}return out;}
}
#define TSF_MALLOC sf_alloc
#define TSF_REALLOC sf_realloc
#define TSF_FREE sf_free
#define TSF_NO_STDIO
#define TSF_IMPLEMENTATION
#include "../third_party/tinysoundfont/tsf.h"
namespace groove {
unsigned soundfont_allocation_count(){return __atomic_load_n(&sf_allocations,__ATOMIC_RELAXED);}
// The retained resource owns shared sample data and one preallocated context per
// engine voice. File-service publishes once; only the DSP owner mutates contexts.
constexpr unsigned kSfVoices=48,kSfRegions=32;
struct SfProgram {unsigned bank=0,program=0;char name[21]{};};
struct SoundFontBank {SoundFontBank* next=nullptr;char path[96]{};uint32_t crc=0;tsf* voices[kSfVoices]{};SfProgram* programs=nullptr;unsigned program_count=0;size_t bytes=0;uint32_t state=0,last_use=0;
 void release(){for(auto*& f:voices){if(f)tsf_close(f);f=nullptr;}bytes=0;}
 ~SoundFontBank(){release();delete[] programs;}
};
static uint32_t sf_use_clock=0;
size_t soundfont_resident_bytes(){return __atomic_load_n(&sf_bytes,__ATOMIC_RELAXED);}
uint32_t soundfont_crc(SoundFontBank* b){return b?b->crc:0;}
bool soundfont_resident(SoundFontBank* b){return b && __atomic_load_n(&b->state,__ATOMIC_ACQUIRE)>=2;}
// state 0 = evicted, 2 = resident without readers, +2 for each sounding voice.
static bool pin(SoundFontBank* b){if(!b)return false;uint32_t state=__atomic_load_n(&b->state,__ATOMIC_ACQUIRE);while(state>=2){if(__atomic_compare_exchange_n(&b->state,&state,state+2,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){__atomic_store_n(&b->last_use,__atomic_add_fetch(&sf_use_clock,1,__ATOMIC_RELAXED),__ATOMIC_RELAXED);return true;}}return false;}

namespace {
uint32_t checksum(IFileAccess& fs,const char* path,bool& ok){FileScanBuffer buffer;uint64_t size=fs.size(path);ProjectCrc crc;ok=buffer.data() && size>0;for(uint64_t at=0;ok && at<size;){unsigned n=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),got=0;ok=fs.read(path,at,buffer.data(),n,got) && got==n;for(unsigned i=0;ok && i<n;++i)crc.add(buffer.data()[i]);at+=n;}return crc.value();}
// File-service read-ahead. Tiny RIFF scalar reads must not become FatFs
// open/seek/read/close transactions. Both whole-file CRC checks are retained.
struct Stream {
 IFileAccess& fs;const char* path;uint64_t at=0,size=0;bool failed=false;
 static constexpr unsigned capacity=65536;
 unsigned char* scratch=alloc_uninitialized_bytes(capacity);uint64_t base=0;unsigned used=0;
 ~Stream(){delete[] scratch;}
 static int read(void* p,void* out,unsigned n){
  auto& s=*static_cast<Stream*>(p);if(!s.scratch || s.at>s.size || n>s.size-s.at){s.failed=true;return 0;}
  unsigned total=0;
  while(total<n){
   if(s.at<s.base || s.at-s.base>=s.used){
    s.base=s.at;unsigned wanted=unsigned(s.size-s.at>capacity?capacity:s.size-s.at),got=0;
    if(!wanted || !s.fs.read(s.path,s.at,s.scratch,wanted,got) || got!=wanted){s.failed=true;return 0;}
    s.used=got;
   }
   unsigned offset=unsigned(s.at-s.base),count=s.used-offset;if(count>n-total)count=n-total;
   __builtin_memcpy(static_cast<unsigned char*>(out)+total,s.scratch+offset,count);s.at+=count;total+=count;
  }
  return int(n);
 }
 static int skip(void* p,unsigned n){auto& s=*static_cast<Stream*>(p);if(s.at>s.size || n>s.size-s.at){s.failed=true;return 0;}s.at+=n;return 1;}
};
void clear(tsf* f){if(f)for(int i=0;i<f->voiceNum;++i)tsf_voice_kill(f->voices+i);}
}

SoundFontCache::~SoundFontCache(){while(head_){auto* p=head_;head_=head_->next;delete p;}}
WavView SoundFontCache::lookup(const char* path)const{for(auto* p=head_;p;p=p->next)if(!strcmp(p->path,path)){WavView out;out.soundfont=p;return out;}return {};}
unsigned SoundFontCache::evict_unused(size_t required){
 unsigned removed=0;for(;;){size_t used=0;SoundFontBank* oldest=nullptr;for(auto* p=head_;p;p=p->next){used+=p->bytes;if(__atomic_load_n(&p->state,__ATOMIC_ACQUIRE)==2 && (!oldest || __atomic_load_n(&p->last_use,__ATOMIC_RELAXED)<__atomic_load_n(&oldest->last_use,__ATOMIC_RELAXED)))oldest=p;}
  if(required && required<=budget_ && used<=budget_-required)break;
  if(!oldest){break;}uint32_t expected=2;if(!__atomic_compare_exchange_n(&oldest->state,&expected,0,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;oldest->release();++removed;
 }return removed;
}
bool SoundFontCache::load(IFileAccess& fs,const char* path,WavView& out){
 failure_="Invalid SF2";if(!soundfont_file_header(fs,path))return false;uint64_t estimate=fs.size(path)*2+4u*1024u*1024u;if(estimate>budget_){failure_="SF2 memory budget / sound kept";return false;}bool ok=false;uint32_t crc=checksum(fs,path,ok);if(!ok)return false;
 SoundFontBank* b=nullptr;for(auto* p=head_;p;p=p->next)if(p->crc==crc && !strcmp(p->path,path)){if(soundfont_resident(p)){out={};out.soundfont=p;__atomic_store_n(&p->last_use,__atomic_add_fetch(&sf_use_clock,1,__ATOMIC_RELAXED),__ATOMIC_RELAXED);return true;}b=p;break;}
 // PCM16 expands to float. Include bounded contexts and metadata scratch. The
 // allocator remains fallible for unusual region/preset layouts beyond this estimate.
 failure_="SF2 memory budget / sound kept";evict_unused(size_t(estimate));size_t used=0;for(auto* p=head_;p;p=p->next)used+=p->bytes;if(used>budget_-size_t(estimate))return false;
 bool fresh=!b;if(fresh){if(handles_>=128){failure_="SF2 handle limit / sound kept";return false;}b=alloc_new<SoundFontBank>();}if(!b)return false;
 size_t before=soundfont_resident_bytes();ok=copy_text(b->path,sizeof b->path,path);b->crc=crc;Stream input{fs,path,0,fs.size(path)};tsf_stream stream{&input,Stream::read,Stream::skip};
 if(ok){b->voices[0]=tsf_load(&stream);}ok=ok && !input.failed && b->voices[0] && tsf_get_presetcount(b->voices[0])>0;
 for(unsigned i=0;ok && i<kSfVoices;++i){if(i)b->voices[i]=tsf_copy(b->voices[0]);auto* f=b->voices[i];ok=f && tsf_set_max_voices(f,kSfRegions) && tsf_channel_set_presetindex(f,0,0);if(ok)tsf_set_output(f,TSF_STEREO_INTERLEAVED,48000,0);}
 if(ok && !b->programs){b->program_count=unsigned(b->voices[0]->presetNum);b->programs=alloc_array<SfProgram>(b->program_count);ok=b->programs!=nullptr;if(ok)for(unsigned i=0;i<b->program_count;++i){auto& dst=b->programs[i];const auto& src=b->voices[0]->presets[i];dst.bank=unsigned(src.bank);dst.program=unsigned(src.preset);__builtin_memcpy(dst.name,src.presetName,20);dst.name[20]=0;}}
 if(ok){bool verified=false;ok=checksum(fs,path,verified)==crc && verified;}
 if(!ok){if(fresh)delete b;else b->release();return false;}b->bytes=soundfont_resident_bytes()-before;
 if(b->bytes>budget_-used){b->release();if(fresh)delete b;return false;}
 if(fresh){b->next=head_;head_=b;++handles_;}__atomic_store_n(&b->last_use,__atomic_add_fetch(&sf_use_clock,1,__ATOMIC_RELAXED),__ATOMIC_RELAXED);__atomic_store_n(&b->state,2,__ATOMIC_RELEASE);out={};out.soundfont=b;failure_="";return true;
}
const char* soundfont_name(SoundFontBank* b,unsigned bank,unsigned program){if(b)for(unsigned i=0;i<b->program_count;++i)if(b->programs[i].bank==bank && b->programs[i].program==program)return b->programs[i].name;return nullptr;}
bool soundfont_first(SoundFontBank* b,unsigned& bank,unsigned& program){if(!b || !b->program_count)return false;bank=b->programs[0].bank;program=b->programs[0].program;return true;}
void SoundFontVoice::stop(){if(bank_){if(index_<kSfVoices)clear(bank_->voices[index_]);__atomic_sub_fetch(&bank_->state,2,__ATOMIC_RELEASE);}bank_=nullptr;active_=gate_=false;cursor_=64;}
void SoundFontVoice::parameters(const float* values){for(unsigned i=0;i<5;++i)values_[i]=values[i];}
bool SoundFontVoice::trigger(SoundFontBank* bank,float hz,unsigned velocity,const SoundFontState& s,bool pad_input,unsigned source_note){stop();if(!bank || index_>=kSfVoices || !(hz>0) || !pin(bank))return false;bank_=bank;auto* f=bank->voices[index_];int preset=tsf_get_presetindex(f,int(s.values[0]),int(s.values[1]));if(preset<0){stop();return false;}clear(f);parameters(s.values);float note=69+12*log2f(hz/440);key_=source_note<128?source_note:unsigned(fmaxf(0,fminf(127,roundf(note))));source_key_=key_;drum_=s.values[0]==128;if(drum_ && pad_input)key_=factory_drum_gm(key_);tsf_set_output(f,TSF_STEREO_INTERLEAVED,int(rate_),0);tsf_channel_set_presetindex(f,0,preset);tsf_channel_set_tuning(f,0,(note-float(drum_?source_key_:key_))+values_[2]);tsf_channel_note_on(f,0,int(key_),1-values_[4]+values_[4]*float(velocity)/127);active_=gate_=true;cursor_=64;return true;}
void SoundFontVoice::next(float hz,bool gate,float& l,float& r){l=r=0;if(!active_ || !bank_)return;auto* f=bank_->voices[index_];if(!gate && gate_)tsf_channel_note_off(f,0,int(key_));gate_=gate;if(cursor_>=64){if(!tsf_active_voice_count(f)){stop();return;}tsf_channel_set_tuning(f,0,(69+12*log2f(hz/440)-float(drum_?source_key_:key_))+values_[2]);tsf_render_float(f,audio_,64,0);cursor_=0;}l=audio_[2*cursor_]*values_[3];r=audio_[2*cursor_+1]*values_[3];++cursor_;}
}
