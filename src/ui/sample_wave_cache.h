#pragma once
#include "../browser_state.h"
namespace groove { namespace ui {
// One immutable completed overview per displayed source. A pending reanalysis
// never blanks that overview. Identity includes project generation and format;
// a different sample cannot borrow the preceding sample's peaks.
class SampleWaveCache {
 const void* source_=nullptr;uint32_t generation_=0,frames_=0;unsigned channels_=0,bits_=0,format_=0;
 SampleAnalysis shown_{},work_{};bool ready_=false,pending_=false;unsigned bin_=0,frame_=0;float low_=1,high_=-1;
public:
 bool ready()const{return ready_;}bool pending()const{return pending_;}
 const SampleAnalysis* peaks()const{return ready_?&shown_:nullptr;}
 bool update(WavView view,const BrowserState& browser,uint32_t generation){
  const void* source=view.reader?static_cast<const void*>(view.reader):view.data;
  bool changed=source!=source_ || generation!=generation_ || view.frames!=frames_ || view.channels!=channels_ || view.bits!=bits_ || view.format!=format_;
  if(changed){source_=source;generation_=generation;frames_=view.frames;channels_=view.channels;bits_=view.bits;format_=view.format;ready_=false;pending_=source && view.data && !view.reader && !browser.analysis_background;bin_=frame_=0;low_=1;high_=-1;}
  if(!source || !view.frames){ready_=pending_=false;return changed;}
  if(browser.analysis_source==source){bool fresh=!ready_ || __builtin_memcmp(shown_.low,browser.analysis.low,sizeof shown_.low) || __builtin_memcmp(shown_.high,browser.analysis.high,sizeof shown_.high);if(fresh)shown_=browser.analysis;ready_=true;pending_=false;return changed || fresh;}
  // Development/no-worker fallback: same exact min/max bins, bounded resident
  // memory reads per UI tick. Disk I/O stays exclusively in SessionFiles.
  unsigned budget=4096;
  while(pending_ && budget && bin_<kSampleWaveBins){unsigned first=unsigned(uint64_t(frames_)*bin_/kSampleWaveBins),end=unsigned(uint64_t(frames_)*(bin_+1)/kSampleWaveBins);if(end<=first)end=first+1;if(end>frames_)end=frames_;if(frame_<first)frame_=first;
   while(budget && frame_<end){for(unsigned ch=0;ch<view.channels;++ch){float value=wav_sample(view,frame_,ch);low_=fminf(low_,value);high_=fmaxf(high_,value);}++frame_;--budget;}
   if(frame_==end){work_.low[bin_]=low_;work_.high[bin_]=high_;++bin_;frame_=unsigned(uint64_t(frames_)*bin_/kSampleWaveBins);low_=1;high_=-1;}
  }
  if(pending_ && bin_==kSampleWaveBins){shown_=work_;ready_=true;pending_=false;changed=true;}
  return changed;
 }
};
}}
