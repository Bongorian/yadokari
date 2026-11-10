// Host-only C acceptance: no content generation or device access.
#include "session_files.h"
#include "../../platform/desktop/file_access.h"
#include "../../platform/desktop/sample_assets.h"
#include <filesystem>
#include <memory>
#include <vector>
#include <cstdio>
#include <cmath>
using namespace groove;
struct ByteSink {std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
static bool check(const char* path){
 DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();PreservedOwner owner;if(!owner)return false;
 if(!read_project_file(fs,path,*p,nullptr,&*owner) || kFormatVersion!=13 || p->content_kind!=ContentKind::Song){std::fprintf(stderr,"%s: MGXC13 Song verification failed\n",path);return false;}
 ByteSink round;if(!encode_project(round,*p,&*owner) || fs.size(path)!=round.bytes.size()){std::fprintf(stderr,"%s: canonical encoding/size failed\n",path);return false;}std::vector<uint8_t> original(round.bytes.size());unsigned got=0;if(!fs.read(path,0,original.data(),unsigned(original.size()),got) || got!=original.size() || original!=round.bytes){std::fprintf(stderr,"%s: canonical byte roundtrip failed\n",path);return false;}
 std::vector<unsigned> tracks;for(unsigned t=0;t<16;++t)if(p->tracks[t].instrument!=Instrument::Empty || p->audio[t].kind!=TrackKind::Instrument)tracks.push_back(t);
 if(tracks.size()!=5 || p->song.length!=5 || !p->song.loop || !p->song.enabled){std::fprintf(stderr,"%s: structure tracks=%zu sections=%u loop=%u enabled=%u\n",path,tracks.size(),p->song.length,p->song.loop,p->song.enabled);return false;}
 const unsigned bars[]={4,8,8,8,4};uint16_t scenes=0;unsigned kit=0;for(unsigned i=0;i<5;++i){const auto& sec=p->song.sections[i];if(sec.ticks || sec.bars!=bars[i] || sec.transpose || sec.mute_mask)return false;scenes|=uint16_t(1u<<sec.scene);}
 if(__builtin_popcount(unsigned(scenes))!=3)return false;
 for(unsigned a=0;a<16;++a)if(scenes&(1u<<a))for(unsigned b=a+1;b<16;++b)if(scenes&(1u<<b)){bool different=p->scenes[a].mute_mask!=p->scenes[b].mute_mask;for(unsigned t:tracks)different|=p->scenes[a].pattern_for_track[t]!=p->scenes[b].pattern_for_track[t];if(!different){std::fprintf(stderr,"%s: referenced scenes have identical track tables\n",path);return false;}}
 for(unsigned t:tracks){const auto& state=p->tracks[t];kit+=state.instrument==Instrument::DrumKit;if(p->audio[t].kind!=TrackKind::Instrument || !state.scale_mask || state.scale_root>=12)return false;}
 if(kit!=1)return false;
 auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);auto files=std::make_unique<SessionFiles>(fs,library,*e,*p,*ui);if(!files->load(path,false,false)){std::fprintf(stderr,"%s: load: %s\n",path,ui->browser().status);return false;}
 float maintenance[2]{};e->render(maintenance,0);ui->poll();
 auto source=std::make_unique<ProjectState>(*p);WavView views[kSampleAssets]{};if(!library.prepare(*p,views)){std::fprintf(stderr,"%s: assets: %s\n",path,library.failure_reason());return false;}
 for(unsigned t:tracks){double energy=0;for(unsigned sc=0;sc<16;++sc)if((scenes&(1u<<sc)) && !(source->scenes[sc].mute_mask&(1u<<t))){*p=*source;p->song.enabled=p->chain.enabled=false;p->muted_tracks=p->solo_tracks=0;for(unsigned j=0;j<16;++j){p->clips[j].playing=j==t;p->clips[j].pattern=p->scenes[sc].pattern_for_track[j];}if(!e->stage_project(*p,views) || !e->submit({CommandType::Start,0,0,0}))return false;unsigned frames=unsigned(std::ceil(track_at(*p,p->clips[t].pattern,t).length*60.0*48000/(p->tempo*4)));if(frames>30*48000){std::fprintf(stderr,"%s: track %u exceeds 30s qualification window\n",path,t+1);return false;}float audio[256];for(unsigned at=0;at<frames;at+=128){unsigned n=frames-at<128?frames-at:128;e->render(audio,n);for(unsigned i=0;i<n*2;++i){if(!std::isfinite(audio[i]))return false;energy+=double(audio[i])*audio[i];}library.service();EngineEvent event;while(e->pop_event(event)){}ClockEvent clock;while(e->pop_clock(clock)){}MidiEvent midi;while(e->pop_midi(midi)){}AudioVisualFrame visual;while(e->pop_visual(visual)){}}}
  if(energy<1e-10){std::fprintf(stderr,"%s: track %u silent\n",path,t+1);return false;}std::printf("%s track=%u energy=%.9g key=%u mask=%03x\n",path,t+1,energy,source->tracks[t].scale_root,source->tracks[t].scale_mask);
 }
 // Combined Scene playback also catches shared Layer/voice admission failures.
 uint16_t heard=0,wanted=0;for(unsigned t:tracks)wanted|=uint16_t(1u<<t);
 for(unsigned sc=0;sc<16;++sc)if(scenes&(1u<<sc)){
  *p=*source;p->song.enabled=p->chain.enabled=false;p->muted_tracks=p->scenes[sc].mute_mask;p->solo_tracks=0;unsigned length=1;
  for(unsigned t=0;t<16;++t){p->clips[t].playing=(wanted&(1u<<t)) && !(p->scenes[sc].mute_mask&(1u<<t));p->clips[t].pattern=p->scenes[sc].pattern_for_track[t];if(p->clips[t].playing && track_at(*p,p->clips[t].pattern,t).length>length)length=track_at(*p,p->clips[t].pattern,t).length;}
  unsigned frames=unsigned(std::ceil(length*60.0*48000/(p->tempo*4)));if(frames>30*48000 || !e->stage_project(*p,views) || !e->submit({CommandType::Start,0,0,0}))return false;
  float audio[256];for(unsigned at=0;at<frames;at+=128){unsigned n=frames-at<128?frames-at:128;e->render(audio,n);for(unsigned i=0;i<n*2;++i)if(!std::isfinite(audio[i]))return false;library.service();EngineEvent event;while(e->pop_event(event)){}ClockEvent clock;while(e->pop_clock(clock)){}MidiEvent midi;while(e->pop_midi(midi)){}AudioVisualFrame visual;while(e->pop_visual(visual))for(unsigned t:tracks)if(visual.peak[t]>1e-7f)heard|=uint16_t(1u<<t);}
 }
 if((heard&wanted)!=wanted){std::fprintf(stderr,"%s: combined Scene playback missing tracks mask=%04x\n",path,unsigned(wanted&~heard));return false;}
 std::printf("%s: VERIFY PASS / MGXC13 BYTES IDENTICAL / FIVE SOUNDING TRACKS / THREE SCENES / 4-8-8-8-4 LOOP / TEMPO %u\n",path,source->tempo);return true;
}
int main(int argc,char** argv){bool one=argc==4 && !__builtin_strcmp(argv[1],"--one");if(!one && argc!=5){std::fprintf(stderr,"usage: check-templates DATA_ROOT SONG1 SONG2 SONG3\nGenre / musical roles / thin-full-break intent and listening remain content review. No files are written.\n");return 2;}std::filesystem::current_path(argv[one?2:1]);bool ok=true;for(unsigned i=one?3:2;i<unsigned(argc);++i){bool passed=check(argv[i]);if(!passed)std::fprintf(stderr,"%s: ACCEPTANCE FAILED\n",argv[i]);ok&=passed;}return ok?0:1;}
