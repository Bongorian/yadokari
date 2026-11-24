#pragma once
#include "wavetable_voice.h"

namespace groove {
// The primary oscillator stays in its original voice and can still use
// the mono render-ahead worker. Satellites are owned by the audio thread.
struct WavetableTap {float pitch,left,right;};
inline WavetableTap wavetable_unison_tap(unsigned count,unsigned i,float cents,float spread,float gain){
 const float side=count==1?0:float(int(2*i+1)-int(count))/float(count-1);
 const float ratio=exp2f(fabsf(side)*cents/1200.f);
 return {side<0?1.f/ratio:ratio,gain*sqrtf(1.f-side*spread),gain*sqrtf(1.f+side*spread)};
}
class WavetableUnison {
 WavetableVoice satellites_[6];
 float pitch_[7]{1},left_[7]{1},right_[7]{1};
 unsigned count_=1;
 float detune_=-1,spread_=-1;
public:
 void reset(){count_=1;detune_=spread_=-1;}
 unsigned count()const{return count_;}
 void parameters(WavetableVoice& primary,const float* values,unsigned count){
  primary.parameters(values);
  const float detune=values[WavetableDetune],spread=values[WavetableSpread];
  if(count!=count_ || detune!=detune_ || spread!=spread_){
   const float scale=1.f/sqrtf(float(count));
   for(unsigned i=0;i<count;++i){
    const auto tap=wavetable_unison_tap(count,i,detune,spread,scale);
    pitch_[i]=tap.pitch;left_[i]=tap.left;right_[i]=tap.right;
    // Start within a quarter cycle: equally spaced full-cycle copies can
    // cancel the fundamental completely at zero cents and zero spread.
    if(i>=count_){satellites_[i-1]=primary;satellites_[i-1].offset_phase(.25f*float(i)/count);}
   }
   count_=count;detune_=detune;spread_=spread;
  }
  for(unsigned i=1;i<count_;++i)satellites_[i-1].parameters(values);
 }
 void next(WavetableVoice& primary,float hz,bool gate,float& left,float& right){
  if(count_==1){left=right=primary.next(hz,gate);return;}
  float x=primary.next(hz*pitch_[0],gate);left=x*left_[0];right=x*right_[0];
  for(unsigned i=1;i<count_;++i){
   x=satellites_[i-1].next(hz*pitch_[i],gate);left+=x*left_[i];right+=x*right_[i];
  }
 }
 float primary_frequency(float hz)const{return hz*pitch_[0];}
};
}
