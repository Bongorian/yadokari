#include "parameter_resolver.h"
#include <cassert>
#include <cmath>
int main(){
 for(unsigned i=0;i<10000;++i){float value=float(i%101)/100,depth=i%3?0:float(i%97)/100,phase=float(i%1000)/1000,pressure=float(i%77)/100,amount=float(i%53)/100;
  float reference=groove::clamp_parameter(3,value*(1-depth*(0.5f+0.5f*sinf(phase*6.2831853f)))+pressure*amount);
  assert(std::abs(reference-groove::modulated_cutoff(value,depth,phase,pressure,amount))<0.000001f);
 }
}
