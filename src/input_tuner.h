#pragma once
#include <math.h>
namespace groove {
struct TunerReading {float hz=0,cents=0,confidence=0;int note=-1;};
// File/UI owner only. 1024 samples at 12 kHz; normalized difference, first
// convincing period, parabolic refinement. No FFT/heap and no audio-owner work.
inline TunerReading tune_input(const float* samples,float sample_rate=12000){
 TunerReading out;constexpr unsigned count=1024,limit=300;float difference[limit+2]{};float energy=0;for(unsigned i=0;i<count;++i)energy+=samples[i]*samples[i];if(energy/count<.000001f)return out;
 float sum=0;unsigned period=0;
 for(unsigned lag=1;lag<=limit;++lag){float d=0;for(unsigned i=0;i<count-limit;++i){float delta=samples[i]-samples[i+lag];d+=delta*delta;}sum+=d;difference[lag]=sum>0?d*lag/sum:1;if(lag>5 && difference[lag-1]<.15f && difference[lag]>=difference[lag-1]){period=lag-1;break;}}
 if(!period)return out;
 float a=difference[period-1],b=difference[period],c=difference[period+1],den=a-2*b+c;float offset=fabsf(den)>1e-9f?.5f*(a-c)/den:0;if(offset<-.5f)offset=-.5f;if(offset>.5f)offset=.5f;out.hz=sample_rate/(period+offset);if(out.hz<40 || out.hz>1200)return {};
 float midi=69+12*log2f(out.hz/440);out.note=int(floorf(midi+.5f));out.cents=(midi-out.note)*100;out.confidence=1-b;return out;
}
}
