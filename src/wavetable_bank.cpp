#include "wavetable_bank.h"
#include "wavetable_spectrum.h"
#include "alloc_policy.h"
#include "wav_sampler.h"
#include "project_codec.h"
namespace groove {
#include "generated/akwf_tables.inc"
const int16_t (*kWavetableData)[kWavetableLevels][kWavetableSamples]=nullptr;
namespace {
constexpr int16_t reed[]={
#include "generated/wavetable_reed.inc"
};
constexpr int16_t prism[]={
#include "generated/wavetable_prism.inc"
};
constexpr WavetableBankView factories[]={{reed+4,unsigned(reed[2])},{prism+4,unsigned(prism[2])}};
struct BankStorage {unsigned char* bytes=nullptr;unsigned state=0;~BankStorage(){delete[] bytes;}} storage;
}
namespace {
constexpr int kWarpHarmonics=4,kPmSidebands=3,kWarpDepthSteps=32;
struct WavetableQuadrature {
 float re,im;
 WavetableQuadrature mix(WavetableQuadrature b,float x)const{return {re+x*(b.re-re),im+x*(b.im-im)};}
 WavetableQuadrature times(WavetableQuadrature b)const{return {re*b.re-im*b.im,re*b.im+im*b.re};}
};
struct WavetableWarpSpectrum {WavetableQuadrature harmonic[2*kWarpHarmonics+1];};
WavetableWarpSpectrum kWarpSpectra[4][kWarpDepthSteps+1]{};
WavetableQuadrature kUpperSource[kWavetableWaves][kUpperHarmonics-1]{};
static void prepare_warp_spectra(const int16_t (*bank)[kWavetableLevels][kWavetableSamples]){
 for(unsigned wave=0;wave<kWavetableWaves;++wave)for(unsigned h=2;h<=kUpperHarmonics;++h){
  auto& a=kUpperSource[wave][h-2];
  constexpr unsigned size=4*kUpperHarmonics;
  for(unsigned i=0;i<size;++i){
   const float p=float(i*h)/size,x=bank[wave][kUpperMip][i*(kWavetableSamples/size)]*(2.f/size);
   a.re+=x*wavetable_sine(p+.25f);a.im+=x*wavetable_sine(p);
  }
 }
 // Project only the periodic warp, once on the control owner. PM has an
 // independent phase and ratio and is expanded separately during synthesis.
 constexpr unsigned samples=128;
 for(unsigned mode=0;mode<4;++mode)for(unsigned d=0;d<=kWarpDepthSteps;++d){
  auto& spectrum=kWarpSpectra[mode][d];const float amount=2.f*d/kWarpDepthSteps-1;
  for(unsigned i=0;i<samples;++i){
   const float p=float(i)/samples;
   const auto map=wavetable_read_phases(p,mode,amount,0,0);
   auto phase=[](float x)->WavetableQuadrature{return {wavetable_sine(x+.25f),wavetable_sine(x)};};
   auto source=phase(map.first).mix(phase(map.second),map.blend);
   for(int h=-kWarpHarmonics;h<=kWarpHarmonics;++h){
    auto& a=spectrum.harmonic[h+kWarpHarmonics];const auto term=source.times(phase(-h*p));
    a.re+=term.re/samples;a.im+=term.im/samples;
   }
  }
 }
}
}
const WavetableBankView* factory_wavetable(unsigned index){
 return index>=1 && index<=2?&factories[index-1]:nullptr;
}
WavetableBankView read_wavetable_asset(const WavView& sample){
 if(!sample.data || sample.reader || sample.format!=1 || sample.channels!=1 || sample.bits!=16 ||
    sample.frames<4 || (uintptr_t(sample.data)&1) || !wav_tag(sample.data,"YWT1"))return {};
 unsigned count=wav_u16(sample.data+4);
 if(count<2 || count>16 || wav_u16(sample.data+6)!=0 || sample.frames!=4+count*(7*256+30))return {};
 return {reinterpret_cast<const int16_t*>(sample.data+8),count};
}
// At high pitches the carrier and PM sidebands can cross Nyquist separately.
// Sum the admitted components at their physical frequencies. This keeps the
// Fourier coefficients (table scan and moving controls) outside the phase map.
float wavetable_spectral_sample(WavetablePhaseMap map,float step,float carrier,float modulator,
                                       float position,unsigned mip,float mip_blend,const WavetableBankView* bank){
 const float depth=(map.warp+1)*(.5f*kWarpDepthSteps);
 unsigned bin=unsigned(depth);if(bin>=kWarpDepthSteps)bin=kWarpDepthSteps-1;
 const auto* spectra=kWarpSpectra[map.mode]+bin;
 auto rotation=[](float p)->WavetableQuadrature{return {wavetable_sine(p+.25f),wavetable_sine(p)};};
 WavetableQuadrature c[2*kWarpHarmonics+1],m[2*kPmSidebands+1];
 auto powers=[](WavetableQuadrature* center,int length,WavetableQuadrature unit){
  center[0]={1,0};
  for(int i=1;i<=length;++i){center[i]=center[i-1].times(unit);center[-i]={center[i].re,-center[i].im};}
 };
 powers(c+kWarpHarmonics,kWarpHarmonics,rotation(carrier));
 powers(m+kPmSidebands,kPmSidebands,rotation(modulator));
 // Jacobi-Anger expansion. The modulation index is at most 0.35*2*pi.
 // Four terms of each Bessel series suffice over that bounded interval.
 const float x=1.09955743f*map.pm,q=x*x;
 const float j[]={1-q*(1-q*(.25f-q*(1.f/36-q/576))),
  x*(1-q*(.5f-q*(1.f/12-q*(1.f/144-q/2880)))),
  q*(.5f-q*(1.f/6-q*(1.f/48-q/720))),
  x*q*(1.f/6-q*(1.f/24-q*(1.f/240-q/4320)))};
 const float side[]={-j[3],j[2],-j[1],j[0],j[1],j[2],j[3]};
 // If every retained sideband is in the flat passband, the phase
 // modulation is common to all harmonics. Factor it out of the sum.
 const bool full_band=(kWarpHarmonics+(map.pm>0?kPmSidebands*map.ratio:0))*step<=.43f;
 const WavetableQuadrature common={j[0]+2*j[2]*m[kPmSidebands+2].re,
                                   2*(j[1]*m[kPmSidebands+1].im+j[3]*m[kPmSidebands+3].im)};
 WavetableQuadrature result{};
 for(int h=-kWarpHarmonics;h<=kWarpHarmonics;++h){
  const unsigned k=unsigned(h+kWarpHarmonics);
  const auto a=spectra[0].harmonic[k].mix(spectra[1].harmonic[k],depth-bin);
  auto term=a.times(c[k]);
  if(!full_band){
   WavetableQuadrature pm{};
   const int extent=map.pm>0?kPmSidebands:0;
   for(int n=-extent;n<=extent;++n){
    const float frequency=fabsf((h+n*map.ratio)*step);
    if(frequency>=.48f)continue;
    const float gain=side[n+kPmSidebands]*fminf(1,(.48f-frequency)*20);
    pm.re+=gain*m[n+kPmSidebands].re;pm.im+=gain*m[n+kPmSidebands].im;
   }
   term=term.times(pm);
  }
  result.re+=term.re;result.im+=term.im;
 }
 if(full_band)result=result.times(common);
 const unsigned count=bank?bank->frames:kWavetableWaves;
 const unsigned a=unsigned(position),b=a+1<count?a+1:a;
 const float mix=position-a;
 const auto* first=bank?bank->wave(a,6):kWavetableData[a][6];const auto* second=bank?bank->wave(b,6):kWavetableData[b][6];
 const float cosine=first[0]+mix*(second[0]-first[0]),sine=first[64]+mix*(second[64]-first[64]);
 float sample=cosine*result.re+sine*result.im;
 // Preserve the source's remaining in-band partials at low warp/PM depths.
 // The same slope, brightness and mip blend as the oversampled path remove
 // them progressively as the warped fundamental needs more bandwidth.
 const unsigned limit=64u>>mip;
 auto osc=c[kWarpHarmonics+1];
 for(unsigned h=2;h<=kUpperHarmonics && h<=limit;++h){
  osc=h<=kWarpHarmonics?c[kWarpHarmonics+h]:osc.times(c[kWarpHarmonics+1]);
  WavetableQuadrature part;
  if(bank){
   const auto* x=bank->partial(a,h);const auto* y=bank->partial(b,h);
   part={x[0]+mix*(y[0]-x[0]),x[1]+mix*(y[1]-x[1])};
  }else part=kUpperSource[a][h-2].mix(kUpperSource[b][h-2],mix);
  sample+=(part.re*osc.re+part.im*osc.im)*(h>limit/2?1-mip_blend:1);
 }
 return sample/32767.f;
}
bool prepare_wavetable_bank(){
 if(__atomic_load_n(&storage.state,__ATOMIC_ACQUIRE)==2)return true;
 unsigned expected=0;if(!__atomic_compare_exchange_n(&storage.state,&expected,1u,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return false;
 constexpr unsigned samples=kWavetableWaves*kWavetableLevels*kWavetableSamples,bytes=samples*sizeof(int16_t);
 auto* raw=allocation_admitted(bytes)?alloc_uninitialized_bytes(bytes):nullptr;bool ok=raw!=nullptr;unsigned at=0;ProjectCrc crc;
 for(unsigned i=0;ok && i<samples;){
  int state[4]={0,0,0,0};
  for(unsigned group=0;ok && group<16;++group){
   if(at==sizeof kWavetablePacked){ok=false;break;}
   const unsigned exponent=kWavetablePacked[at++];
   if(exponent>19){ok=false;break;}
   unsigned bit=8;uint8_t packed=0;
   auto read_bit=[&]()->unsigned{
    if(bit==8){if(at==sizeof kWavetablePacked){ok=false;return 0;}packed=kWavetablePacked[at++];bit=0;}
    return (packed>>bit++)&1u;
   };
   for(unsigned n=0;ok && n<16;++n,++i){
    uint32_t quotient=0;
    while(ok && !read_bit())if(++quotient>(1048560u>>exponent)){ok=false;break;}
    uint32_t code=quotient<<exponent;
    for(unsigned shift=0;ok && shift<exponent;++shift)code|=read_bit()<<shift;
    if(!ok || code>1048560u){ok=false;break;}
    int decoded=(code&1)?-int(code>>1)-1:int(code>>1);
    // Validate every derivative before the next addition, including corrupt ROM.
    const int limits[4]={32768,65535,131070,262140};
    for(int level=3;level>=0;--level){
     decoded+=state[level];
     if(decoded< -limits[level] || decoded>limits[level] || (level==0 && decoded==32768)){ok=false;break;}
     state[level]=decoded;
    }
    if(!ok)break;
    uint16_t word=uint16_t(int16_t(decoded));raw[2*i]=uint8_t(word);raw[2*i+1]=uint8_t(word>>8);crc.add(raw[2*i]);crc.add(raw[2*i+1]);
   }
  }
 }
 ok=ok && at==sizeof kWavetablePacked && crc.value()==kWavetableCrc;
 if(!ok){delete[] raw;__atomic_store_n(&storage.state,0u,__ATOMIC_RELEASE);return false;}
 const auto bank=reinterpret_cast<const int16_t (*)[kWavetableLevels][kWavetableSamples]>(raw);
 prepare_warp_spectra(bank);storage.bytes=raw;
 __atomic_store_n(&kWavetableData,bank,__ATOMIC_RELEASE);
 __atomic_store_n(&storage.state,2u,__ATOMIC_RELEASE);return true;
}
}
