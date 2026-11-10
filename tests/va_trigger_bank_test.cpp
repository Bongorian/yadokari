#include "virtual_analog_voice.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using namespace groove;
int main(){unsigned cases=0;
 for(unsigned rate:{32000u,44100u,48000u})for(unsigned mode=0;mode<6;++mode){VirtualAnalogTriggerBank bank;bank.init(rate);VirtualAnalogVoice actual[2],reference[2];for(unsigned i=0;i<2;++i){actual[i].init(rate,&bank);reference[i].init(rate);}VirtualAnalogState state;
  state.values[18]=mode==3;state.values[19]=mode==4?.4f:0;state.values[23]=float(mode%3);if(mode==5){state.values[6]=0;state.values[7]=0;}
  for(unsigned trigger=0;trigger<4;++trigger){if(trigger==2)state.values[11]=.02f;for(unsigned i=0;i<2;++i){if(i && trigger==3)state.values[13]=.3f;actual[i].trigger(.8f,state.values,0x57313001+i);reference[i].trigger(.8f,state.values,0x57313001+i);}
   for(unsigned n=0;n<4096;++n)for(unsigned i=0;i<2;++i){if(n%1024==0){state.values[4]=2200+float((n+i)%19);actual[i].parameters(state.values);reference[i].parameters(state.values);}float hz=220+float(i)*110+float(n%11);float a=actual[i].next(hz,n<3000),b=reference[i].next(hz,n<3000);assert(!std::memcmp(&a,&b,sizeof a));}
  }
  assert(bank.envelope_updates<8);++cases;
 }
 VirtualAnalogTriggerBank other_rate;other_rate.init(48000);VirtualAnalogVoice fallback,reference;fallback.init(32000,&other_rate);reference.init(32000);VirtualAnalogState state;fallback.trigger(1,state.values,1);reference.trigger(1,state.values,1);for(unsigned n=0;n<2048;++n){float a=fallback.next(440,true),b=reference.next(440,true);assert(a==b);}
 std::printf("VA trigger bank: %u rate/mode cases, independent sounding state, changing ADSR/PWM/filter, retrigger, exact PCM and wrong-rate fallback passed\n",cases);
}
