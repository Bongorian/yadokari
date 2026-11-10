#pragma once
#include "wav_sampler.h"
#include "file_access.h"
namespace groove {
inline constexpr unsigned kSampleWaveBins=440;
struct SampleAnalysis {float low[kSampleWaveBins]{},high[kSampleWaveBins]{};float transients[65]{};uint8_t slices=0;};
// Detect threshold crossings at source-frame resolution, independently of the
// 440-column overview. A smoothed envelope and 40 ms spacing suppress
// repeated zero-crossing detections. A leading slice starts at frame zero.
class SampleTransientDetector {
 uint32_t next_=0,last_=0,spacing_=0,first_=0,end_=0;float envelope_=0,decay_=0,attack_=0;
public:
 void reset(uint32_t rate,uint32_t frames,float start=0,float end=1){next_=0;first_=uint32_t(double(start)*frames);end_=uint32_t(double(end)*frames);last_=first_;spacing_=rate/25;envelope_=0;decay_=expf(-1.0f/(float(rate)*0.035f));attack_=1.0f-expf(-1.0f/(float(rate)*0.002f));}
 void feed(uint32_t frame,uint32_t frames,float magnitude,SampleAnalysis& out){if(frame<next_)return;next_=frame+1;if(frame>=first_ && frame<end_ && magnitude>0.08f && magnitude>envelope_*1.8f && uint64_t(frame)>uint64_t(last_)+spacing_ && out.slices<64){out.transients[out.slices++]=float(frame)/float(frames);last_=frame;}if(magnitude>envelope_)envelope_+=(magnitude-envelope_)*attack_;else envelope_*=decay_;}
};
template<class Cancel> inline bool analyze_sample_until(WavView view,SampleAnalysis& out,float start,float end,Cancel cancelled){out=SampleAnalysis{};if(!view.data || !view.frames || !view.rate || !(start>=0 && start<end && end<=1))return false;SampleTransientDetector detector;detector.reset(view.rate,view.frames,start,end);out.transients[0]=0;out.slices=1;for(unsigned x=0;x<kSampleWaveBins;++x){uint32_t first=uint32_t(uint64_t(view.frames)*x/kSampleWaveBins),end=uint32_t(uint64_t(view.frames)*(x+1)/kSampleWaveBins);if(end<=first)end=first+1;float lo=1,hi=-1;for(uint32_t i=first;i<end && i<view.frames;++i){if((i&1023u)==0 && cancelled())return false;float magnitude=0;for(unsigned ch=0;ch<view.channels;++ch){float v=wav_sample(view,i,ch);if(v<lo)lo=v;if(v>hi)hi=v;magnitude=fmaxf(magnitude,fabsf(v));}detector.feed(i,view.frames,magnitude,out);}out.low[x]=lo;out.high[x]=hi;}out.transients[out.slices]=1;return true;}
inline void analyze_sample(WavView view,SampleAnalysis& out,float start=0,float end=1){analyze_sample_until(view,out,start,end,[]{return false;});}
// Task-side incremental full-file analysis. Each service call reads at most
// 2048 audio bytes; it never touches the DSP's single-producer page queue.
class SampleFileAnalysis {
 IFileAccess& fs_;char path_[160]{};WavView format_{};uint64_t offset_=0,size_=0;FileRevision revision_{};bool revision_available_=false;SampleAnalysis result_{};
 bool current(){if(fs_.size(path_)!=size_)return false;if(!revision_available_)return true;FileRevision now;return fs_.revision(path_,now) && same_file_revision(revision_,now);}
 unsigned bin_=0,frame_=0;float low_=1,high_=-1;SampleTransientDetector detector_{};bool busy_=false,done_=false;
public:
 explicit SampleFileAnalysis(IFileAccess& fs):fs_(fs){}
 bool start(const char* path,float start=0,float end=1){busy_=done_=false;result_=SampleAnalysis{};bin_=frame_=0;low_=1;high_=-1;if(!(start>=0 && start<end && end<=1) || !copy_text(path_,sizeof path_,path))return false;revision_available_=fs_.revision(path_,revision_);if(!sample_file_header(fs_,path_,format_,offset_))return false;size_=fs_.size(path_);if(!current())return false;detector_.reset(format_.rate,format_.frames,start,end);result_.slices=1;busy_=true;return true;}
 bool busy()const{return busy_;}bool done()const{return done_;}const SampleAnalysis& result()const{return result_;}
 void cancel(){busy_=done_=false;}
 void service(){if(!busy_)return;if(!current()){cancel();return;}uint8_t data[2048];unsigned budget=sizeof data/wav_frame_bytes(format_);
  while(budget && bin_<kSampleWaveBins){unsigned first=unsigned(uint64_t(format_.frames)*bin_/kSampleWaveBins),end=unsigned(uint64_t(format_.frames)*(bin_+1)/kSampleWaveBins);if(end<=first)end=first+1;if(end>format_.frames)end=format_.frames;if(frame_<first)frame_=first;unsigned count=end-frame_;if(count>budget)count=budget;if(count){unsigned bytes=count*wav_frame_bytes(format_),got=0;if(!fs_.read(path_,offset_+uint64_t(frame_)*wav_frame_bytes(format_),data,bytes,got) || got!=bytes){cancel();return;}auto v=format_;v.data=data;v.frames=count;for(unsigned n=0;n<count;++n){float magnitude=0;for(unsigned ch=0;ch<v.channels;++ch){float s=wav_sample(v,n,ch);low_=fminf(low_,s);high_=fmaxf(high_,s);magnitude=fmaxf(magnitude,fabsf(s));}detector_.feed(frame_+n,format_.frames,magnitude,result_);}frame_+=count;budget-=count;}
   if(frame_>=end){result_.low[bin_]=low_;result_.high[bin_]=high_;++bin_;frame_=unsigned(uint64_t(format_.frames)*bin_/kSampleWaveBins);low_=1;high_=-1;}}
  if(!current()){cancel();return;}if(bin_==kSampleWaveBins){result_.transients[result_.slices]=1;busy_=false;done_=true;}}
};
// Analysis positions are relative to the file; voice slice markers are
// relative to the current trim region. Keep only transients inside it.
inline void apply_transients(SampleEdit& edit,const SampleAnalysis& a){unsigned count=1;edit.markers[0]=0;float length=edit.end-edit.start;for(unsigned i=1;i<a.slices && count<64;++i){float p=a.transients[i];if(p<=edit.start || p>=edit.end)continue;float marker=(p-edit.start)/length;if(marker>edit.markers[count-1] && marker<1)edit.markers[count++]=marker;}edit.slice_count=uint8_t(count);edit.markers[count]=1;for(unsigned i=0;i<16;++i)edit.pads[i]=uint8_t(i%count);}
}
