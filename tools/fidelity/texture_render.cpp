#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "stmlib/utils/random.h"
#ifdef GROOVE_ADAPTER
#include "texture_dsp.h"
using Frame=groove::TextureFrame;
#else
#include "clouds/dsp/granular_processor.h"
using Frame=clouds::ShortFrame;
struct Native {
 clouds::GranularProcessor dsp;
 alignas(16) unsigned char large[118784],small[65536-128];
 bool freeze=false,trigger=false;
 void init(unsigned seed){memset(static_cast<void*>(this),0,sizeof *this);stmlib::Random::Seed(seed);dsp.Init(large,sizeof large,small,sizeof small);}
 void process(const float* p,Frame* in,Frame* out){
  dsp.set_playback_mode(clouds::PlaybackMode(unsigned(p[0])));dsp.set_quality(int(p[11]));
  clouds::Parameters* q=dsp.mutable_parameters();q->position=p[1];q->size=p[2];q->pitch=p[3];q->density=p[4];q->texture=p[5];q->dry_wet=p[6];q->stereo_spread=p[7];q->feedback=p[8];q->reverb=p[9];
  bool f=p[10]>=.5f,tr=p[13]>=.5f;if(f!=freeze)dsp.set_freeze(f);freeze=f;q->trigger=tr && !trigger;trigger=tr;q->gate=p[14]>=.5f;
  dsp.Prepare();dsp.Process(in,out,32);
 }
};
#endif
int main(int argc,char** argv){
 if(argc!=2)return 2;FILE* output=fopen(argv[1],"wb");if(!output)return 3;
 for(unsigned source=0;source<2;++source){
  FILE* input=fopen(source?"build/fidelity/texture-input-pad.wav":"build/fidelity/texture-input-drum.wav","rb");if(!input)return 4;
  // Fixtures have the canonical PCM WAV header written by Python wave.
  unsigned char header[44];if(fread(header,1,44,input)!=44 || memcmp(header,"RIFF",4) || memcmp(header+36,"data",4))return 5;
  std::vector<Frame> signal(128000);if(fread(signal.data(),sizeof(Frame),signal.size(),input)!=signal.size())return 6;fclose(input);
  for(unsigned mode=0;mode<4;++mode)for(unsigned quality=0;quality<4;++quality)for(unsigned grid=0;grid<4;++grid){
#ifdef GROOVE_ADAPTER
   auto dsp=std::make_unique<groove::TextureDsp>();if(!dsp->ready())return 7;
#else
   auto dsp=std::make_unique<Native>();
#endif
   dsp->init(0x12345678);Frame out[32];float f[64];
   for(unsigned frame=0;frame<128000;frame+=32){
    float x=float(frame)/127968;
    float p[15]={float(mode),.2f,.5f,0,.6f,.5f,1,.7f,.15f,.1f,0,float(quality),1,0,0};
    if(grid==0){p[1]=x;p[2]=.1f+.8f*x;}
    if(grid==1){p[4]=x;p[5]=x;p[3]=-12+24*x;}
    if(grid==2){p[6]=.3f+.7f*x;p[7]=x;p[8]=.7f*x;p[9]=.8f*x;}
    if(grid==3){p[10]=frame>=64000 && frame<112000;p[13]=frame%16000==0;p[14]=frame%32000<16000;}
    dsp->process(p,signal.data()+frame,out);
    for(unsigned i=0;i<32;++i){f[2*i]=out[i].l/32768.f;f[2*i+1]=out[i].r/32768.f;}
    if(fwrite(f,sizeof(float),64,output)!=64)return 8;
   }
  }
 }
 return fclose(output)?9:0;
}
