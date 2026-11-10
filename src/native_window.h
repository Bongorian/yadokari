#pragma once
#include <stdint.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
namespace groove {
// Each lane is exactly int16 -> float -> one multiply; no reassociation.
inline void native_analysis_window(float* output,const int16_t* input,const float* window,unsigned stride,unsigned count){
 unsigned i=0;
#if defined(__aarch64__)
 for(;i+4<=count;i+=4){float weights[4]={window[i*stride],window[(i+1)*stride],window[(i+2)*stride],window[(i+3)*stride]};auto samples=vcvtq_f32_s32(vmovl_s16(vld1_s16(input+i)));vst1q_f32(output+i,vmulq_f32(samples,vld1q_f32(weights)));}
#endif
 for(;i<count;++i)output[i]=window[i*stride]*input[i];
}
}
