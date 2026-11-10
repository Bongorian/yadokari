// MGXC project codec tests (docs/project-format.md). The legacy MGX*
// positional formats and v1 are retired; no migration or compatibility reader.
#include "project_codec.h"
#include <vector>
#include <memory>
#include <cassert>
#include <cstdio>
#include <cstring>
struct Sink { std::vector<uint8_t> bytes; bool write(const uint8_t* p,size_t n) { bytes.insert(bytes.end(),p,p+n); return true; } };
struct Source { const std::vector<uint8_t>& bytes; size_t pos=0; bool read(uint8_t* p,size_t n) { if(n>bytes.size()-pos) return false; for(size_t i=0;i<n;++i) p[i]=bytes[pos++]; return true; } bool at_end() const { return pos==bytes.size(); } };

static uint32_t kind4(const char* s){return uint32_t(uint8_t(s[0]))|(uint32_t(uint8_t(s[1]))<<8)|(uint32_t(uint8_t(s[2]))<<16)|(uint32_t(uint8_t(s[3]))<<24);}
struct TChunk{uint32_t kind=0;uint16_t version=1;std::vector<uint8_t> payload;};
static std::vector<TChunk> parse_chunks(const uint8_t* p,size_t n){
 std::vector<TChunk> out;size_t at=0;
 while(at<n){assert(at+12<=n);TChunk c;c.kind=uint32_t(p[at])|(uint32_t(p[at+1])<<8)|(uint32_t(p[at+2])<<16)|(uint32_t(p[at+3])<<24);c.version=uint16_t(p[at+4]|(p[at+5]<<8));uint32_t len=uint32_t(p[at+8])|(uint32_t(p[at+9])<<8)|(uint32_t(p[at+10])<<16)|(uint32_t(p[at+11])<<24);assert(at+12+len<=n);c.payload.assign(p+at+12,p+at+12+len);out.push_back(std::move(c));at+=12+len;}
 assert(at==n);return out;
}
static void append_chunks(std::vector<uint8_t>& out,const std::vector<TChunk>& chunks){
 for(const auto& c:chunks){out.push_back(uint8_t(c.kind));out.push_back(uint8_t(c.kind>>8));out.push_back(uint8_t(c.kind>>16));out.push_back(uint8_t(c.kind>>24));out.push_back(uint8_t(c.version));out.push_back(uint8_t(c.version>>8));out.push_back(0);out.push_back(0);uint32_t len=uint32_t(c.payload.size());out.push_back(uint8_t(len));out.push_back(uint8_t(len>>8));out.push_back(uint8_t(len>>16));out.push_back(uint8_t(len>>24));out.insert(out.end(),c.payload.begin(),c.payload.end());}
}
static std::vector<uint8_t> make_file(const std::vector<TChunk>& chunks){
 std::vector<uint8_t> stream;append_chunks(stream,chunks);
 std::vector<uint8_t> file={'M','G','X','C',uint8_t(groove::kFormatVersion),0};
 uint32_t len=uint32_t(stream.size());file.push_back(uint8_t(len));file.push_back(uint8_t(len>>8));file.push_back(uint8_t(len>>16));file.push_back(uint8_t(len>>24));
 file.insert(file.end(),stream.begin(),stream.end());
 groove::ProjectCrc crc;for(size_t i=4;i<file.size();++i)crc.add(file[i]);uint32_t sum=crc.value();
 file.push_back(uint8_t(sum));file.push_back(uint8_t(sum>>8));file.push_back(uint8_t(sum>>16));file.push_back(uint8_t(sum>>24));
 return file;
}
static std::vector<TChunk> parse_file(const std::vector<uint8_t>& file){
 assert(file.size()>14 && file[0]=='M' && file[1]=='G' && file[2]=='X' && file[3]=='C');
 return parse_chunks(file.data()+10,file.size()-14);
}
// Copy out the ENGN sub-chunk of one track inside the persistent TRKS chunk payload.
static TChunk find_track_engn(TChunk& ptrn,uint8_t track_index){
 // TRKS contains the sixteen persistent TRCK records.
 auto subs=parse_chunks(ptrn.payload.data(),ptrn.payload.size());
 for(auto& trck:subs)if(trck.kind==kind4("TRCK") && !trck.payload.empty() && trck.payload[0]==track_index){
  auto parts=parse_chunks(trck.payload.data()+1,trck.payload.size()-1);
  for(auto& part:parts)if(part.kind==kind4("ENGN"))return part;
 }
 assert(false);return {};
}
static void rebuild_track_engn(TChunk& ptrn,uint8_t track_index,const TChunk& new_engn){
 auto subs=parse_chunks(ptrn.payload.data(),ptrn.payload.size());
 for(auto& trck:subs)if(trck.kind==kind4("TRCK") && !trck.payload.empty() && trck.payload[0]==track_index){
  auto parts=parse_chunks(trck.payload.data()+1,trck.payload.size()-1);
  for(auto& part:parts)if(part.kind==kind4("ENGN"))part=new_engn;
  std::vector<uint8_t> tp{track_index};append_chunks(tp,parts);trck.payload=tp;
 }
 std::vector<uint8_t> pp;append_chunks(pp,subs);ptrn.payload=pp;
}
static std::vector<uint8_t> read_file_bytes(const char* path){
 FILE* f=std::fopen(path,"rb");assert(f);assert(std::fseek(f,0,SEEK_END)==0);std::vector<uint8_t> bytes(size_t(std::ftell(f)));std::rewind(f);assert(std::fread(bytes.data(),1,bytes.size(),f)==bytes.size());assert(std::fclose(f)==0);return bytes;
}

int main() {
 // The final save buffer is also a checked, borrowed decoder source.
 // Streaming and borrowed paths must reproduce bytes and reject identical
 // CRC damage/truncation; a failed slice must never fall back to reads.
 {
  auto p=std::make_unique<groove::ProjectState>();groove::make_empty_project(*p);
  groove::ChunkBuffer encoded;assert(groove::encode_project_buffer(encoded,*p));
  Sink stream;assert(groove::encode_project(stream,*p));
  assert(stream.bytes.size()==encoded.size && !std::memcmp(stream.bytes.data(),encoded.data,encoded.size));
  groove::ChunkSource view{encoded.data,encoded.size,0};auto restored=std::make_unique<groove::ProjectState>();
  assert(groove::decode_project(view,*restored) && view.at_end());
  encoded.data[encoded.size-1]^=1;view.pos=0;assert(!groove::decode_project(view,*restored));encoded.data[encoded.size-1]^=1;
  view={encoded.data,encoded.size-5,0};assert(!groove::decode_project(view,*restored));
  struct FailedView {
   groove::ChunkSource source;unsigned slices=0;
   bool read(uint8_t* p,size_t n){assert(!slices);return source.read(p,n);}
   bool slice(uint32_t,groove::ChunkSource&){++slices;return false;}
   bool at_end()const{return source.at_end();}
  } rejected{{encoded.data,encoded.size,0}};
  assert(!groove::decode_project(rejected,*restored) && rejected.slices==1);
 }
 groove::ProjectCrc standard_crc;for(char c : "123456789")if(c)standard_crc.add(uint8_t(c));assert(standard_crc.value()==0xcbf43926u);
 auto in=std::make_unique<groove::ProjectState>();in->midi_input_route[0]=16;in->muted_tracks=1;in->solo_tracks=2;groove::make_empty_project(*in);assert(in->midi_input_route[0]==15 && !in->muted_tracks && !in->solo_tracks); track_at(*in,4,7).arp_mode=3;track_at(*in,4,7).arp_rate=2;track_at(*in,4,7).arp_octaves=3;in->effects.compressor[0]=0.2f;in->effects.compressor[1]=8;in->effects.compressor[5]=0.7f;in->effects.compressor[6]=3;in->morph_b[2].values[groove::morph_index(71)]=11;track_at(*in,4,7).compressor[0]=0.12f;track_at(*in,4,7).compressor[5]=0.8f;in->morph_a[7].values[groove::morph_index(88)]=9;in->midi_input_route[1]=3;in->midi_input_route[5]=16;in->muted_tracks=0x0082;in->solo_tracks=0x0400;in->clips[7].pattern=4;in->tempo=137;in->ui.chord_shape=5;in->ui.chord_inversion=2;in->ui.arp_direction=2;in->ui.arp_octaves=3;in->ui.repeat_gate=55;in->ui.repeat_rate=6;in->ui.note_quantized=false;in->ui.note_replace=true;in->ui.metronome=true;in->ui.count_in=true;in->ui.click_main=true;track_at(*in,4,7).reverb_send=0.43f;in->morph_a[0].values[groove::morph_index(10)]=0.21f;in->effects.reverb_mix=0.7f;in->effects.reverb_decay=0.8f;in->effects.reverb_damp=0.3f;in->morph_a[0].values[groove::morph_index(67)]=0.6f; track_at(*in,4,7).steps[22].enabled=true; track_at(*in,4,7).steps[22].microtiming=-9; track_at(*in,4,7).steps[22].locks[1]={groove::engine_target(groove::Instrument::PlaitsMacro,1),0.375f,true}; in->scenes[2].pattern_for_track[7]=4;
 in->routing.groups[2].members=0x0081;in->routing.groups[2].gain=0.4f;in->routing.groups[2].output=2;in->assets[3].path[0]='x'; track_at(*in,4,7).sample_slot=3;track_at(*in,4,7).speed_num=3;track_at(*in,4,7).transpose=-12;track_at(*in,4,7).scale_mask=0x06bd;track_at(*in,4,7).scale_root=2;track_at(*in,4,7).steps[22].condition=groove::TrigCondition::Cycle;track_at(*in,4,7).steps[22].cycle_every=3;track_at(*in,4,7).steps[22].cycle_phase=2;track_at(*in,4,7).steps[22].locks[2]={149,64,true};track_at(*in,4,7).steps[22].chord[0]=64;track_at(*in,4,7).steps[22].chord[1]=67;in->chain.bars=2;in->morph_enabled=true;in->morph_b[7].values[2]=0.1f;
 auto advanced=track_at(*in,4,7);advanced.accumulator.pitch=-3;advanced.accumulator.mode=groove::AccumulatorMode::Bounce;advanced.accumulator.velocity=4;advanced.accumulator.scale_degrees=true;advanced.groove.preset=4;advanced.groove.amount=70;advanced.groove.timing[3]=-24;advanced.lanes_enabled=true;advanced.lanes[2].duration[3]=4;in->chain.mode=groove::FollowMode::NoRepeat;in->chain.probability=75;
 in->ui.wave_profile_mask=0x8101;in->ui.wave_clock_mask=0x8001;track_at(*in,4,7).midi_program=74;track_at(*in,4,7).pitch_bend=10000;track_at(*in,4,7).pressure=77;track_at(*in,4,7).expression_used=3;in->morph_b[7].values[groove::morph_index(260)]=12000;track_at(*in,4,7).nrpn[2]={1234,8193,true,true};in->morph_b[7].values[groove::morph_index(258)]=10000;track_at(*in,4,7).automation[0].values[63]=0.73f;track_at(*in,4,7).automation[0].values[255]=0.91f;track_at(*in,4,7).mutation.seed=0x12345678;track_at(*in,4,7).mutation.amount=63;track_at(*in,4,7).mutation.frozen=true;track_at(*in,4,7).mutation_source[3].note=71;track_at(*in,4,7).length=256;track_at(*in,4,7).steps[255].enabled=true;track_at(*in,4,7).steps[255].note=83;track_at(*in,4,7).steps[255].locks[2]={3,0.5f,true};track_at(*in,4,7).mutation_source[255].note=77;in->chain.follow[2].bars=7;in->chain.follow[2].probability=35;in->chain.follow[2].weight=9;in->chain.follow[2].condition=groove::FollowCondition::Fill;Sink sink; assert(groove::encode_project(sink,*in)); auto out=std::make_unique<groove::ProjectState>(); Source source{sink.bytes}; assert(groove::decode_project(source,*out)); assert(out->tempo==137 && track_at(*out,4,7).steps[22].enabled && track_at(*out,4,7).steps[22].microtiming==-9 && track_at(*out,4,7).steps[22].locks[1].value==0.375f && out->scenes[2].pattern_for_track[7]==4);
 assert(track_at(*out,4,7).sample_slot==3 && out->assets[3].path[0]=='x' && track_at(*out,4,7).speed_num==3 && track_at(*out,4,7).transpose==-12 && track_at(*out,4,7).scale_mask==0x06bd && track_at(*out,4,7).scale_root==2 && track_at(*out,4,7).steps[22].cycle_phase==2 && out->morph_b[7].values[2]==0.1f && out->chain.bars==2 && track_at(*out,4,7).steps[22].locks[2].id==149 && track_at(*out,4,7).steps[22].locks[2].value==64);
 assert(track_at(*out,4,7).steps[22].chord[0]==64 && track_at(*out,4,7).steps[22].chord[1]==67 && track_at(*out,4,7).steps[22].chord[2]==255);assert(out->routing.groups[2].members==0x0081 && out->routing.groups[2].gain==0.4f && out->routing.groups[2].output==2);assert(track_at(*out,4,7).reverb_send==0.43f && out->morph_a[0].values[groove::morph_index(10)]==0.21f);assert(!out->ui.note_quantized && out->ui.note_replace && out->ui.metronome && out->ui.count_in && out->ui.click_main);assert(out->effects.reverb_mix==0.7f && out->effects.reverb_decay==0.8f && out->effects.reverb_damp==0.3f && out->morph_a[0].values[groove::morph_index(67)]==0.6f);assert(sink.bytes[0]=='M' && sink.bytes[1]=='G' && sink.bytes[2]=='X' && sink.bytes[3]=='C' && out->ui.wave_profile_mask==0x8101 && out->ui.wave_clock_mask==0x8001 && track_at(*out,4,7).midi_program==74 && out->ui.chord_shape==5 && out->ui.chord_inversion==2 && track_at(*out,4,7).automation[0].values[63]==0.73f && track_at(*out,4,7).nrpn[2].number==1234 && track_at(*out,4,7).nrpn[2].value==8193 && track_at(*out,4,7).nrpn[2].enabled && track_at(*out,4,7).nrpn[2].registered && out->morph_b[7].values[groove::morph_index(258)]==10000);assert(out->midi_input_route[1]==3 && out->midi_input_route[5]==16);assert(out->muted_tracks==0x0082 && out->solo_tracks==0x0400);assert(track_at(*out,4,7).compressor[0]==0.12f && track_at(*out,4,7).compressor[5]==0.8f && out->morph_a[7].values[groove::morph_index(88)]==9);assert(out->effects.compressor[0]==0.2f && out->effects.compressor[1]==8 && out->effects.compressor[5]==0.7f && out->effects.compressor[6]==3 && out->morph_b[2].values[groove::morph_index(71)]==11);assert(track_at(*out,4,7).arp_mode==3 && track_at(*out,4,7).arp_rate==2 && track_at(*out,4,7).arp_octaves==3);assert(out->ui.arp_direction==2 && out->ui.arp_octaves==3 && out->ui.repeat_gate==55 && out->ui.repeat_rate==6);assert(track_at(*out,4,7).mutation.seed==0x12345678 && track_at(*out,4,7).mutation.amount==63 && track_at(*out,4,7).mutation.frozen && track_at(*out,4,7).mutation_source[3].note==71);assert(out->chain.follow[2].bars==7 && out->chain.follow[2].probability==35 && out->chain.follow[2].weight==9 && out->chain.follow[2].condition==groove::FollowCondition::Fill);const auto restored=track_at(*out,4,7);assert(restored.accumulator.pitch==-3 && restored.accumulator.scale_degrees && restored.groove.timing[3]==-24 && restored.lanes_enabled && restored.lanes[2].duration[3]==4 && out->chain.mode==groove::FollowMode::NoRepeat && out->chain.probability==75);
 assert(track_at(*out,4,7).automation[0].values[255]==0.91f);assert(track_at(*out,4,7).length==256 && track_at(*out,4,7).steps[255].enabled && track_at(*out,4,7).steps[255].note==83 && track_at(*out,4,7).steps[255].locks[2].value==0.5f && track_at(*out,4,7).mutation_source[255].note==77);assert(track_at(*out,4,7).pitch_bend==10000 && track_at(*out,4,7).pressure==77 && track_at(*out,4,7).expression_used==3 && out->morph_b[7].values[groove::morph_index(260)]==12000);
 // Round-trip byte identity: re-encoding the decoded project (empty
 // store on both sides) reproduces the exact image.
 {Sink again;assert(groove::encode_project(again,*out));assert(again.bytes==sink.bytes);}
 // Corruption, truncation and the retired legacy magic are rejected.
 {std::vector<uint8_t> damaged=sink.bytes;damaged[100]^=1;Source s{damaged};assert(!groove::decode_project(s,*out));}
 {std::vector<uint8_t> truncated=sink.bytes;truncated.pop_back();Source s{truncated};assert(!groove::decode_project(s,*out));}
 {std::vector<uint8_t> legacy_magic=sink.bytes;legacy_magic[3]='Z';Source s{legacy_magic};assert(!groove::decode_project(s,*out));}
 std::printf("project codec: %zu bytes, MGXC roundtrip + CRC + rejection verified\n",sink.bytes.size());

 // Unknown engine GUID: the track falls back to Instrument::Unknown, the
 // project still opens, and the payload round-trips verbatim via the
 // preserved store.
 {
  auto chunks=parse_file(sink.bytes);
  TChunk* ptrn=nullptr;for(auto& c:chunks)if(c.kind==kind4("TRKS"))ptrn=&c;assert(ptrn);
  TChunk engn=find_track_engn(*ptrn,0);
  // Rebuild the ENGN payload with an unregistered GUID, keeping the
  // schema version and sub-chunk bytes.
  unsigned glen=engn.payload[0];assert(glen>0 && 1+glen+2<=engn.payload.size());
  std::vector<uint8_t> payload;const char* future="mk3.future-synth";payload.push_back(uint8_t(std::strlen(future)));payload.insert(payload.end(),future,future+std::strlen(future));payload.insert(payload.end(),engn.payload.begin()+1+glen,engn.payload.end());
  TChunk modified=engn;modified.payload=payload;rebuild_track_engn(*ptrn,0,modified);
  std::vector<uint8_t> file=make_file(chunks);
  auto decoded=std::make_unique<groove::ProjectState>();Source src{file};assert(groove::decode_project(src,*decoded));
  assert(track_at(*decoded,0,0).instrument==groove::Instrument::Unknown);
  assert(decoded->tempo==137 && track_at(*decoded,4,7).steps[22].enabled);
  groove::PreservedStore store;auto decoded2=std::make_unique<groove::ProjectState>();Source src2{file};assert(groove::decode_project(src2,*decoded2,&store));
  assert(track_at(*decoded2,0,0).instrument==groove::Instrument::Unknown && !store.tracks[0].engn.empty());
  Sink resaved;assert(groove::encode_project(resaved,*decoded2,&store));
  auto chunks2=parse_file(resaved.bytes);TChunk* ptrn2=nullptr;for(auto& c:chunks2)if(c.kind==kind4("TRKS"))ptrn2=&c;assert(ptrn2);
  TChunk engn2=find_track_engn(*ptrn2,0);assert(engn2.payload==payload);
  auto decoded3=std::make_unique<groove::ProjectState>();Source src3{resaved.bytes};groove::PreservedStore store3;assert(groove::decode_project(src3,*decoded3,&store3));
  assert(track_at(*decoded3,0,0).instrument==groove::Instrument::Unknown && store3.tracks[0].engn.size==store.tracks[0].engn.size);
  std::puts("unknown engine GUID fallback + verbatim preservation verified");
 }
 // Unknown top-level chunk: preserved and re-emitted exactly once.
 {
  auto chunks=parse_file(sink.bytes);
  chunks.push_back(TChunk{kind4("XTRA"),1,{'h','i'}});
  std::vector<uint8_t> file=make_file(chunks);
  groove::PreservedStore store;auto decoded=std::make_unique<groove::ProjectState>();Source src{file};assert(groove::decode_project(src,*decoded,&store));
  assert(store.chunk_count==1 && store.chunks[0].kind==kind4("XTRA") && store.chunks[0].payload.size==2);
  Sink resaved;assert(groove::encode_project(resaved,*decoded,&store));
  auto chunks2=parse_file(resaved.bytes);unsigned seen=0;for(auto& c:chunks2)if(c.kind==kind4("XTRA")){++seen;assert(c.payload.size()==2 && c.payload[0]=='h');}assert(seen==1);
  std::puts("unknown top-level chunk preservation verified");
 }
 // A known engine whose payload fails validation falls back instead of
 // rejecting the whole project.
 {
  auto chunks=parse_file(sink.bytes);
  TChunk* ptrn=nullptr;for(auto& c:chunks)if(c.kind==kind4("TRKS"))ptrn=&c;assert(ptrn);
  TChunk engn=find_track_engn(*ptrn,7);
  auto subs=parse_chunks(engn.payload.data()+1+engn.payload[0]+2,engn.payload.size()-(1+engn.payload[0]+2));
  for(auto& sub:subs)if(sub.kind==kind4("PARM")){assert(sub.payload.size()>=4);sub.payload[0]=0xe7;sub.payload[1]=0x03;sub.payload[2]=0;sub.payload[3]=0;} // plaits_model = 999
  std::vector<uint8_t> ep(engn.payload.begin(),engn.payload.begin()+1+engn.payload[0]+2);append_chunks(ep,subs);
  TChunk modified=engn;modified.payload=ep;rebuild_track_engn(*ptrn,7,modified);
  std::vector<uint8_t> file=make_file(chunks);
  groove::PreservedStore store;auto decoded=std::make_unique<groove::ProjectState>();Source src{file};assert(groove::decode_project(src,*decoded,&store));
  assert(track_at(*decoded,4,7).instrument==groove::Instrument::Unknown && !store.tracks[7].engn.empty());
  assert(decoded->tempo==137);
  std::puts("invalid engine payload fallback verified");
 }
 // Internal save validation must reject invalid serialized values even
 // when the reusable snapshot contains valid values and the CRC is correct.
 {auto chunks=parse_file(sink.bytes);bool found=false;
  for(auto& chunk:chunks)if(chunk.kind==kind4("PROJ")){assert(chunk.payload.size()>2);chunk.payload[0]=0;chunk.payload[1]=0;found=true;}
  assert(found);auto invalid=make_file(chunks);auto scratch=std::make_unique<groove::ProjectState>(*in);
  Source src{invalid};assert(!groove::validate_snapshot_encoding(src,*scratch));
  Source valid{sink.bytes};assert(groove::validate_snapshot_encoding(valid,*scratch));assert(scratch->tempo==137);
  std::puts("snapshot validation rejects invalid values with a valid CRC, then reuses workspace");
 }
 // v1 is rejected even with its CRC recomputed. PREF never permits cycles.
 {auto old=sink.bytes;old[4]=1;groove::ProjectCrc crc;for(size_t i=4;i<old.size()-4;++i)crc.add(old[i]);for(unsigned i=0;i<4;++i)old[old.size()-4+i]=uint8_t(crc.value()>>(8*i));Source src{old};assert(!groove::decode_project(src,*out));}
 {auto chunks=parse_file(sink.bytes);bool found=false;for(auto& c:chunks)if(c.kind==kind4("PREF")){c.payload[1]=c.payload[0];found=true;break;}assert(found);auto bad=make_file(chunks);Source src{bad};assert(!groove::decode_project(src,*out));}
 // Legacy fixtures are rejected cleanly.
 {auto legacy=read_file_bytes("tests/fixtures/legacy-v9.mgx");Source s{legacy};assert(!groove::decode_project(s,*out));std::puts("legacy image rejection verified");}
 // Old default.mgx was retired; generated current demos have dedicated roundtrips.

 track_at(*in,4,7).automation[0].length=256;Sink long_lane;assert(groove::encode_project(long_lane,*in));Source long_source{long_lane.bytes};assert(groove::decode_project(long_source,*out));assert(track_at(*out,4,7).automation[0].length==256 && track_at(*out,4,7).automation[0].values[255]==0.91f);
}
