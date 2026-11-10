#include "readiness_workflow.h"
#include <SDL.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <memory>
#include "performance.h"
#include "midi_input_router.h"
#include "project_codec.h"
#include "ui_renderer.h"
#include "creative_demo.h"
#include "demo_project.h"
#include "factory_content.h"
#include "session_actions.h"
#include "../../examples/workflow_demo.h"
#include "ui/framebuffer565.h"
#include "wav_writer.h"
#include "sample_assets.h"
#include "file_access.h"
#include "session_files.h"
static bool assign_sample(const char* source,groove::SessionFiles& session) {
 groove::DesktopFiles fs;char imported[160];const char* path=source;
 if(source[0]=='/'){if(!fs.directory("assets/imported"))return false;uint32_t hash;if(!groove::file_checksum(fs,source,hash))return false;char name[112];std::snprintf(name,sizeof name,"%08x-%s",hash,groove::filename(source));if(!groove::join_path(imported,sizeof imported,"assets/imported",name))return false;if(!fs.size(imported) && !groove::copy_file(fs,source,imported))return false;path=imported;}

 return session.load_sample_path(path);
}
class DesktopClock : public groove::IClockSource {public:uint64_t microseconds()const override{uint64_t value=SDL_GetPerformanceCounter(),rate=SDL_GetPerformanceFrequency();return (value/rate)*1000000+(value%rate)*1000000/rate;}};
class Audio : public groove::IAudioBackend {
public:
 explicit Audio(groove::Engine& engine):engine_(engine){}
 ~Audio(){stop();}
 std::atomic<unsigned> callbacks{0},max_us{0},over_budget{0};std::atomic<uint64_t> frames{0};std::atomic<float> peak{0};
 bool start() override {SDL_AudioSpec wanted{},obtained{};wanted.freq=48000;wanted.format=AUDIO_F32SYS;wanted.channels=listen_cue_?2:4;wanted.samples=256;wanted.callback=callback;wanted.userdata=this;device_=SDL_OpenAudioDevice(nullptr,0,&wanted,&obtained,0);if(!device_){wanted.channels=2;device_=SDL_OpenAudioDevice(nullptr,0,&wanted,&obtained,0);}if(!device_)return false;channels_=obtained.channels;SDL_PauseAudioDevice(device_,0);return true;}
 void stop() override {if(device_){SDL_CloseAudioDevice(device_);device_=0;}}
 unsigned sample_rate()const override{return 48000;}
void input(groove::WavView view){input_.start(view,48000,1,true);}
 void listen_cue(bool yes){listen_cue_=yes;}
private:
 static void callback(void* context,Uint8* data,int bytes) {
  auto& a=*static_cast<Audio*>(context);auto* samples=reinterpret_cast<float*>(data);unsigned count=unsigned(bytes/int(sizeof(float))),frames=count/a.channels_;uint64_t start=SDL_GetPerformanceCounter();
  float main[512],cue[512],input[512];for(unsigned at=0;at<frames;){unsigned n=frames-at<256?frames-at:256;for(unsigned i=0;i<n;++i){float l=0,r=0;if(a.input_.active())a.input_.next(l,r);input[2*i]=l;input[2*i+1]=r;}a.engine_.render_ports(main,cue,input,n);for(unsigned i=0;i<n;++i)for(unsigned c=0;c<a.channels_;++c)samples[(at+i)*a.channels_+c]=a.channels_==2?(a.listen_cue_?cue[2*i+c]:main[2*i+c]):c<2?main[2*i+c]:cue[2*i+c-2];at+=n;}
  float p=0;for(unsigned i=0;i<count;++i){float v=std::fabs(samples[i]);if(v>p)p=v;}float old=a.peak.load(std::memory_order_relaxed);if(p>old)a.peak.store(p,std::memory_order_relaxed);
  unsigned elapsed=unsigned((SDL_GetPerformanceCounter()-start)*1000000/SDL_GetPerformanceFrequency());unsigned previous=a.max_us.load();if(elapsed>previous)a.max_us.store(elapsed);if(uint64_t(elapsed)*48000>uint64_t(frames)*1000000)a.over_budget.fetch_add(1);
  a.frames.fetch_add(frames,std::memory_order_release);a.callbacks.fetch_add(1,std::memory_order_relaxed);
 }
 groove::Engine& engine_;unsigned channels_=2;groove::WavVoice input_;bool listen_cue_=false;SDL_AudioDeviceID device_=0;
};
class DesktopDisplay : public groove::IDisplayBackend {
public:
 explicit DesktopDisplay(SDL_Renderer* renderer):renderer_(renderer){for(auto& texture:textures_)texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,480,272);}
 ~DesktopDisplay(){for(auto* texture:textures_)if(texture)SDL_DestroyTexture(texture);}
 void settings(const groove::ui::VisualSettings& s){preview_=s.preview565;order_=groove::ui::PixelOrder(s.pixel_order);}
 bool present(unsigned side,const uint32_t* argb,unsigned width,unsigned height)override {if(side>1 || width!=480 || height!=272 || !textures_[side])return false;const uint32_t* pixels=argb;if(preview_){for(unsigned i=0;i<480*272;++i)preview_pixels_[i]=groove::ui::preview565(argb[i],order_);pixels=preview_pixels_;}SDL_Rect destination{10+int(side)*500,44,480,272};return SDL_UpdateTexture(textures_[side],nullptr,pixels,480*4)==0 && SDL_RenderCopy(renderer_,textures_[side],nullptr,&destination)==0;}
private:SDL_Renderer* renderer_;SDL_Texture* textures_[2]{};uint32_t preview_pixels_[480*272]{};bool preview_=true;groove::ui::PixelOrder order_=groove::ui::PixelOrder::RGB;
};
static void rect(SDL_Renderer* r,int x,int y,int w,int h,Uint8 red,Uint8 green,Uint8 blue){SDL_SetRenderDrawColor(r,red,green,blue,255);SDL_Rect box{x,y,w,h};SDL_RenderFillRect(r,&box);}
static void label(SDL_Renderer* renderer,int x,int y,const char* text,unsigned scale=1,uint32_t color=0xffeaf1ef,int width=500){groove::ui::raster_text([&](int px,int py,unsigned alpha){SDL_SetRenderDrawColor(renderer,uint8_t(color>>16),uint8_t(color>>8),uint8_t(color),uint8_t(alpha));SDL_RenderDrawPoint(renderer,px,py);},x,y,text,scale>1?groove::ui::FontRole::Heading:groove::ui::FontRole::Small,width);}
static void rounded(SDL_Renderer* r,int x,int y,int w,int h,int radius,uint32_t color,unsigned opacity=255){
 SDL_SetRenderDrawColor(r,uint8_t(color>>16),uint8_t(color>>8),uint8_t(color),uint8_t(opacity));SDL_Rect middle{x+radius,y,w-radius*2,h};SDL_RenderFillRect(r,&middle);SDL_Rect left{x,y+radius,radius,h-2*radius},right{x+w-radius,y+radius,radius,h-2*radius};SDL_RenderFillRect(r,&left);SDL_RenderFillRect(r,&right);
 for(int j=0;j<radius;++j)for(int i=0;i<radius;++i){unsigned alpha=unsigned(groove::SoftwareCanvas::rounded_coverage(i+0.5f,j+0.5f,float(w),float(h),float(radius))*opacity);SDL_SetRenderDrawColor(r,uint8_t(color>>16),uint8_t(color>>8),uint8_t(color),uint8_t(alpha));SDL_RenderDrawPoint(r,x+i,y+j);SDL_RenderDrawPoint(r,x+w-i-1,y+j);SDL_RenderDrawPoint(r,x+i,y+h-j-1);SDL_RenderDrawPoint(r,x+w-i-1,y+h-j-1);}
}
static int pad_at(int x,int y){if(x<20 || y<382 || x>=340 || y>=702)return -1;if((x-20)%80>=70 || (y-382)%80>=70)return -1;return ((y-382)/80)*4+(x-20)/80;}
static int key_pad(SDL_Keycode key){const char* keys="1234qwerasdfzxcv";if(key<=0 || key>127)return -1;const char* p=std::strchr(keys,int(key));return p?int(p-keys):-1;}
struct FileWriter {FILE* file;bool write(const uint8_t* p,size_t n){return std::fwrite(p,1,n,file)==n;}};
struct TimedInput {uint64_t sample;groove::ControllerInput input;};
struct ScriptMidi {uint64_t sample;groove::MidiPacket packet;};
int main(int argc,char** argv) {
 bool self=false,generative=false,empty=false,workflow=false,creative=false,factory=false,cue_listen=false,readiness=false;const char* input_path=nullptr;const char* record_path=nullptr;const char* replay_path=nullptr;const char* midi_script_path=nullptr;
 for(int i=1;i<argc;++i){if(std::strcmp(argv[i],"--readiness-test")==0){readiness=true;empty=true;}else if(std::strcmp(argv[i],"--factory")==0)factory=true;else if(std::strcmp(argv[i],"--cue")==0)cue_listen=true;else if(std::strcmp(argv[i],"--input")==0 && i+1<argc)input_path=argv[++i];else if(std::strcmp(argv[i],"--creative")==0)creative=true;else if(std::strcmp(argv[i],"--generative")==0)generative=true;else if(std::strcmp(argv[i],"--empty")==0)empty=true;else if(std::strcmp(argv[i],"--workflow-test")==0){workflow=true;self=true;}else if(std::strcmp(argv[i],"--self-test")==0)self=true;else if(std::strcmp(argv[i],"--record")==0 && i+1<argc)record_path=argv[++i];else if(std::strcmp(argv[i],"--replay")==0 && i+1<argc)replay_path=argv[++i];else if(std::strcmp(argv[i],"--midi-in-csv")==0 && i+1<argc)midi_script_path=argv[++i];else{std::fprintf(stderr,"usage: %s [--empty | --factory | --generative | --creative] [--cue] [--input file.wav] [--self-test | --workflow-test] [--record file] [--replay file] [--midi-in-csv file]\n",argv[0]);return 1;}}
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0){std::fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 SDL_Window* window=SDL_CreateWindow("Virtual MK3 | Space play | 9 modes | [ ] patterns | F9/F10 morph A/B | F11 stutter | F12 fill | K capture",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1000,744,0);if(!window)return 1;
 SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer)return 1;SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
 auto engine=std::make_unique<groove::Engine>();auto project=std::make_unique<groove::ProjectState>();if(empty || factory)groove::make_empty_project(*project);else if(creative)groove::make_creative_demo(*project);else if(generative)groove::make_generative_demo(*project);else if(self)groove::make_demo(*project);else groove::make_benchmark_project(*project);groove::DesktopFiles files;if(!self){std::vector<uint8_t> seed(96044);unsigned got=0;groove::FactoryInstallReport report;if(!files.read("FACTORY/DEBUG/demo-slices.wav",0,seed.data(),unsigned(seed.size()),got) || got!=seed.size() || !groove::install_factory_content(files,seed.data(),got,report)){std::fprintf(stderr,"Factory catalog preparation failed\n");return 1;}}groove::DesktopSampleAssets assets;assets.files(&files);groove::WavView bindings[groove::kSampleAssets]{};if(!assets.prepare_authored(*project,bindings) || !engine->stage_project(*project,bindings))return 1;
 groove::Performance perf(*engine,*project);if(!self && !empty && !factory && !creative && !generative)perf.project_path(groove::kFactoryStartupProject);auto ui=std::make_unique<groove::UiRenderer>();Audio audio(*engine);DesktopClock clock;groove::SessionFiles session(files,assets,*engine,*project,perf);session.set_bindings(bindings);audio.listen_cue(cue_listen);if(input_path){groove::WavView input;if(!assets.load(input_path,input))return 1;audio.input(input);}if(factory){float initial[2];engine->render(initial,0);perf.poll();if(!session.load("FACTORY/KITS/Techno-01.mgxk",true)){std::fprintf(stderr,"factory kit load failed\n");return 1;}engine->render(initial,0);perf.poll();}
 FILE* recording=record_path?std::fopen(record_path,"w"):nullptr;if(record_path && !recording)return 1;
 std::vector<TimedInput> replay;if(replay_path){FILE* file=std::fopen(replay_path,"r");if(!file)return 1;unsigned long long time;unsigned type,index,pressure;int value;while(std::fscanf(file,"%llu,%u,%u,%d,%u\n",&time,&type,&index,&value,&pressure)==5){if(type>unsigned(groove::ControllerInput::Type::KnobTouch) || index>255 || pressure>4095)return 1;replay.push_back({time,{groove::ControllerInput::Type(type),uint8_t(index),value,uint16_t(pressure)}});}std::fclose(file);}
 auto midi_router=std::make_unique<groove::MidiInputRouter>();std::vector<ScriptMidi> midi_script;size_t midi_cursor=0;unsigned midi_rejected=0;
 if(midi_script_path){FILE* f=std::fopen(midi_script_path,"r");if(!f)return 1;unsigned long long at;unsigned cable,status,a,b;int n;while((n=std::fscanf(f,"%llu,%u,%u,%u,%u\n",&at,&cable,&status,&a,&b))==5){if(cable>15 || status>255 || a>127 || b>127 || (!midi_script.empty() && at<midi_script.back().sample)){std::fclose(f);return 1;}unsigned len=status>=0xf8?1:((status&0xf0)==0xc0 || (status&0xf0)==0xd0)?2:3;groove::MidiPacket packet{0,uint8_t(cable),uint8_t(len),{uint8_t(status),uint8_t(a),uint8_t(b)}};if(!groove::valid_midi_packet(packet)){std::fclose(f);return 1;}midi_script.push_back({at,packet});}if(n!=EOF){std::fclose(f);return 1;}std::fclose(f);}
 auto dispatch=[&](groove::ControllerInput input){bool ok=perf.dispatch(input);if(ok && recording)std::fprintf(recording,"%llu,%u,%u,%d,%u\n",(unsigned long long)audio.frames.load(),unsigned(input.type),input.index,input.value,input.pressure);return ok;};
 if(workflow){float stopped[2];engine->render(stopped,0);if(!groove::build_workflow_demo(*engine,*project,perf,[&](){session.service();engine->render(stopped,0);perf.poll();})){std::fprintf(stderr,"workflow setup failed\n");return 1;}}
 // The scripted sample-browser path starts in assets, independently of the DATA-root default.
 if(readiness)groove::copy_text(perf.browser().directory,sizeof perf.browser().directory,"assets");
 if(readiness && !desktop_readiness_workflow(*engine,*project,perf,session,files))return 1;
 if(readiness && (!dispatch({groove::ControllerInput::Type::Mode,0,int(groove::UiMode::Routing)}) || !dispatch({groove::ControllerInput::Type::Shift,0,1}) || !dispatch({groove::ControllerInput::Type::Play})))return 1;
 if(!audio.start()){std::fprintf(stderr,"audio: %s\n",SDL_GetError());return 1;}
 const char* self_dir=workflow?"build/virtual-workflow":"build/virtual-selftest";
 const char* self_path=workflow?"build/virtual-workflow/session.mgxs":"build/virtual-selftest/session.mgxs";
 if(self && !files.directory(self_dir))return 1;
 perf.project_path(self?self_path:"virtual-mk3.mgxs");
 using Type=groove::ControllerInput::Type;
 auto control_toggle=[&](unsigned byte,unsigned mask,bool on){return dispatch({Type::Control,uint8_t(byte),int((perf.control_value(byte)&~mask)|(on?mask:0))});};auto control_press=[&](unsigned byte,unsigned mask){control_toggle(byte,mask,true);control_toggle(byte,mask,false);};
 bool running=true,shift=false,strip=false,virtual_mute=false,virtual_solo=false,virtual_select=false,virtual_erase=false,capture_requested=false,resample=false,self_released=false,self_roundtrip=false,self_save_queued=false,self_save_done=false,self_load_queued=false,self_browser_opened=false,self_drop_sent=false,self_drop_ok=false,self_resampled=false;uint64_t resampled_at=0;unsigned knob=0;int mouse_pad=-1;size_t replay_cursor=0;uint64_t start=clock.microseconds();
 std::vector<float> capture(48000*4*2);unsigned captured=0;FILE* midi_log=self?std::fopen("build/virtual-midi.csv","w"):nullptr;
 if(self){dispatch({Type::Play});dispatch({Type::Pad,12,0x10,3500});dispatch({Type::Mode,0,int(groove::UiMode::Scene)});dispatch({Type::Pad,13,0x10,3000});}
 {
 DesktopDisplay display(renderer);
 while(running){
  SDL_Event event;while(SDL_PollEvent(&event)){
   if(event.type==SDL_QUIT)running=false;
   if(event.type==SDL_DROPFILE){bool ok=assign_sample(event.drop.file,session);if(self)self_drop_ok=ok;std::fprintf(stderr,"sample: %s\n",ok?"loaded":"failed (PCM16/24 or float32 WAV, path <96 bytes, cache limit)");SDL_free(event.drop.file);}
   if(event.type==SDL_KEYDOWN && !event.key.repeat){auto key=event.key.keysym.sym;int pad=key_pad(key);bool ctrl=event.key.keysym.mod&KMOD_CTRL;
    if(ctrl && key==SDLK_s)perf.request_save();
    else if(ctrl && key==SDLK_o)perf.request_project(groove::Performance::ProjectAction::Load);
    else if(ctrl && key==SDLK_d)control_press(5,0x04);
    else if(ctrl && key==SDLK_n)perf.request_project(groove::Performance::ProjectAction::New);
    else if(ctrl && key==SDLK_z)dispatch({shift?Type::Redo:Type::Undo});
    else if(ctrl && key==SDLK_c)dispatch({Type::ControlAll,0,perf.control_all()?0:1});
    else if(pad>=0)dispatch({Type::Pad,uint8_t(pad),0x10,3000});
    else if(key==SDLK_SPACE && ctrl)dispatch({Type::Panic});else if(key==SDLK_SPACE)dispatch({perf.playing()?Type::Stop:Type::Play});
    else if(key==SDLK_TAB)dispatch({Type::Mode,0,int(perf.editing()?groove::UiMode::Pad:groove::UiMode::Step)});
    else if(key>=SDLK_F1 && key<=SDLK_F8)dispatch({Type::Track,uint8_t(unsigned(key-SDLK_F1)+(shift?8:0))});
    else if(key==SDLK_LALT || key==SDLK_RALT){virtual_select=true;control_toggle(5,0x08,true);}
    else if(key==SDLK_BACKSPACE){virtual_erase=true;control_toggle(6,0x04,true);}
    else if(key==SDLK_RETURN && perf.pending_project()!=groove::Performance::ProjectAction::None)perf.confirm_project();
    else if(key==SDLK_LSHIFT || key==SDLK_RSHIFT){shift=true;dispatch({Type::Shift,0,1});}
    else if(key==SDLK_p)dispatch({Type::WaveProfile,0,perf.wave_profile()?0:1});else if(key==SDLK_m){virtual_mute=true;dispatch({Type::MuteModifier,0,1});}else if(key==SDLK_o){virtual_solo=true;dispatch({Type::SoloModifier,0,1});}else if(key==SDLK_i)dispatch({Type::Instrument});else if(key==SDLK_g)dispatch({Type::Euclidean,0,5});
    else if(key==SDLK_b)dispatch({Type::Mode,0,int(groove::UiMode::Browser)});else if(key==SDLK_9)dispatch({Type::Mode,0,(int(perf.mode())+1)%18});
    else if(key==SDLK_LEFTBRACKET)dispatch({Type::Pattern,uint8_t((project->active_pattern+15)%16)});else if(key==SDLK_RIGHTBRACKET)dispatch({Type::Pattern,uint8_t((project->active_pattern+1)%16)});
    else if(key==SDLK_UP)dispatch({Type::StepPage,uint8_t((perf.page()+15)%16)});else if(key==SDLK_DOWN)dispatch({Type::StepPage,uint8_t((perf.page()+1)%16)});
    else if(key==SDLK_F9 || key==SDLK_F10)dispatch({Type::StoreMorph,uint8_t(key==SDLK_F10)});
    else if(key==SDLK_F11)dispatch({Type::Stutter,0,1});else if(key==SDLK_F12)dispatch({Type::Fill,0,1});
    else if(key==SDLK_k){if(!capture_requested){capture_requested=engine->request_capture();resample=shift;}}else if(key==SDLK_ESCAPE){if(perf.pending_project()!=groove::Performance::ProjectAction::None)perf.cancel_project();else running=false;}
   }
   if(event.type==SDL_KEYUP){auto key=event.key.keysym.sym;int pad=key_pad(key);if(key==SDLK_LALT || key==SDLK_RALT){virtual_select=false;control_toggle(5,0x08,false);}if(key==SDLK_BACKSPACE){virtual_erase=false;control_toggle(6,0x04,false);}if(pad>=0)dispatch({Type::Pad,uint8_t(pad),0x20,0});if(key==SDLK_LSHIFT || key==SDLK_RSHIFT){shift=false;dispatch({Type::Shift,0,0});}if(key==SDLK_m){virtual_mute=false;dispatch({Type::MuteModifier,0,0});}if(key==SDLK_o){virtual_solo=false;dispatch({Type::SoloModifier,0,0});}if(key==SDLK_F11)dispatch({Type::Stutter,0,0});if(key==SDLK_F12)dispatch({Type::Fill,0,0});}
   if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT){int x=event.button.x,y=event.button.y;mouse_pad=pad_at(x,y);if(mouse_pad>=0)dispatch({Type::Pad,uint8_t(mouse_pad),0x10,uint16_t(shift?1200:3500)});else if(y>=10 && y<36 && x>=10 && x<990){unsigned side=unsigned((x-10)/500),column=unsigned((x-10)%500)/117;if(column<4)dispatch({Type::SoftKey,uint8_t(side*4+column)});}else if(y>=324 && y<348 && x>=10 && x<990){unsigned side=unsigned((x-10)/500),column=unsigned((x-10)%500)/117;if(column<4){knob=side*4+column;dispatch({Type::KnobTouch,uint8_t(knob),1});}}else if(y>=382 && y<460 && x>=400 && x<960){unsigned row=unsigned((y-382)/40),col=unsigned((x-400)/80);if(row<2 && col<7)dispatch({Type::Mode,0,int(row*7+col)});}else if(y>=458 && y<478 && x>=400 && x<960){unsigned action=unsigned(x-400)/70;if(action==0)control_press(8,0x10);else if(action==1)control_press(8,0x20);else if(action==2)control_press(5,0x04);else if(action==3){bool was=shift;dispatch({Type::Shift,0,1});control_press(5,0x04);dispatch({Type::Shift,0,was});}else if(action==4){virtual_select=!virtual_select;control_toggle(5,0x08,virtual_select);}else if(action==5){virtual_erase=!virtual_erase;control_toggle(6,0x04,virtual_erase);}else if(action==6)control_press(4,0x10);else if(action==7)control_press(6,0x40);}else if(y>=480 && y<530){if(x>=400 && x<490)dispatch({perf.playing()?Type::Stop:Type::Play});else if(x>=510 && x<600)dispatch({Type::Mode,0,int(groove::UiMode::Step)});else if(x>=620 && x<700){virtual_mute=!virtual_mute;dispatch({Type::MuteModifier,0,virtual_mute});}else if(x>=710 && x<790){virtual_solo=!virtual_solo;dispatch({Type::SoloModifier,0,virtual_solo});}else if(x>=800 && x<880)dispatch({Type::ControlAll,0,perf.control_all()?0:1});else if(x>=890 && x<970)dispatch({Type::WaveProfile,0,perf.wave_profile()?0:1});}else if(x>=335 && x<400 && y>=520 && y<582){uint8_t mask=y<535?0x04:y>565?0x10:x<352?0x20:x>380?0x08:0x01;dispatch({Type::Control,1,int(mask|(shift?0x40:0))});dispatch({Type::Control,1,shift?0x40:0});}else if(y>=650 && y<682 && x>=335 && x<400)dispatch({Type::Mode,0,int(groove::UiMode::Chord)});else if(y>=650 && y<682 && x>=400 && x<960)dispatch({Type::Track,uint8_t((shift?8:0)+(x-400)/70)});else if(y>=575 && y<610 && x>=400 && x<960)strip=true;}
   if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT){if(mouse_pad>=0)dispatch({Type::Pad,uint8_t(mouse_pad),0x20,0});mouse_pad=-1;strip=false;dispatch({Type::KnobTouch,uint8_t(knob),0});}
   if(event.type==SDL_MOUSEMOTION && mouse_pad>=0 && shift){int value=4095-((event.motion.y-382)%80)*4095/80;if(value<0)value=0;if(value>4095)value=4095;dispatch({Type::Pad,uint8_t(mouse_pad),0x40,uint16_t(value)});}
   if(event.type==SDL_MOUSEWHEEL){int x,y;SDL_GetMouseState(&x,&y);if(x>=335 && x<400 && y>=520 && y<582)dispatch({Type::Encoder,0,event.wheel.y});else dispatch({Type::Knob,uint8_t(knob),event.wheel.y*1000});}
  }
  if(strip){int x,y;SDL_GetMouseState(&x,&y);int position=(x-400)*16383/560;if(position<0)position=0;if(position>16383)position=16383;dispatch({Type::Strip,0,position});}
  uint64_t frames=audio.frames.load(std::memory_order_acquire);midi_router->service(*engine);while(midi_cursor<midi_script.size() && midi_script[midi_cursor].sample<=frames){if(!midi_router->accept(*engine,*project,midi_script[midi_cursor].packet)){++midi_rejected;std::fprintf(stderr,"MIDI script event rejected\n");}++midi_cursor;}while(replay_cursor<replay.size() && replay[replay_cursor].sample<=frames)dispatch(replay[replay_cursor++].input);
  if(readiness && frames>48000)running=false;
  if(self && frames>1000 && !self_released){dispatch({Type::Pad,12,0x20,0});dispatch({Type::Pad,13,0x20,0});self_released=true;}
  perf.animate((clock.microseconds()-start)/1000);perf.poll();session.service();auto project_action=perf.take_project_action();if(project_action!=groove::Performance::ProjectAction::None){bool ok=project_action==groove::Performance::ProjectAction::New?groove::create_empty_session(*engine,*project,perf):session.load(self?self_path:"virtual-mk3.mgxs");if(ok && project_action==groove::Performance::ProjectAction::New)session.set_bindings(nullptr);if(project_action==groove::Performance::ProjectAction::Load && !ok)perf.notify(groove::UiNotice::Failed);std::fprintf(stderr,"project action: %s\n",ok?"ok":"failed");}if(perf.take_save_request()){bool saved=session.save(perf.project_path());if(saved)perf.notify(groove::UiNotice::Saved);else if(perf.require_project_save_safe())perf.notify(groove::UiNotice::SaveFailed);if(self)self_save_done=saved;std::fprintf(stderr,"save: %s\n",saved?"ok":"failed");}if(perf.take_transfer_request()){perf.notify(groove::UiNotice::HostFileTools);}if(perf.take_update_request()){perf.notify(groove::UiNotice::HostUpdateTools);std::fprintf(stderr,"Desktop update: use tools/update/update.py and update simulator; no device write requested\n");}groove::ClockEvent tick;while(engine->pop_clock(tick)){}groove::MidiEvent packet;while(engine->pop_midi(packet))if(midi_log)std::fprintf(midi_log,"%llu,%u,%u,%u\n",(unsigned long long)packet.sample,packet.bytes[0],packet.bytes[1],packet.bytes[2]);
  if(self && frames>24000 && !self_save_queued){perf.set_mode(groove::UiMode::Project);self_save_queued=dispatch({Type::SoftKey,0});if(!self_save_queued)running=false;}
  // Follow the current PROJECT -> OPEN -> Browser path. The old double
  // CONFIRM sequence now means Browser UP and can lose the pending refresh.
  if(self && self_save_done && !self_browser_opened){
   perf.set_mode(groove::UiMode::Project);
   self_browser_opened=dispatch({Type::SoftKey,1});
   if(!self_browser_opened)running=false;
  }else if(self && self_browser_opened && !self_load_queued && !perf.file_action_pending()){
   auto& browser=perf.browser();
   for(unsigned i=0;i<browser.count;++i){
    if(browser.entries[i].directory || std::strcmp(browser.entries[i].name,groove::filename(self_path)))continue;
    browser.cursor=i;self_load_queued=dispatch({Type::SoftKey,0});break;
   }
   if(!self_load_queued){std::fprintf(stderr,"self-test saved song missing from Browser\n");running=false;}
  }
  if(self && self_load_queued && !self_roundtrip && perf.notice()==groove::UiNotice::Loaded){
   self_roundtrip=true;dispatch({Type::Play});
  }
  if(self && self_roundtrip && frames>48000 && !self_drop_sent){SDL_Event drop{};drop.type=SDL_DROPFILE;drop.drop.file=SDL_strdup("FACTORY/DEBUG/demo-slices.wav");if(SDL_PushEvent(&drop)!=1){SDL_free(drop.drop.file);running=false;}self_drop_sent=true;}
  if(readiness && frames>48000)running=false;
  if(self && self_drop_ok && frames>100000 && !capture_requested && !captured){resample=true;capture_requested=engine->request_capture();}
  if(capture_requested && engine->capture_ready()){captured=engine->copy_capture(capture.data(),48000*4);FILE* file=std::fopen("build/skip-back.wav","wb");if(file){FileWriter writer{file};groove::write_wav(writer,capture.data(),captured);std::fclose(file);}if(resample){bool ok=assign_sample("build/skip-back.wav",session);if(self)self_resampled=ok;std::fprintf(stderr,"resample: %s\n",ok?"assigned to selected track":"failed");}capture_requested=false;if(self){perf.set_mode(groove::UiMode::Sampler);perf.pad(12,0x10,3500);resampled_at=frames;}}
  if(self && resampled_at && frames>=resampled_at+4800){perf.pad(12,0x20,0);running=false;}
  const auto& theme=groove::ui::Theme::by_index(perf.visual_settings().theme);
  rect(renderer,0,0,1000,744,uint8_t(theme.chassis>>16),uint8_t(theme.chassis>>8),uint8_t(theme.chassis));ui->draw(*project,perf,unsigned(audio.peak.load()*100),unsigned(uint64_t(audio.max_us.load())*48000/(256*10000)),assets.lookup(project->assets[track_at(*project,project->active_pattern,perf.selected_track()).sample_slot].path));display.settings(perf.visual_settings());display.present(0,ui->pixels[0],480,272);display.present(1,ui->pixels[1],480,272);
  groove::ui::LedFrame lights;perf.led_frame(lights);
  for(int i=0;i<16;++i){int x=20+i%4*80,y=382+i/4*80;uint32_t color=lights.pad_rgb[i];bool bright=(lights.pads[i]&3)>=2;rounded(renderer,x-4,y-4,78,78,11,color,bright?18:4);rounded(renderer,x,y,70,70,9,groove::ui::mix(theme.surface,color,bright?185:85));rounded(renderer,x+5,y+5,60,60,7,color,bright?180:85);char name[4];std::snprintf(name,sizeof name,"%u",(3-i/4)*4+i%4+1);label(renderer,x+10,y+8,name,1,theme.text,46);}
  for(unsigned i=0;i<8;++i){unsigned side=i/4,column=i%4;int x=10+int(side)*500+int(column)*117;rounded(renderer,x,9,112,24,6,theme.surface);uint8_t code=lights.buttons[12+i];rounded(renderer,x+7,12,3,17,1,code?theme.text:theme.disabled,code==groove::mk3::mono_active?255:85);label(renderer,x+17,12,perf.soft_label(i),1,perf.soft_selected(i)?theme.teal:theme.secondary,90);rounded(renderer,x+43,325,32,32,16,i==knob?theme.teal:theme.border);rounded(renderer,x+46,328,26,26,13,theme.background);rounded(renderer,x+58,328,2,8,1,i==knob?theme.teal:theme.secondary);}
  for(unsigned i=0;i<14;++i){int x=400+int(i%7)*80,y=382+int(i/7)*40;bool on=unsigned(perf.mode())==i;rounded(renderer,x,y,74,32,6,on?groove::ui::mix(theme.surface,theme.teal,50):theme.surface);label(renderer,x+6,y+8,(i==unsigned(groove::UiMode::Modulation)?"MOD":groove::mode_name(groove::UiMode(i))),1,on?theme.teal:theme.secondary,62);}
  rounded(renderer,335,650,60,32,6,perf.mode()==groove::UiMode::Chord?groove::ui::mix(theme.surface,theme.teal,50):theme.surface);label(renderer,341,658,"CHORD",1,perf.mode()==groove::UiMode::Chord?theme.teal:theme.secondary,50);
  const char* action_names[]={"FILE","AUTO","COPY","PASTE","SELECT","ERASE","FIXED","REC"};for(unsigned i=0;i<8;++i){bool active=i==4?virtual_select:i==5?virtual_erase:i==6?lights.buttons[45]!=0:i==7?(perf.gesture_recording() || perf.note_recording()):false;int x=400+int(i)*70;rounded(renderer,x,458,64,19,5,active?groove::ui::mix(theme.surface,theme.amber,45):theme.surface);label(renderer,x+7,460,action_names[i],1,active?theme.amber:theme.secondary,60);}
  const int positions[]={400,510,620,710,800,890},widths[]={90,90,80,80,80,80};const char* names[]={perf.playing()?"STOP":"PLAY","STEP","MUTE","SOLO","ALL","WAVE"};bool active[]={perf.playing(),perf.editing(),virtual_mute,virtual_solo,perf.control_all(),perf.wave_profile()};
  for(unsigned i=0;i<6;++i){rounded(renderer,positions[i],480,widths[i],50,6,active[i]?groove::ui::mix(theme.surface,i>=2?theme.amber:theme.teal,50):theme.surface);label(renderer,positions[i]+12,493,names[i],1,active[i]?(i>=2?theme.amber:theme.teal):theme.text,widths[i]-20);rounded(renderer,positions[i]+10,520,widths[i]-20,2,1,active[i]?(i>=2?theme.amber:theme.teal):theme.border,i==0 && lights.buttons[41]==groove::mk3::mono_dim?85:255);}
  label(renderer,400,545,perf.control_all()?"SMART STRIP / CONTROL ALL":"SMART STRIP / SCENE MORPH",1,theme.secondary,560);
  for(unsigned i=0;i<25;++i)rounded(renderer,400+int(i)*23,566,6,3,1,lights.strip_level[i]?theme.blue:theme.border,lights.strip_level[i]==3?255:lights.strip_level[i]?100:100);
  rounded(renderer,400,580,560,28,6,theme.surface);rounded(renderer,405+int((perf.control_all()?perf.control_all_position():perf.morph_position())*541),584,9,20,3,theme.teal);
  rounded(renderer,347,532,36,36,18,theme.border);rounded(renderer,350,535,30,30,15,theme.surface);label(renderer,355,541,"4D",1,theme.secondary,24);
  rounded(renderer,363,522,5,4,2,lights.button_rgb[58]);rounded(renderer,337,548,4,5,2,lights.button_rgb[59]);rounded(renderer,389,548,4,5,2,lights.button_rgb[60]);rounded(renderer,363,575,5,4,2,lights.button_rgb[61]);
  label(renderer,400,624,"A-H: TRACKS 1-8 / SHIFT: 9-16",1,theme.secondary,560);
  for(unsigned i=0;i<8;++i){int x=400+int(i)*70;uint32_t color=lights.button_rgb[29+i];rounded(renderer,x,650,62,31,6,color);char group[8];std::snprintf(group,sizeof group,"%c %u",char('A'+i),unsigned((shift?8:0)+i+1));label(renderer,x+10,657,group,1,theme.text,42);}
  label(renderer,20,719,"VIRTUAL MK3 / 480 x 272 x 2",1,theme.secondary,340);label(renderer,400,700,"F9 A / F10 B / F11 STUTTER / F12 FILL",1,theme.secondary,570);label(renderer,400,719,perf.visual_settings().preview565?"16-BIT LCD PREVIEW / SETTINGS: MOTION + THEME":"ARGB PREVIEW / SETTINGS: MOTION + THEME",1,theme.secondary,580);
  if((self || readiness) && !running){SDL_Surface* image=SDL_CreateRGBSurfaceWithFormat(0,1000,744,32,SDL_PIXELFORMAT_ARGB8888);if(image){if(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,image->pixels,image->pitch)==0)SDL_SaveBMP(image,readiness?"build/virtual-readiness.bmp":"build/virtual-mk3.bmp");SDL_FreeSurface(image);}}
  // Functional smoke deadline includes sanitized project I/O on slow hosts.
  // Audio timing is reported separately; this is not a real-time performance gate.
  SDL_RenderPresent(renderer);if(self && clock.microseconds()-start>60000000){std::fprintf(stderr,"self-test timeout\n");running=false;}SDL_Delay(20);
 }
 }
 audio.stop();if(recording)std::fclose(recording);if(midi_log)std::fclose(midi_log);
 bool ok=readiness?(audio.callbacks.load()>0 && audio.peak.load()>0.0001f):!self || (audio.callbacks.load()>0 && audio.peak.load()>0.0001f && captured>0 && project->active_scene==1 && self_roundtrip && self_drop_ok && self_resampled);
 if(midi_script_path){std::printf("MIDI script: %zu/%zu events, %u rejected\n",midi_cursor,midi_script.size(),midi_rejected);if((self || readiness) && (midi_cursor!=midi_script.size() || midi_rejected))ok=false;}
 if(self || readiness)std::printf("virtual-mk3 callbacks=%u peak=%.5f scene=%u capture=%u max-callback-us=%u over-budget=%u\n",audio.callbacks.load(),double(audio.peak.load()),project->active_scene,captured,audio.max_us.load(),audio.over_budget.load());
 SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return ok?0:1;
}
