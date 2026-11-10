#include "engine_registry.h"
#include <cassert>
int main(){using namespace groove;NativeBudget b;float values[15]{};
 values[0]=0;unsigned accepted=0;for(unsigned t=0;t<16;++t)accepted+=b.admit(t,engine_native_cost(Instrument::Texture,values));assert(accepted==9 && b.count()==9);
 values[0]=3;assert(!b.admit(0,engine_native_cost(Instrument::Texture,values)));assert(b.held(0) && b.count()==9);
 b.clear();assert(b.admit(0,engine_native_cost(Instrument::Texture,values)));assert(!b.admit(1,engine_native_cost(Instrument::Texture,values)));b.release(0);assert(b.admit(1,engine_native_cost(Instrument::Texture,values)));
 b.clear();values[0]=2;accepted=0;for(unsigned t=0;t<16;++t)accepted+=b.admit(t,engine_native_cost(Instrument::Texture,values));assert(accepted==13 && b.count()==13);
 b.clear();values[0]=1;accepted=0;for(unsigned t=0;t<16;++t)accepted+=b.admit(t,engine_native_cost(Instrument::Resonator,values));assert(accepted==9);
 assert(engine_descriptor(unsigned(Instrument::Texture))->runtime_cost.count==16);assert(engine_descriptor(unsigned(Instrument::Resonator))->runtime_cost.count==3);
 b.clear();values[0]=0;values[11]=1;accepted=0;for(unsigned t=0;t<16;++t)accepted+=b.admit(t,engine_native_cost(Instrument::Texture,values));assert(accepted==14 && b.count()==14);
 b.clear();values[0]=2;accepted=0;for(unsigned t=0;t<16;++t)accepted+=b.admit(t,engine_native_cost(Instrument::Resonator,values));assert(accepted==14 && b.count()==14);
}
