#include "plaits_voice.h"
#include <cstdio>
#include <cstdint>
static void w16(FILE* f,uint16_t v) { fputc(v&255,f); fputc(v>>8,f); }
static void w32(FILE* f,uint32_t v) { w16(f,uint16_t(v)); w16(f,uint16_t(v>>16)); }
int main(int argc,char** argv) {
 const char* path=argc>1?argv[1]:"build/plaits-models.wav"; FILE* f=fopen(path,"wb"); if(!f) return 1;
 constexpr uint32_t segment=24000, frames=segment*24;
 fwrite("RIFF",1,4,f); w32(f,36+frames*4); fwrite("WAVEfmt ",1,8,f); w32(f,16); w16(f,1); w16(f,2); w32(f,48000); w32(f,192000); w16(f,4); w16(f,16); fwrite("data",1,4,f); w32(f,frames*4);
 for(unsigned model=0;model<24;++model) { groove::PlaitsVoice voice; voice.init(); voice.set_model(model); voice.trigger(); for(unsigned i=0;i<segment;++i) { int16_t s=int16_t(voice.next()*32767); w16(f,uint16_t(s)); w16(f,uint16_t(s)); } }
 return fclose(f)!=0;
}
