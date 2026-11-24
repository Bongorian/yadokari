#pragma once
#include <stdint.h>
namespace groove {
constexpr unsigned kWavetableWaves=128,kWavetableLevels=7,kWavetableSamples=256;
// Immutable process-lifetime decoded bank. Reserve on the control owner before
// any voice starts; trigger/render never allocate or decode.
extern const int16_t (*kWavetableData)[kWavetableLevels][kWavetableSamples];
// Converted PCM contains contiguous [frame][mip][phase] samples followed by
// signed cosine/sine pairs for harmonics 2..16. Views own no audio memory.
struct WavetableBankView {
 const int16_t* pcm=nullptr;
 unsigned frames=0;
 const int16_t* wave(unsigned f,unsigned mip)const{return pcm+(f*kWavetableLevels+mip)*kWavetableSamples;}
 const int16_t* partial(unsigned f,unsigned h)const{return wave(frames,0)+30*f+2*(h-2);}
 float position(float control)const{return control*(float(frames-1)/127.f);}
};
struct WavView;
WavetableBankView read_wavetable_asset(const WavView& sample);
const WavetableBankView* factory_wavetable(unsigned index);
bool prepare_wavetable_bank();
inline bool wavetable_bank_ready(){return __atomic_load_n(&kWavetableData,__ATOMIC_ACQUIRE)!=nullptr;}
}
