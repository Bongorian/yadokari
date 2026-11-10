#include "fm6_voice.h"
#include "dx7_bank.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace groove;
int main(){Fm6Voice voice;voice.init(48000);double total=0;unsigned audible=0;for(unsigned patch=0;patch<96;++patch){Fm6State state;state.values[0]=float(patch);voice.trigger(261.626f,110,state);double energy=0;for(unsigned i=0;i<6000;++i){float y=voice.next(261.626f,true);assert(std::isfinite(y));energy+=double(y)*y;}audible+=energy>1e-8;total+=energy;for(unsigned i=0;i<128;++i)voice.next(261.626f,false);}assert(audible>80 && total>1);printf("FM6: %u/96 factory patches emitted finite energy, %f total\n",audible,total);}
