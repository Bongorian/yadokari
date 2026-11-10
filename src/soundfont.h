#pragma once
#include "soundfont_state.h"
#include "file_access.h"
namespace groove {
class IFileAccess;struct WavView;struct SoundFontBank;
unsigned soundfont_allocation_count();
size_t soundfont_resident_bytes();
uint32_t soundfont_crc(SoundFontBank*);
bool soundfont_resident(SoundFontBank*);
inline bool soundfont_file_header(IFileAccess& fs,const char* path){uint8_t h[12];unsigned got=0;return fs.read(path,0,h,12,got) && got==12 && wav_tag(h,"RIFF") && wav_tag(h+8,"sfbk") && uint64_t(wav_u32(h+4))+8==fs.size(path);}
class SoundFontCache {
 SoundFontBank* head_=nullptr;size_t budget_=512u*1024u*1024u;unsigned handles_=0;const char* failure_="";
public:
 explicit SoundFontCache(size_t budget=512u*1024u*1024u):budget_(budget){}
 const char* failure()const{return failure_;}
 unsigned evict_unused(size_t required=0);
 ~SoundFontCache();bool load(IFileAccess& fs,const char* path,WavView& out);WavView lookup(const char* path)const;
};
const char* soundfont_name(SoundFontBank*,unsigned bank,unsigned program);
bool soundfont_first(SoundFontBank*,unsigned& bank,unsigned& program);
class SoundFontVoice {
 SoundFontBank* bank_=nullptr;unsigned index_=0,rate_=48000,cursor_=64,key_=60,source_key_=60;bool gate_=false,active_=false,drum_=false;float values_[5]{},audio_[128]{};
public:
 ~SoundFontVoice(){stop();}
 SoundFontVoice()=default;SoundFontVoice(const SoundFontVoice&)=delete;SoundFontVoice& operator=(const SoundFontVoice&)=delete;
 void init(unsigned index,unsigned rate){index_=index;rate_=rate;}
 void stop();bool trigger(SoundFontBank*,float hz,unsigned velocity,const SoundFontState&,bool pad_input=true,unsigned source_note=128);void parameters(const float* values);void next(float hz,bool gate,float& l,float& r);bool active()const{return active_;}unsigned key()const{return key_;}
};
}
