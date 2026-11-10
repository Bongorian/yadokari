#include "project_load_slot.h"
#include "dx7_bank.h"
#include "wav_writer.h"
#include "midi_import.h"
#include "soundfont.h"
#include "soundfont_fixture.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace groove;
struct Bytes final:IFileAccess {
 std::vector<uint8_t> data;uint64_t reported=0;unsigned reads=0;
 unsigned list(const char*,FileEntry*,unsigned)override{return 0;}
 bool read(const char*,uint64_t at,void* dst,unsigned count,unsigned& got)override{got=0;if(++reads>2048 || at>data.size()){return false;}got=unsigned(std::min<uint64_t>(count,data.size()-at));if(got){std::memcpy(dst,data.data()+at,got);}return true;}
 bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}
 bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 uint64_t size(const char*)override{return reported?reported:data.size();}bool exists(const char*)override{return true;}
};
static uint32_t seed=0x52454c31;static uint32_t random_value(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void mutate(std::vector<uint8_t>& bytes,unsigned i){if(i%4==0)bytes.resize(random_value()%(bytes.size()+1));else if(!bytes.empty())bytes[random_value()%bytes.size()]^=uint8_t(1u<<(random_value()%8));}
int main(){
 auto original=std::make_unique<ProjectState>();make_empty_project(*original);original->tempo=137;original->tracks[0].instrument=Instrument::ExternalMidi;
 auto candidate=std::make_unique<ProjectState>();ChunkBuffer encoded;assert(encode_project_buffer(encoded,*original));unsigned rejected=0;
 for(unsigned i=0;i<64;++i){std::vector<uint8_t> bytes(encoded.data,encoded.data+encoded.size);mutate(bytes,i);*candidate=*original;ChunkSource input{bytes.data(),unsigned(bytes.size()),0};bool ok=decode_project_impl(input,nullptr,nullptr,candidate.get());if(ok)assert(valid_project(*candidate));else ++rejected;}
 // Public decoding retains a live destination, unlike a private scratch parse.
 candidate->tempo=191;std::vector<uint8_t> broken(encoded.data,encoded.data+encoded.size);broken.back()^=1;ChunkSource bad{broken.data(),unsigned(broken.size()),0};assert(!decode_project(bad,*candidate) && candidate->tempo==191);
 Bytes file;file.data.assign(encoded.data,encoded.data+encoded.size);ProjectDecodeSlot worker;AsyncProjectRead reader(file,worker);
 file.reported=uint64_t(kMaxFormatBytes)+15;assert(!reader.start("huge.mgxs") && !file.reads && !worker.busy());file.reported=0;file.data[4]^=1;assert(!reader.start("obsolete.mgxs") && !reader.busy());file.data[4]^=1;file.data.pop_back();assert(!reader.start("cut.mgxs"));
 std::vector<uint8_t> wav(44+128);assert(wav_header(wav.data(),32,48000));unsigned wav_ok=0;
 for(unsigned i=0;i<64;++i){auto b=wav;mutate(b,i);WavView view{};view.frames=17;if(parse_wav(b.data(),b.size(),view)){++wav_ok;for(unsigned n=0;n<view.frames;++n)for(unsigned c=0;c<view.channels;++c)assert(std::isfinite(wav_sample(view,n,c)));}else assert(view.frames==17);}
 std::vector<uint8_t> syx(4104);syx[0]=0xf0;syx[1]=0x43;syx[3]=9;syx[4]=0x20;syx.back()=0xf7;unsigned syx_ok=0;
 for(unsigned i=0;i<128;++i){auto b=syx;mutate(b,i);Dx7BankView bank{};if(parse_dx7_bulk(b.data(),b.size(),bank)){++syx_ok;assert(bank.data==b.data()+6);}else assert(!bank.data);}
 const uint8_t midi[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,0x90,60,100,24,0x80,60,0,0,0xff,0x2f,0};auto track=std::make_unique<TrackState>(track_at(*original,0,0));unsigned midi_ok=0;
 for(unsigned i=0;i<64;++i){file.data.assign(midi,midi+sizeof midi);mutate(file.data,i);file.reads=0;MidiImport importer(file);if(importer.start("input.mid",*original,0,0)){unsigned budget=0;while(importer.busy()){importer.service();assert(++budget<=4096);}midi_ok+=importer.done();}assert(same_track_snapshot(*track,track_at(*original,0,0)));}
 size_t resident=soundfont_resident_bytes();unsigned sf_ok=0;
 for(unsigned i=0;i<32;++i){file.data.assign(kSoundFontFixture,kSoundFontFixture+sizeof kSoundFontFixture);mutate(file.data,i);file.reads=0;{SoundFontCache cache(8u<<20);WavView out{};out.frames=17;if(cache.load(file,"input.sf2",out)){++sf_ok;unsigned bank,program;assert(soundfont_first(out.soundfont,bank,program));SoundFontState state;state.values[0]=float(bank);state.values[1]=float(program);SoundFontVoice voice;voice.init(0,48000);assert(voice.trigger(out.soundfont,440,100,state));for(unsigned n=0;n<256;++n){float l,r;voice.next(440,true,l,r);assert(std::isfinite(l) && std::isfinite(r));}voice.stop();}else assert(out.frames==17);}assert(soundfont_resident_bytes()==resident);}
 // A valid RIFF header for a bank beyond budget must not scan its payload.
 file.data.assign(kSoundFontFixture,kSoundFontFixture+sizeof kSoundFontFixture);file.reported=uint64_t(1)<<30;uint32_t riff=uint32_t(file.reported-8);for(unsigned n=0;n<4;++n)file.data[4+n]=uint8_t(riff>>(8*n));file.reads=0;{SoundFontCache cache;WavView out;assert(!cache.load(file,"too-large.sf2",out) && file.reads==1);}
 std::printf("Content boundaries seed 0x52454c31: MGXC64 rejected %u; WAV64 accepted %u; SYX128 accepted %u; SMF64 accepted %u; SF2x32 accepted %u; early size/header rejection and destination/resource preservation passed\n",rejected,wav_ok,syx_ok,midi_ok,sf_ok);
}
