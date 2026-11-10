// Offline power-loss fixture. Never opens a device or changes factory content.
#include "project_codec.h"
#include <cstdio>
#include <memory>
#include <vector>
#include <cstring>
struct Bytes {
 std::vector<uint8_t> data;
 bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}
};
struct Reader {
 const std::vector<uint8_t>& data;size_t at=0;
 bool read(uint8_t* p,size_t n){if(n>data.size()-at)return false;std::memcpy(p,data.data()+at,n);at+=n;return true;}
 bool at_end()const{return at==data.size();}
};
int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: recovery-song NEW_OUTPUT.mgxs\n");return 2;}
 auto p=std::make_unique<groove::ProjectState>();groove::make_empty_project(*p);
 p->tempo=120;p->random_seed=0x52454331;p->routing.main=.25f;p->routing.cue=0;
 for(auto& clip:p->clips)clip.playing=false;
 auto t=groove::track_at(*p,0,0);t.instrument=groove::Instrument::FM;t.length=32;t.level=.2f;t.root_note=60;
 std::snprintf(t.track_name,sizeof t.track_name,"Recovery FM");p->clips[0].playing=true;
 const unsigned notes[]={60,64,67,72,67,64,62,60};
 for(unsigned i=0;i<8;++i){auto& step=t.steps[i*4];step.enabled=true;step.note=notes[i];step.velocity=90;step.gate=48;}
 p->song.length=1;p->song.enabled=true;p->song.loop=true;
 auto& section=p->song.sections[0];section.bars=2;section.ticks=2*groove::kSongTicksPerBar;section.playing_mask=section.restart_mask=1;
 groove::reset_track_morph(*p,0);
 if(!groove::valid_project(*p))return 1;
 Bytes bytes;if(!groove::encode_project(bytes,*p))return 1;
 auto decoded=std::make_unique<groove::ProjectState>();Reader source{bytes.data};const unsigned version=groove::kFormatVersion;
 if(!groove::decode_project(source,*decoded) || version!=13 || decoded->content_kind!=groove::ContentKind::Song)return 1;
 for(const auto& sound:decoded->sounds)if(sound.instrument!=groove::Instrument::Empty)return 1;
 for(const auto& asset:decoded->assets)if(asset.path[0])return 1;
 Bytes canonical;if(!groove::encode_project(canonical,*decoded) || canonical.data!=bytes.data)return 1;
 FILE* f=std::fopen(argv[1],"wbx");if(!f){std::perror(argv[1]);return 1;}
 bool ok=std::fwrite(bytes.data.data(),1,bytes.data.size(),f)==bytes.data.size();if(std::fclose(f))ok=false;
 std::printf("Recovery Song: MGXC%u, 120BPM, 32 steps, 8 FM notes, 0 assets, 0 pool definitions, canonical byte roundtrip; %zu bytes\n",version,bytes.data.size());return ok?0:1;
}
