#include "engine.h"
#include <cstdio>
#include <cstdint>
static void w16(FILE* f,uint16_t v){ fputc(v&255,f); fputc(v>>8,f); }
static void w32(FILE* f,uint32_t v){ w16(f,uint16_t(v)); w16(f,uint16_t(v>>16)); }
int main(int argc,char**argv) {
 const char* path=argc>1?argv[1]:"build/demo.wav"; FILE* f=fopen(path,"wb"); if(!f) return 1;
 constexpr uint32_t frames=48000*4; fwrite("RIFF",1,4,f); w32(f,36+frames*4); fwrite("WAVEfmt ",1,8,f); w32(f,16); w16(f,1); w16(f,2); w32(f,48000); w32(f,48000*4); w16(f,4); w16(f,16); fwrite("data",1,4,f); w32(f,frames*4);
 groove::Engine e; e.submit({groove::CommandType::SetInstrument,3,0,uint16_t(groove::Instrument::DaisySynth)}); e.submit({groove::CommandType::SetNote,3,0,45}); for(unsigned s=0;s<16;s+=4) e.submit({groove::CommandType::SetStep,3,uint8_t(s),1}); e.submit({groove::CommandType::SetInstrument,1,0,uint16_t(groove::Instrument::DaisyAnalogDrums)}); e.submit({groove::CommandType::SetInstrument,2,0,uint16_t(groove::Instrument::PolySaw)}); e.submit({groove::CommandType::SetStep,2,8,1}); for(unsigned s=0;s<16;s+=4) e.submit({groove::CommandType::SetStep,0,uint8_t(s),1}); for(unsigned s=2;s<16;s+=4) e.submit({groove::CommandType::SetStep,1,uint8_t(s),1}); e.submit({groove::CommandType::Start,0,0,0});
 float b[256]; for(unsigned i=0;i<frames/128;++i) { e.render(b,128); for(float x:b) w16(f,uint16_t(int16_t(x*32767))); }
 return fclose(f)!=0;
}
