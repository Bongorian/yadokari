#include "parameter_resolver.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <cstdio>
int main(){using namespace groove;auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.level=.5f;auto& m=p->macros[7];m.active=true;m.value=.5f;auto& r=m.routes[15];r={true,1,1,-.4f,.4f,LayerMode::Add,MacroCurve::Slow};auto resolve=[&](unsigned track){return resolve_parameter(*p,t,track,1,nullptr,0).value;};assert(fabsf(resolve(0)-.3f)<.0001f);assert(fabsf(resolve(1)-.5f)<.0001f);r.mode=LayerMode::Multiply;r.low=.5f;r.high=1.5f;r.curve=MacroCurve::Fast;assert(fabsf(resolve(0)-.625f)<.0001f);r.mode=LayerMode::Replace;r.low=1;r.high=0;r.curve=MacroCurve::Smooth;assert(fabsf(resolve(0)-.5f)<.0001f);r.parameter=94;r.low=0;r.high=18;r.curve=MacroCurve::Slow;assert(resolve_parameter(*p,t,0,94,nullptr,0).value==0);r.curve=MacroCurve::Fast;assert(resolve_parameter(*p,t,0,94,nullptr,0).value==18);puts("macro additive, multiply, mask, inverse and discrete curves passed");}
