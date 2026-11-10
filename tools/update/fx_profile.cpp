#include "benchmark_project.h"
#include "engine.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: fx-profile sample.wav\n");return 2;}
 std::ifstream input(argv[1],std::ios::binary);if(!input)return 2;
 std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)),{});
 groove::WavView views[groove::kSampleAssets]{};if(!groove::parse_wav(data.data(),data.size(),views[0]))return 2;
 auto project=std::make_unique<groove::ProjectState>();groove::make_fx_stress_project(*project);
 auto engine=std::make_unique<groove::Engine>(48000);if(!engine->stage_project(*project,views))return 2;
 float output[48]{};engine->render(output,0);if(!engine->submit({groove::CommandType::Start,0,0,0}))return 2;
 constexpr unsigned frames=48000*30;uint64_t hash=14695981039346656037ull;auto begin=std::chrono::steady_clock::now();
 for(unsigned at=0;at<frames;at+=24){if(at%240000==0){const unsigned modes[]={0,9,12,10,13,11};groove::Command fx{groove::CommandType::SetPerformanceFx,0,0,uint16_t(modes[at/240000])};fx.scalar=0.7f;if(!engine->submit(fx))return 2;}if(at%12000==0){unsigned phase=(at/12000)%16;groove::Command c{groove::CommandType::SetMorph,0,0,0};c.scalar=float(phase<=8?phase:16-phase)/8;if(!engine->submit(c))return 2;}engine->render(output,24);for(float sample:output){uint32_t bits;std::memcpy(&bits,&sample,sizeof bits);hash^=bits;hash*=1099511628211ull;}}
 double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
 std::printf("rendered %u frames in %.3f s, ratio %.3f hash %016llx\n",frames,seconds,seconds/(frames/48000.0),static_cast<unsigned long long>(hash));
}
