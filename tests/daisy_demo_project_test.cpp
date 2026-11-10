#include "benchmark_project.h"
#include <cassert>
#include <memory>
int main(){auto p=std::make_unique<groove::ProjectState>();groove::make_daisy_drum_demo_project(*p);assert(groove::valid_project(*p));assert(p->tempo==120);for(unsigned t=0;t<6;++t){const auto track=track_at(*p,0,t);assert(track.instrument==(t<3?groove::Instrument::DaisyAnalogDrums:groove::Instrument::DaisySyntheticDrums));unsigned hits=0;for(const auto& s:track.steps)hits+=s.enabled;assert(hits>=2);}for(unsigned t=6;t<16;++t)for(const auto& step:track_at(*p,0,t).steps)assert(!step.enabled);}
