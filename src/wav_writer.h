#pragma once
#include <stdint.h>
#include <stddef.h>
#include "pcm_output.h"
namespace groove {
// Header construction is shared by finite captures and task-owned streamed recording.
inline bool wav_header(uint8_t* header,uint32_t frames,uint32_t rate=48000){
 if(!header || !rate || frames>(0xffffffffu-36)/4)return false;
 const uint8_t initial[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0};
 for(unsigned i=0;i<44;++i){header[i]=initial[i];}
 const uint32_t values[]={36+frames*4,rate,rate*4,frames*4};const unsigned offsets[]={4,24,28,40};for(unsigned j=0;j<4;++j)for(unsigned b=0;b<4;++b)header[offsets[j]+b]=uint8_t(values[j]>>(b*8));return true;
}
// Channel-aware variant (Stage 3 stretch output keeps the source
// channel count); wav_header above stays stereo-only for captures.
inline bool wav_header_ch(uint8_t* header,uint32_t frames,uint32_t rate,uint16_t channels){
 if(!header || !rate || (channels!=1 && channels!=2))return false;
 const uint32_t frame_bytes=uint32_t(channels)*2;
 if(frames>(0xffffffffu-36)/frame_bytes)return false;
 const uint8_t initial[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0};
 for(unsigned i=0;i<44;++i){header[i]=initial[i];}
 header[22]=uint8_t(channels);header[23]=0;
 const uint32_t values[]={36+frames*frame_bytes,rate,rate*frame_bytes,frames*frame_bytes};
 const unsigned offsets[]={4,24,28,40};
 for(unsigned j=0;j<4;++j)for(unsigned b=0;b<4;++b)header[offsets[j]+b]=uint8_t(values[j]>>(b*8));
 header[32]=uint8_t(frame_bytes);header[33]=0;return true;
}
// Existing IEEE-float WAV playback accepts format 3; finite autosampling keeps
// full precision until the normal sampler decoder. No limiter is added here.
inline bool wav_header_float32(uint8_t* header,uint32_t frames,uint32_t rate){
 if(frames>(0xffffffffu-36)/8 || !wav_header_ch(header,frames,rate,2))return false;
 header[20]=3;header[32]=8;header[34]=32;
 const uint32_t values[]={36+frames*8,rate*8,frames*8};const unsigned offsets[]={4,28,40};for(unsigned j=0;j<3;++j)for(unsigned b=0;b<4;++b)header[offsets[j]+b]=uint8_t(values[j]>>(b*8));return true;
}
// Sink must expose write(bytes, count); all file I/O remains caller-owned.
template<class Sink> bool write_wav(Sink& sink,const float* stereo,uint32_t frames,uint32_t rate=48000) {
 if(!stereo){return false;}uint8_t header[44];if(!wav_header(header,frames,rate) || !sink.write(header,44))return false;
 uint8_t buffer[1024];uint32_t cursor=0;
 while(cursor<frames) { unsigned count=frames-cursor<256?frames-cursor:256;for(unsigned i=0;i<count*2;++i){uint16_t sample=uint16_t(pcm16(stereo[cursor*2+i]));buffer[i*2]=uint8_t(sample);buffer[i*2+1]=uint8_t(sample>>8);}if(!sink.write(buffer,count*4))return false;cursor+=count; }return true;
}
}
