#include "ui_probe_workload.h"
#include "audio_phase_timing.h"
#include <cassert>
#include <memory>
#include <cstdio>
using namespace groove;
int main(){static const SoundState expected{};static const LayerState layer{};assert(!__builtin_memcmp(&empty_sound(),&expected,sizeof expected) && !__builtin_memcmp(&empty_layer(),&layer,sizeof layer));auto p=std::make_unique<ProjectState>();make_ui_probe_project(*p);assert(valid_project(*p));assert(p->tracks[0].instrument==Instrument::FM && p->tracks[1].instrument==Instrument::Sampler && p->tempo==120 && p->random_seed==kUiProbeSeed && p->clips[0].playing && !p->clips[1].playing);assert(kUiProbeStages==22 && __builtin_strcmp(ui_probe_class(8),"TRACKER")==0 && __builtin_strcmp(ui_probe_class(10),"SAMPLER-WAVEFORM")==0);for(unsigned i=2;i<16;++i)assert(p->tracks[i].instrument==Instrument::Empty);
 AudioPhaseTiming t;t.worker(500,1);t.worker(501,2);t.phase(AudioPhaseTiming::Save);t.worker(1001,3);t.callback(0xfffffff0u);t.callback(0x00000200u);assert(t.worker_stats(unsigned(AudioPhaseTiming::Boot)).over500==1);assert(t.worker_stats(unsigned(AudioPhaseTiming::Save)).maximum==1001 && t.worker_stats(unsigned(AudioPhaseTiming::Save)).first_at==3);assert(t.callback_stats(unsigned(AudioPhaseTiming::Save)).maximum==528 && t.callback_stats(unsigned(AudioPhaseTiming::Save)).over500==1);t.phase(AudioPhaseTiming::Play);t.worker(499,100);assert(t.worker_stats(unsigned(AudioPhaseTiming::Play)).over500==0);puts("Optimization contract: immutable22-screen score, phase bins, 500us boundary and wrapping clocks passed");}
