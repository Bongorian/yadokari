#pragma once
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "sample_state.h"
#include "alloc_policy.h"
#include "resampler_table.h"
namespace groove {
inline uint16_t wav_u16(const uint8_t* p){return uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8);}
inline uint32_t wav_u32(const uint8_t* p){return uint32_t(wav_u16(p))|uint32_t(wav_u16(p+2))<<16;}
inline bool wav_tag(const uint8_t* p,const char* id){return p[0]==id[0] && p[1]==id[1] && p[2]==id[2] && p[3]==id[3];}
class ISampleReader {public:virtual ~ISampleReader(){}virtual bool read_frame(uint32_t frame,unsigned channel,float& result)=0;virtual uint32_t underruns()const=0;
 virtual bool read_stereo(uint32_t frame,float& left,float& right){bool l=read_frame(frame,0,left),r=read_frame(frame,1,right);return l && r;}
 // Block read (O-04c): fill up to count consecutive frames of one
 // channel starting at frame, returning how many were read. The
 // sampler uses this for the resampler tap window when the wrapped
 // indices are contiguous. Paged/streaming readers should override
 // it to serve the window with a single page lookup instead of
 // one virtual call per tap.
 virtual unsigned read_block(uint32_t frame,unsigned channel,float* out,unsigned count){unsigned n=0;while(n<count && read_frame(frame+n,channel,out[n]))++n;return n;}};
struct SoundFontBank;
struct WavView {const uint8_t* data=nullptr;uint32_t frames=0,rate=0;uint16_t channels=0,bits=16,format=1;ISampleReader* reader=nullptr;SoundFontBank* soundfont=nullptr;};
inline unsigned wav_frame_bytes(WavView v){return v.channels*(v.bits/8);}
inline float wav_sample(WavView v,uint32_t frame,unsigned channel){if(frame>=v.frames || channel>=v.channels)return 0;if(v.reader){float out=0;return v.reader->read_frame(frame,channel,out)?out:0;}if(!v.data)return 0;const uint8_t* p=v.data+(size_t(frame)*v.channels+channel)*(v.bits/8);float out=0;if(v.format==3){uint32_t bits=wav_u32(p);__builtin_memcpy(&out,&bits,4);if(!isfinite(out))return 0;return out>1?1:out< -1?-1:out;}if(v.bits==8)return (float(p[0])-128.0f)/128.0f;if(v.bits==16)return float(int16_t(wav_u16(p)))/32768;if(v.bits==32)return float(int32_t(wav_u32(p)))/2147483648.0f;int32_t n=int32_t(uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16);if(n&0x800000)n|=int32_t(0xff000000);return float(n)/8388608;}
// Header parsing also supports task-level stream preparation without pretending
// that a partial file is a resident buffer. Audio never calls file functions.
inline bool parse_wav_header(const uint8_t* p,size_t bytes,uint64_t file_bytes,WavView& out,uint64_t& data_offset){
 if(!p || bytes<12 || !wav_tag(p,"RIFF") || !wav_tag(p+8,"WAVE"))return false;
 uint64_t total=uint64_t(wav_u32(p+4))+8;if(total<12 || total>file_bytes)return false;
 uint16_t format=0,channels=0,align=0,bits=0;uint32_t rate=0;
 for(uint64_t i=12;i+8<=total;){if(i+8>bytes)return false;uint32_t n=wav_u32(p+i+4);uint64_t end=i+8+n;if(end>total || (end+(n&1)>total && !wav_tag(p+i,"data")))return false;
  if(wav_tag(p+i,"fmt ")){if(n<16 || i+8+n>bytes)return false;format=wav_u16(p+i+8);channels=wav_u16(p+i+10);rate=wav_u32(p+i+12);align=wav_u16(p+i+20);bits=wav_u16(p+i+22);if(format==0xfffe){if(n<40 || wav_u16(p+i+24)<22)return false;format=wav_u16(p+i+32);const uint8_t guid_tail[]={0,0,0,0,16,0,128,0,0,170,0,56,155,113};for(unsigned k=0;k<14;++k)if(p[i+34+k]!=guid_tail[k])return false;}}
  if(wav_tag(p+i,"data")){if(rate<1000 || rate>768000 || (channels!=1 && channels!=2) || !((format==1 && (bits==8 || bits==16 || bits==24 || bits==32)) || (format==3 && bits==32)) || align!=channels*(bits/8) || !n || n>268435456u || n%align)return false;data_offset=i+8;out={nullptr,n/align,rate,channels,bits,format,nullptr};return true;}i=end+(n&1);
 }return false;
}
inline bool parse_wav(const uint8_t* p,size_t bytes,WavView& out){uint64_t offset;if(!parse_wav_header(p,bytes,bytes,out,offset) || offset+uint64_t(out.frames)*wav_frame_bytes(out)>bytes)return false;out.data=p+offset;return true;}
class WavVoice {
public:
 // Voice construction belongs to the control/initialization owner. Reserve
 // every stretch buffer here, including voices that have not sounded yet.
 WavVoice(){stretch_ensure();}
 ~WavVoice(){delete[] sbuf_;delete[] snorm_;}
private:
 WavView view_{};double position_=0,increment_=0,target_increment_=0;uint32_t first_=0,last_=0,fade_=0,crossfade_=0;float envelope_=0,attack_=1,release_=1,fade_ms_=0,crossfade_ms_=0;uint32_t output_rate_=0;bool loop_=false,pingpong_=false,active_=false,reverse_=false,releasing_=false;float edit_speed_=1;
 mutable double filter_increment_=0;mutable unsigned filter_band_=0;mutable float filter_blend_=1;mutable bool filter_valid_=false;
 void update_edge_lengths(double ratio){uint32_t half=(last_-first_)/2;double frames_per_ms=ratio*output_rate_/1000.0;fade_=uint32_t(fade_ms_*frames_per_ms+0.5);crossfade_=uint32_t(crossfade_ms_*frames_per_ms+0.5);if(fade_>half)fade_=half;if(crossfade_>half)crossfade_=half;}
 // O-04: the band selection, kernel blend and wrapped tap indices
 // depend only on the position and the increment, never on the
 // channel, so they are computed once per position and shared by
 // both channels (and by the crossfade read in next()).
 void filtered_setup(uint32_t frame,float phase,float coeffs[resampler::kTaps],uint32_t indices[resampler::kTaps])const{
  // Band and blend depend only on the exact increment, not frame/phase.
  // Keep the original arithmetic on a miss; pitch smoothing invalidates the
  // cache on every actual change, with no tolerance or altered filter quality.
  if(!filter_valid_ || filter_increment_!=increment_){unsigned band=0;while(band+1<resampler::kBands && increment_>resampler::kRatio[band])++band;float blend=band?float((increment_-resampler::kRatio[band-1])/(resampler::kRatio[band]-resampler::kRatio[band-1])):1;if(blend>1)blend=1;filter_increment_=increment_;filter_band_=band;filter_blend_=blend;filter_valid_=true;}
  unsigned band=filter_band_;float blend=filter_blend_;
  unsigned part=unsigned(phase*resampler::kPhases);if(part>=resampler::kPhases)part=resampler::kPhases-1;
  const float* weights=resampler::kKernel[band][part];const float* lower=resampler::kKernel[band?band-1:0][part];
  int64_t first=first_,last=last_,span=last-first;
  for(unsigned tap=0;tap<resampler::kTaps;++tap){
   int64_t index=int64_t(frame)+int(tap)-5;
   if(index<first || index>=last){if(pingpong_ && span>1){int64_t period=2*(span-1),wphase=(index-first)%period;if(wphase<0)wphase+=period;index=first+(wphase<span?wphase:period-wphase);}else if(loop_){index=first+(index-first)%span;if(index<first)index+=span;}else index=index<first?first:last-1;}
   indices[tap]=uint32_t(index);coeffs[tap]=lower[tap]+(weights[tap]-lower[tap])*blend;
  }
 }
 float filtered_taps(const float coeffs[resampler::kTaps],const uint32_t indices[resampler::kTaps],unsigned channel)const{
  if(view_.reader){
   bool contiguous=true;for(unsigned tap=1;tap<resampler::kTaps;++tap)if(indices[tap]!=indices[0]+tap){contiguous=false;break;}
   if(contiguous){float window[resampler::kTaps];unsigned got=view_.reader->read_block(indices[0],channel,window,resampler::kTaps);float result=0;for(unsigned tap=0;tap<resampler::kTaps;++tap)result+=(tap<got?window[tap]:0.0f)*coeffs[tap];return result;}
  }
  float result=0;
  if(view_.data && !view_.reader && view_.format==1 && view_.bits==16){
   for(unsigned tap=0;tap<resampler::kTaps;++tap){const uint8_t* p=view_.data+(size_t(indices[tap])*view_.channels+channel)*2;result+=float(int16_t(wav_u16(p)))/32768*coeffs[tap];}
   return result;
  }
  for(unsigned tap=0;tap<resampler::kTaps;++tap)result+=wav_sample(view_,indices[tap],channel)*coeffs[tap];
  return result;
 }
 float filtered_tail(uint32_t frame,float phase,unsigned channel,float result)const{
  if(increment_<1.05){uint32_t next=frame+1<last_?frame+1:pingpong_ && last_-first_>1?last_-2:loop_?first_:frame;float a=wav_sample(view_,frame,channel),b=wav_sample(view_,next,channel);float linear=a+(b-a)*phase;result=linear+(result-linear)*float((increment_-1.0)*20.0);}
  return result;
 }
 __attribute__((noinline)) float at_filtered(uint32_t frame,float phase,unsigned channel)const{float coeffs[resampler::kTaps];uint32_t indices[resampler::kTaps];filtered_setup(frame,phase,coeffs,indices);return filtered_tail(frame,phase,channel,filtered_taps(coeffs,indices,channel));}
 __attribute__((noinline)) void at_filtered2(uint32_t frame,float phase,float& out0,float& out1)const{float coeffs[resampler::kTaps];uint32_t indices[resampler::kTaps];filtered_setup(frame,phase,coeffs,indices);out0=filtered_tail(frame,phase,0,filtered_taps(coeffs,indices,0));out1=filtered_tail(frame,phase,1,filtered_taps(coeffs,indices,1));}
 __attribute__((always_inline)) inline float linear_at(uint32_t frame,float phase,unsigned channel)const{uint32_t next=frame+1<last_?frame+1:pingpong_ && last_-first_>1?last_-2:loop_?first_:frame;float a,b;if(view_.data && !view_.reader && view_.format==1 && view_.bits==16){size_t stride=size_t(view_.channels)*2,offset=size_t(channel)*2;const uint8_t* p=view_.data+size_t(frame)*stride+offset;const uint8_t* q=view_.data+size_t(next)*stride+offset;a=float(int16_t(wav_u16(p)))/32768;b=float(int16_t(wav_u16(q)))/32768;}else{a=wav_sample(view_,frame,channel);b=wav_sample(view_,next,channel);}return a+(b-a)*phase;}
 __attribute__((always_inline)) inline float at(double p,unsigned channel)const{if(p<first_)p=first_;if(p>=last_)p=last_-1;uint32_t frame=uint32_t(p);float phase=float(p-frame);return increment_<=1.0?linear_at(frame,phase,channel):at_filtered(frame,phase,channel);}
 __attribute__((always_inline)) inline void at2(double p,float& v0,float& v1)const{if(p<first_)p=first_;if(p>=last_)p=last_-1;uint32_t frame=uint32_t(p);float phase=float(p-frame);if(view_.reader && increment_==1 && phase==0){if(!view_.reader->read_stereo(frame,v0,v1))v0=v1=0;return;}if(increment_<=1.0){v0=linear_at(frame,phase,0);v1=linear_at(frame,phase,1);return;}at_filtered2(frame,phase,v0,v1);}

 // Stage 3 second stage: Stretch playback mode. The trimmed region is
 // played pitch-preserved and tempo-locked: a WSOLA grain engine (the
 // same algorithm as the offline TimeStretch, with a narrower search
 // for the audio thread) runs in source-frame space, and the read side
 // performs plain sample-rate conversion on the grain ring, so the
 // output pitch never depends on the project tempo or the note.
 // stretch_ratio_ is the duration multiplier (output/input) derived by
 // the engine from the asset BPM metadata and the project tempo; the
 // edit speed acts as in the other modes (2 = twice as fast).
 static constexpr uint32_t kGrainWindow=2048,kGrainHop=512,kGrainRing=8192;
 float* sbuf_=nullptr;float* snorm_=nullptr;
 double s_analysis_=0,s_read_=0;uint64_t s_write_=0,s_read_abs_=0;float stretch_ratio_=1;
 bool stretch_=false;
 void stretch_ensure(){if(sbuf_)return;sbuf_=alloc_array<float>(size_t(kGrainRing)*2);snorm_=alloc_array<float>(kGrainRing);if(!sbuf_ || !snorm_){delete[] sbuf_;delete[] snorm_;sbuf_=nullptr;snorm_=nullptr;}}
 uint32_t stretch_source(uint32_t rel)const{uint32_t span=last_-first_;return first_+(span?rel%span:0);}
 void stretch_grain(){
  uint32_t span=last_-first_;if(!span)return;
  unsigned channels=view_.channels;uint32_t overlap=kGrainWindow-kGrainHop;
  int64_t nominal=int64_t(s_analysis_+0.5);int best=0;
  if(s_write_>0){
   double best_score=-1e30;
   for(int d=-192;d<=192;d+=2){
    int64_t cand=nominal+d;double score=0,ref=0,candp=0;
    for(uint32_t i=0;i<overlap;i+=4){
     double r=0,c=0;
     for(unsigned ch=0;ch<channels;++ch){r+=sbuf_[size_t((s_write_+i)%kGrainRing)*2+ch];int64_t s=cand+int64_t(i);if(s<0)s=0;c+=wav_sample(view_,stretch_source(uint32_t(s)),ch);}
     score+=r*c;ref+=r*r;candp+=c*c;
    }
    double denom=sqrt(ref*candp);double value=denom>1e-12?score/denom:0;
    if(value>best_score){best_score=value;best=d;}
   }
  }
  int64_t start=nominal+best;
  for(uint32_t i=0;i<kGrainWindow;++i){
   int64_t s=start+int64_t(i);if(s<0)s=0;
   float win=float(0.5-0.5*cos(6.283185307179586*double(i)/double(kGrainWindow-1)));
   size_t ring=size_t((s_write_+i)%kGrainRing);
   for(unsigned ch=0;ch<channels;++ch)sbuf_[ring*2+ch]+=wav_sample(view_,stretch_source(uint32_t(s)),ch)*win;
   snorm_[ring]+=win;
  }
  s_write_+=kGrainHop;
  double advance=double(kGrainHop)*double(edit_speed_)/double(stretch_ratio_);
  s_analysis_+=advance;double span_d=double(span);if(s_analysis_>=span_d)s_analysis_=fmod(s_analysis_,span_d);
 }
 float stretch_at(double ring_pos,unsigned channel)const{
  if(ring_pos<0)ring_pos=0;
  uint64_t base=uint64_t(ring_pos);float phase=float(ring_pos-double(base));
  auto one=[&](uint64_t idx)->float{size_t r=size_t(idx%kGrainRing);float n=snorm_[r];float v=n>1e-6f?sbuf_[r*2+channel]/n:0;return v;};
  float a=one(base),b=one(base+1);return a+(b-a)*phase;
 }
public:
 bool start(WavView sample,uint32_t output_rate,float pitch,bool loop,bool reverse=false){SampleEdit edit;edit.fade_ms=edit.crossfade_ms=edit.attack_ms=edit.release_ms=0;return start_edit(sample,output_rate,pitch,loop,reverse,edit);}
 bool start_edit(WavView sample,uint32_t output_rate,float pitch,bool loop,bool reverse,const SampleEdit& edit,float stretch_ratio=1){active_=false;stretch_=false;edit_speed_=edit.speed;if((!sample.data && !sample.reader) || !sample.frames || !sample.rate || (sample.channels!=1 && sample.channels!=2) || !output_rate || !(pitch>=0.015625f && pitch<=64) || !valid_sample_edit(edit))return false;view_=sample;first_=uint32_t(edit.start*sample.frames);last_=uint32_t(edit.end*sample.frames);if(last_<=first_)last_=first_+1;if(last_>sample.frames)last_=sample.frames;increment_=double(sample.rate)*pitch*edit.speed/output_rate;if(!(increment_>0 && increment_<=1024))return false;target_increment_=increment_;reverse_=reverse;position_=reverse?last_-1:first_;pingpong_=edit.mode==SampleMode::PingPong;loop_=loop || edit.mode==SampleMode::Loop || pingpong_;fade_ms_=edit.fade_ms;crossfade_ms_=pingpong_?0:edit.crossfade_ms;output_rate_=output_rate;update_edge_lengths(increment_);envelope_=edit.attack_ms>0?0:1;attack_=edit.attack_ms>0?1000/(edit.attack_ms*output_rate):1;release_=edit.release_ms>0?1000/(edit.release_ms*output_rate):1;releasing_=false;if(edit.mode==SampleMode::Stretch && stretch_ratio>=0.5f && stretch_ratio<=2.0f){if(!sbuf_ || !snorm_)return false;{stretch_=true;stretch_ratio_=stretch_ratio;loop_=true;pingpong_=false;reverse_=false;for(unsigned i=0;i<kGrainRing*2;++i)sbuf_[i]=0;for(unsigned i=0;i<kGrainRing;++i)snorm_[i]=0;s_analysis_=0;s_read_=0;s_write_=0;s_read_abs_=0;stretch_grain();stretch_grain();}}active_=true;return true;}
 bool start_region(WavView sample,uint32_t rate,uint32_t first,uint32_t frames,bool loop){
  if(!frames || first>=sample.frames || frames>sample.frames-first || !start(sample,rate,1,loop))return false;
  first_=first;last_=first+frames;position_=first;return true;
 }
 void stop(){active_=false;}void release(){releasing_=true;}bool active()const{return active_;}
 void speed(float ratio){if(ratio>=0.015625f && ratio<=1024){target_increment_=ratio;update_edge_lengths(ratio);}}
 float normalized_position()const{return view_.frames?float(position_/view_.frames):0;}
 __attribute__((always_inline)) inline void next(float& left,float& right){left=right=0;if(!active_)return;float gain=1;if(!loop_ && fade_){double distance=position_-first_,end=last_-1-position_;gain=float((distance<end?distance:end)/fade_);if(gain>1)gain=1;if(gain<0)gain=0;}float values[2]{};
  if(stretch_){while(s_write_<=uint64_t(s_read_)+(kGrainWindow-kGrainHop))stretch_grain();values[0]=stretch_at(s_read_,0);values[1]=view_.channels>=2?stretch_at(s_read_,1):values[0];uint64_t consumed=uint64_t(s_read_);s_read_+=double(view_.rate)/double(output_rate_);for(uint64_t i=consumed;i<uint64_t(s_read_);++i){unsigned slot=unsigned(i%kGrainRing);sbuf_[slot*2]=sbuf_[slot*2+1]=0;snorm_[slot]=0;}s_read_abs_++;if(releasing_){envelope_-=release_;if(envelope_<=0){envelope_=0;active_=false;}}else{envelope_+=attack_;if(envelope_>1)envelope_=1;}left=values[0]*envelope_;right=values[1]*envelope_;return;}
  if(view_.channels>=2){at2(position_,values[0],values[1]);if(loop_ && crossfade_){double distance=reverse_?position_-first_+1:last_-position_;if(distance<crossfade_){float blend=float(1-distance/crossfade_);double next=reverse_?last_-crossfade_+distance:first_+crossfade_-distance;float n0,n1;at2(next,n0,n1);values[0]=values[0]*(1-blend)+n0*blend;values[1]=values[1]*(1-blend)+n1*blend;}}}else if(view_.channels==1){values[0]=at(position_,0);if(loop_ && crossfade_){double distance=reverse_?position_-first_+1:last_-position_;if(distance<crossfade_){float blend=float(1-distance/crossfade_);double next=reverse_?last_-crossfade_+distance:first_+crossfade_-distance;values[0]=values[0]*(1-blend)+at(next,0)*blend;}}values[1]=values[0];}if(releasing_){envelope_-=release_;if(envelope_<=0){envelope_=0;active_=false;}}else{envelope_+=attack_;if(envelope_>1)envelope_=1;}left=values[0]*gain*envelope_;right=values[1]*gain*envelope_;increment_+=(target_increment_-increment_)*0.005;position_+=reverse_?-increment_:increment_;double length=last_-first_;if(pingpong_){double span=double(last_-first_-1);if(span<1){position_=first_;}else if(reverse_ && position_<first_){double phase=fmod(first_-position_,2*span);if(phase<=span){position_=first_+phase;reverse_=false;}else position_=first_+2*span-phase;}else if(!reverse_ && position_>last_-1){double phase=fmod(position_-(last_-1),2*span);if(phase<=span){position_=last_-1-phase;reverse_=true;}else position_=first_+phase-span;}}else if(position_<double(first_)-(reverse_?1.0:0.0)+1e-8 || (!reverse_ && position_>=double(last_)-1e-8)){if(loop_){double span=length-crossfade_;if(reverse_){double distance=first_-position_-1;double p=fmod(distance<0?0:distance,span);position_=last_-crossfade_-1-(p<0?p+span:p);}else{double distance=position_-last_;double p=fmod(distance<0?0:distance,span);position_=first_+crossfade_+(p<0?p+span:p);}}else active_=false;}}
};
}
