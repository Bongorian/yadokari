#include "engine.h"
#include "performance.h"
#include "demo_project.h"
#include "project_codec.h"
#include "session_files.h"
#include "wav_writer.h"
#include "ui_renderer.h"
#include "../platform/desktop/sample_assets.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
struct Writer{FILE* file;bool write(const uint8_t* p,size_t n){return std::fwrite(p,1,n,file)==n;}};
struct Reader{FILE* file;bool read(uint8_t* p,size_t n){return std::fread(p,1,n,file)==n;}bool at_end(){return std::fgetc(file)==EOF && !std::ferror(file);}};
struct Input{uint64_t sample;groove::ControllerInput event;};
int main(int argc,char** argv){
 const char* stem=argc>1?argv[1]:"build/ultimate-demo";unsigned seconds=8;const char* input_path=nullptr;const char* event_path=nullptr;bool stress=false,generative=false;
 for(int i=2;i<argc;++i){if(std::strcmp(argv[i],"--generative")==0)generative=true;else if(std::strcmp(argv[i],"--stress")==0)stress=true;else if(std::strcmp(argv[i],"--seconds")==0 && i+1<argc)seconds=unsigned(std::atoi(argv[++i]));else if(std::strcmp(argv[i],"--project")==0 && i+1<argc)input_path=argv[++i];else if(std::strcmp(argv[i],"--events")==0 && i+1<argc)event_path=argv[++i];else return 1;}
 if(!seconds || seconds>120)return 1;
 auto project=std::make_unique<groove::ProjectState>();if(generative)groove::make_generative_demo(*project);else groove::make_demo(*project);
 groove::DesktopFiles files;if(input_path)groove::make_empty_project(*project);
 groove::DesktopSampleAssets assets;assets.files(&files);groove::WavView bindings[groove::kSampleAssets]{};if(!input_path && !assets.prepare_authored(*project,bindings))return 1;
 auto engine=std::make_unique<groove::Engine>();if(!engine->stage_project(*project,bindings))return 1;groove::Performance perf(*engine,*project);groove::SessionFiles session(files,assets,*engine,*project,perf);session.set_bindings(bindings);float maintenance[2]{};engine->render(maintenance,0);perf.project_loaded();perf.poll();if(input_path){if(!session.load(input_path))return 1;engine->render(maintenance,0);perf.poll();session.copy_bindings(bindings);}if(!session.save((std::string(stem)+"-initial.mgxs").c_str()))return 1;using Type=groove::ControllerInput::Type;
 auto display=stress?std::make_unique<groove::UiRenderer>():nullptr;unsigned ui_frames=0,roundtrips=0;
 std::vector<Input> inputs;
 if(event_path){FILE* f=std::fopen(event_path,"r");if(!f)return 1;unsigned long long time;unsigned type,index,pressure;int value;while(std::fscanf(f,"%llu,%u,%u,%d,%u\n",&time,&type,&index,&value,&pressure)==5){if(type>unsigned(Type::KnobTouch)||index>255||pressure>4095)return 1;inputs.push_back({time,{Type(type),uint8_t(index),value,uint16_t(pressure)}});}std::fclose(f);if(!std::is_sorted(inputs.begin(),inputs.end(),[](const Input& a,const Input& b){return a.sample<b.sample;}))return 1;}
 else inputs={{0,{Type::Play}},{24000,{Type::StoreMorph,0}},{48000,{Type::Mode,0,int(groove::UiMode::Mixer)}},{48000,{Type::Knob,2,-8000}},{60000,{Type::StoreMorph,1}},{72000,{Type::Strip,0,8192}},{80000,{Type::Mode,0,int(groove::UiMode::Scene)}},{80000,{Type::Pad,13,0x10,3000}},{80100,{Type::Pad,13,0x20,0}},{120000,{Type::Stutter,0,1}},{132000,{Type::Stutter,0,0}},{150000,{Type::Fill,0,1}},{168000,{Type::Fill,0,0}},{192000,{Type::Mode,0,int(groove::UiMode::Generate)}},{192000,{Type::Track,2}},{192000,{Type::Euclidean,3,7}}};
 std::string base(stem);FILE* midi=std::fopen((base+"-midi.csv").c_str(),"w"),*events=std::fopen((base+"-events.csv").c_str(),"w");if(!midi || !events)return 1;
 std::vector<float> output(size_t(seconds)*48000*2);uint64_t cursor=0;size_t input=0;std::vector<double> costs;double max_us=0;unsigned over_budget=0,clips=0;float peak=0;
 const auto total_start=std::chrono::steady_clock::now();
 while(cursor<uint64_t(seconds)*48000){
  session.service();
  if(stress && cursor%3200==0){display->draw(*project,perf);++ui_frames;}
  if(stress && cursor && cursor%48000==0){
   FILE* state=std::fopen((base+"-stress.tmp").c_str(),"wb");if(!state)return 1;Writer writer{state};bool valid=groove::encode_project(writer,*project);if(std::fclose(state)!=0)valid=false;if(!valid)return 1;
   state=std::fopen((base+"-stress.tmp").c_str(),"rb");if(!state)return 1;auto staged=std::make_unique<groove::ProjectState>();Reader reader{state};valid=groove::decode_project(reader,*staged);if(std::fclose(state)!=0)valid=false;if(!valid || !engine->stage_project(*staged,bindings))return 1;*project=*staged;perf.project_loaded();++roundtrips;
  }
while(input<inputs.size() && inputs[input].sample==cursor){const auto& e=inputs[input++].event;if(!perf.dispatch(e)){std::fprintf(stderr,"input rejected at sample %llu: type=%u index=%u value=%d\n",(unsigned long long)cursor,unsigned(e.type),e.index,e.value);return 1;}}
  uint64_t count=uint64_t(seconds)*48000-cursor;if(count>128)count=128;if(input<inputs.size() && inputs[input].sample>cursor && count>inputs[input].sample-cursor)count=inputs[input].sample-cursor;
  if(stress && count>3200-cursor%3200)count=3200-cursor%3200;
  auto start=std::chrono::steady_clock::now();engine->render(output.data()+cursor*2,size_t(count));double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();costs.push_back(us);if(us>max_us)max_us=us;if(us>double(count)*1000000/48000)++over_budget;
  for(uint64_t i=cursor*2;i<(cursor+count)*2;++i){float x=output[size_t(i)];if(!std::isfinite(x))return 1;if(std::fabs(x)>peak)peak=std::fabs(x);if(std::fabs(x)>=1)++clips;}
  groove::EngineEvent event;while(engine->pop_event(event)){perf.accept_event(event);std::fprintf(events,"%llu,%u,%u,%u\n",(unsigned long long)event.sample,unsigned(event.type),event.track,event.value);}
  // Keep UI state current without consuming the event queue a second time.
  groove::MidiEvent message;while(engine->pop_midi(message))std::fprintf(midi,"%llu,%u,%u,%u,%u\n",(unsigned long long)message.sample,message.length,message.bytes[0],message.bytes[1],message.bytes[2]);groove::ClockEvent clock;while(engine->pop_clock(clock)){}cursor+=count;
 }
 std::fclose(events);std::fclose(midi);FILE* wav=std::fopen((base+".wav").c_str(),"wb");if(!wav)return 1;Writer writer{wav};bool ok=groove::write_wav(writer,output.data(),seconds*48000);if(std::fclose(wav)!=0)ok=false;
 ok=session.save((base+".mgxs").c_str()) && ok;
 std::sort(costs.begin(),costs.end());FILE* stats=std::fopen((base+"-metrics.txt").c_str(),"w");if(!stats)return 1;std::fprintf(stats,"host_sanitized=1\nframes=%u\npeak=%.8f\nclipped_samples=%u\nmedian_callback_us=%.3f\np95_callback_us=%.3f\nmax_callback_us=%.3f\nover_budget_callbacks=%u\n",seconds*48000,double(peak),clips,costs[costs.size()/2],costs[costs.size()*95/100],max_us,over_budget);std::fprintf(stats,"ui_frames=%u\nproject_roundtrips=%u\nwall_seconds=%.3f\n",ui_frames,roundtrips,std::chrono::duration<double>(std::chrono::steady_clock::now()-total_start).count());std::fclose(stats);
 std::printf("session render: %u seconds peak %.5f clipped %u max callback %.1f us (host, sanitizer build)\n",seconds,double(peak),clips,max_us);return ok && peak>0 && !clips?0:1;
}
