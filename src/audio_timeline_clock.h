#pragma once
#include "alloc_policy.h"
#include "project.h"

namespace groove {
// Output-frame stamps for the current Song pass. Absolute checkpoints and
// low frame bits allow constant-time lookup. Only the producer allocates a page. It
// remains valid until Engine destruction, including across queued edits.
class AudioTimelineClock {
 // 7,168 narrow stamps plus checkpoints occupy 16,128 bytes in a 16 KiB Circle bucket.
 static constexpr unsigned Block=32,Page=7168,Pages=kSongMaxTicks/Page+1;
 struct PageBase {uint64_t frames[Page/Block];};
 template<class T> struct TickPage : PageBase {T positions[Page];};
 PageBase* pages_[Pages]{};
 unsigned prepared_=0,section_count_=0;
 uint32_t lengths_[32]{};
 // Store low frame bits. Even with a clock phase correction, 32 ticks at
 // the transport minimum of 20 BPM fit in 16 bits at supported audio rates.
 // Higher-rate hosts use a wider timestamp.
 const bool wide_;
 bool reserve(unsigned ticks){
  unsigned end=ticks>kSongMaxTicks?kSongMaxTicks:ticks?ticks-1:0;
  unsigned required=ticks?end/Page+1:0;
  if(required<=prepared_)return true;
  for(unsigned i=prepared_;i<required;++i){
   pages_[i]=wide_?static_cast<PageBase*>(alloc_new<TickPage<uint32_t>>()):alloc_new<TickPage<uint16_t>>();
   if(!pages_[i]){
    while(i>prepared_)free_page(--i);
    return false;
   }
  }
  __atomic_store_n(&prepared_,required,__ATOMIC_RELEASE);
  return true;
 }
 void free_page(unsigned i){
  if(wide_)delete static_cast<TickPage<uint32_t>*>(pages_[i]);
  else delete static_cast<TickPage<uint16_t>*>(pages_[i]);
  pages_[i]=nullptr;
 }
 // Render-owner state. first_ can be nonzero when transport started from SPP.
 uint32_t first_=0,last_=0,initial_tempo_=0;
 uint64_t first_frame_=0,last_frame_=0;
public:
 explicit AudioTimelineClock(unsigned rate):wide_(rate>500000){}
 ~AudioTimelineClock(){for(unsigned i=0;i<prepared_;++i)free_page(i);}
 bool prepare(const SongState& song){
  uint32_t total=0;
  for(unsigned i=0;i<song.length;++i)total+=song_section_ticks(song.sections[i]);
  if(!reserve(total))return false;
  section_count_=song.length;
  for(unsigned i=0;i<section_count_;++i)lengths_[i]=song_section_ticks(song.sections[i]);
  return true;
 }
 bool prepare_section(unsigned index,const SongSection& section){
  if(index>=section_count_)return true;
  // The currently sounding section can keep its old duration until its end.
  uint32_t total=0,next=song_section_ticks(section);
  if(next<=lengths_[index])return true;
  for(unsigned i=0;i<section_count_;++i)total+=i==index?next:lengths_[i];
  if(!reserve(total))return false;
  lengths_[index]=next;return true;
 }
 void reset(){initial_tempo_=0;}
 void observe(uint32_t tick,uint64_t frame,uint32_t tempo){
  if(initial_tempo_ && tick==last_)return;
  unsigned index=tick/Page;
  auto* page=index<__atomic_load_n(&prepared_,__ATOMIC_ACQUIRE)?pages_[index]:nullptr;
  bool first=!initial_tempo_ || tick!=last_+1;
  if(first){first_=tick;first_frame_=frame;initial_tempo_=tempo;}
  if(page){
   unsigned i=tick%Page;
   if(first || i%Block==0)page->frames[i/Block]=frame;
   if(wide_)static_cast<TickPage<uint32_t>*>(page)->positions[i]=uint32_t(frame);
   else static_cast<TickPage<uint16_t>*>(page)->positions[i]=uint16_t(frame);
  }
  last_=tick;last_frame_=frame;
 }
 uint64_t elapsed(uint32_t origin,unsigned rate)const{
  if(origin<=first_){
   uint64_t prefix=uint64_t(first_-origin)*rate;
   // Split the division before Q16 scaling to keep long seeks representable.
   uint64_t units=uint64_t(60)*65536,den=uint64_t(initial_tempo_)*kSongTicksPerBeat;
   return last_frame_-first_frame_+(prefix/den)*units+(prefix%den)*units/den;
  }
  const auto* page=pages_[origin/Page];
  unsigned i=origin%Page;
  uint64_t base=page->frames[i/Block];
  uint32_t delta=wide_?uint32_t(static_cast<const TickPage<uint32_t>*>(page)->positions[i]-uint32_t(base)):
                       uint16_t(static_cast<const TickPage<uint16_t>*>(page)->positions[i]-uint16_t(base));
  return last_frame_-(base+delta);
 }
};
}
