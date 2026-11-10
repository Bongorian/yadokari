#include "engine.h"
#include "factory_content.h"
#include "session_files.h"
#include "wav_writer.h"
#include "../../platform/desktop/file_access.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "factory_wav.h"
#include "drum_gm.h"
using namespace groove;
// Development fixtures only. Official Preset/Kit masters live in
// assets/factory-library; do not maintain a second legacy preset generator.
static void save(DesktopFiles& files,const char* path,const ProjectState& project){auto copy=std::make_unique<ProjectState>(project);assert(project_paths(*copy,path,false));FileWriter writer(files,path);assert(encode_project(writer,*copy) && writer.finish());}
int main(){DesktopFiles files;for(const char* path:{"assets/factory","assets/kits"})assert(files.directory(path));
 const char* names[]={"808-Kick","909-Kick","808-Snare","909-Snare","Closed-Hat","Open-Hat","Clap","Noise-Rim","FM-Tom-Low","FM-Tom-High","FM-Cowbell","FM-Metal","Chip-Perc","Acid-Zap","VA-Hit","Noise-Sweep"};
 const Instrument kinds[]={Instrument::DaisyAnalogDrums,Instrument::DaisySyntheticDrums,Instrument::DaisyAnalogDrums,Instrument::DaisySyntheticDrums,Instrument::DaisyAnalogDrums,Instrument::DaisySyntheticDrums,Instrument::DaisyAnalogDrums,Instrument::FM,Instrument::FM,Instrument::FM,Instrument::FM,Instrument::FM,Instrument::ChipClock,Instrument::Acid,Instrument::PolySaw,Instrument::VirtualAnalog};
 const unsigned notes[]={60,63,60,60,60,60,60,60,41,53,69,78,60,43,48,60};
 auto source=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>();float block[256];std::vector<float> sound(48000*2);
 for(unsigned i=0;i<16;++i){make_empty_project(*source);auto t=track_at(*source,0,0);t.instrument=kinds[i];t.root_note=uint8_t(notes[i]);t.cutoff=i==7?0.5f:1;t.drive=i==1?0.2f:0;t.engine_parameters[0]=float(uint16_t(1800+i*613))/16383.0f;t.engine_parameters[1]=float(uint16_t(i==10?11000:6500))/16383.0f;t.engine_parameters[3]=float(7000)/16383.0f;t.engine_parameters[4]=float(4000)/16383.0f;if(i<7)t.engine_parameters[0]=i<2?0:i<4 || i==6?.5f:1;if(i==15){t.virtual_analog.values[7]=1;t.virtual_analog.values[13]=0;t.virtual_analog.values[12]=.6f;}assert(engine->stage_project(*source));assert(engine->submit({CommandType::Trigger,0,0,127}));float peak=0;
  for(unsigned n=0;n<48000;n+=128){engine->render(block,128);for(unsigned j=0;j<256;++j){unsigned frame=n+j/2;float decay=i==5?0.5f:i==6?0.09f:i==7?0.04f:i>=8?0.3f:0.25f;float env=std::exp(-float(frame)/(48000*decay));if(i==5)env=std::pow(env,0.22f);float v=block[j]*env;sound[2*n+j]=v;peak=std::fmax(peak,std::fabs(v));}}// Original procedural noise layers complement the shared drum render.
  if(i==5 || i==6){uint32_t seed=0x3657ab19;float previous=0;peak=0;for(unsigned n=0;n<48000;++n){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;float noise=float(int32_t(seed>>16)-32768)/32768;float env=i==5?std::exp(-float(n)/(48000*0.17f)):std::exp(-float(n)/(48000*0.085f))*(0.4f+0.6f*(n%900<500));float v=(noise-previous)*env;previous=noise;sound[2*n]=sound[2*n+1]=v;peak=std::fmax(peak,std::fabs(v));}}
  assert(peak>0.00001f);for(auto& v:sound)v*=0.78f/peak;
  char path[96];std::snprintf(path,sizeof path,"assets/factory/%s.wav",names[i]);if(!save_factory_wav(files,path,sound.data(),48000))return 1;
 }
 auto kit=std::make_unique<ProjectState>();make_empty_project(*kit);kit->content_kind=ContentKind::Kit;auto techno=track_at(*kit,0,0);techno.instrument=Instrument::DrumKit;techno.preset_identity=builtin_factory_identity(0x302);copy_text(techno.preset_name,96,"Techno-01");
 const unsigned order[]={0,2,4,5,6,7,8,9,10,11,12,13,14,15,1,3};const uint8_t gm[]={36,38,42,46,39,37,45,50,56,51,75,76,77,55,35,40};for(unsigned pad=0;pad<16;++pad){unsigned i=order[pad];char path[96];std::snprintf(path,sizeof path,"assets/factory/%s.wav",names[i]);copy_text(kit->assets[pad].path,96,path);auto& m=kit->assets[pad].metadata;m.rate=48000;m.frames=48000;m.bits=16;m.channels=2;m.root=60;assert(file_checksum(files,path,m.content_crc));auto& sound=techno.kit.pads[pad];sound.gm_note=gm[pad];sound.sample[0]=uint8_t(pad);sound.level=pad==0?.6f:.4f;sound.edit.choke=pad==2 || pad==3?1:0;}
 save(files,"assets/kits/Techno-01.mgxk",*kit);
 std::vector<uint8_t> seed(96044);unsigned got=0;FactoryInstallReport report;assert(files.read("FACTORY/DEBUG/demo-slices.wav",0,seed.data(),unsigned(seed.size()),got) && got==seed.size());assert(install_factory_content(files,seed.data(),got,report));
 auto boot=std::make_unique<ProjectState>();make_benchmark_project(*boot);save(files,"build/factory-boot.mgxs",*boot);
 std::puts("Factory development fixtures: 16 original DSP WAVs, named Techno kit, operational factory install and benchmark. Official masters: assets/factory-library.");
}
