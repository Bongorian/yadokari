#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <vector>

struct Sink {
 std::vector<uint8_t> bytes;
 bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}
};
struct Source {
 const std::vector<uint8_t>& bytes;size_t pos=0;
 bool read(uint8_t* p,size_t n){if(n>bytes.size()-pos)return false;std::memcpy(p,bytes.data()+pos,n);pos+=n;return true;}
 bool at_end()const{return pos==bytes.size();}
};
struct Work {
 const std::vector<uint8_t>& file;
 groove::ProjectState& decoded;
 groove::ProjectState& reordered;
 const groove::ChunkBuffer& trck;
 const groove::ChunkBuffer& unknown;
 const groove::ChunkBuffer& phrase;
};
static groove::ChunkSource source_of(const groove::ChunkBuffer& buffer){
 groove::ChunkSource source;source.data=buffer.data;source.size=buffer.size;return source;
}
static void* run_on_target_stack(void* arg){
 using namespace groove;
 auto& work=*static_cast<Work*>(arg);
 Source file{work.file};assert(decode_project(file,work.decoded));
 Source validation{work.file};assert(validate_snapshot_encoding(validation,work.decoded));
 const auto decoded=track_at(work.decoded,0,0);
 assert(decoded.instrument==Instrument::DrumKit && decoded.length==256);
 assert(decoded.kit.pads[3].sample[0]==5 && decoded.kit.steps[3][255].enabled);
 assert(decoded.steps[255].note==83 && decoded.mutation_source[255].note==77 && decoded.steps[255].attributes==3 && decoded.mutation_source[255].slide());
 uint16_t seen=0;auto reordered=source_of(work.trck);assert(parse_trck(reordered,work.reordered,nullptr,seen));seen=0;auto phrase=source_of(work.phrase);assert(parse_phrs(phrase,work.reordered,0,seen,nullptr));
 auto track=track_at(work.reordered,0,0);
 assert(track.instrument==Instrument::DrumKit && track.kit.pads[3].sample[0]==5);
 assert(track.kit.steps[3][255].enabled && track.steps[255].note==83);
 // Unknown-engine fallback used to allocate another whole TrackState.
 auto unknown=source_of(work.unknown);assert(parse_track_engn(unknown,track,nullptr,nullptr));
 assert(track.instrument==Instrument::Unknown && track.kit.pads[3].sample[0]==255);
 assert(track.kit.steps[3][255].enabled && track.kit.steps[3][255].ratchet==1); // Engine fallback preserves the phrase.
 assert(track.steps[255].note==83 && track.steps[255].attributes==3 && track.length==256 && track.root_note==65);
 return nullptr;
}
int main(){
 using namespace groove;
 auto project=std::make_unique<ProjectState>();
 auto decoded=std::make_unique<ProjectState>();
 auto reordered=std::make_unique<ProjectState>();
 auto track=track_at(*project,0,0);track.instrument=Instrument::DrumKit;
 track.length=256;track.root_note=65;track.steps[255].enabled=true;track.steps[255].note=83;track.steps[255].attributes=3;
 track.mutation.frozen=true;track.mutation_source[255].note=77;track.mutation_source[255].slide(true);
 track.kit.pads[3].sample[0]=5;track.kit.steps[3][255].enabled=1;
 Sink sink;assert(encode_project(sink,*project));
 auto corrupt=sink.bytes;corrupt.back()^=1;Source invalid{corrupt};assert(!validate_snapshot_encoding(invalid,*project));
 ChunkBuffer tcom,engn,trck,unknown,pseq,kseq,phrase;
 ChunkWriter<ChunkBuffer> common_writer(tcom),track_writer(trck),unknown_writer(unknown);
 assert(write_tcom_section(common_writer,track) && write_track_engn(engn,track,nullptr));
 assert(track_writer.u8(0));
 // The format permits ENGN before TCOM. Common-field decoding must not
 // clobber engine state while avoiding a large temporary kit copy.
 assert(write_framed(track_writer,kChunkEngn,1,engn));
 assert(write_framed(track_writer,kChunkTcom,1,tcom));
 assert(unknown_writer.u8(1) && unknown_writer.u8('?') && unknown_writer.u16(1));
 ChunkWriter<ChunkBuffer> seq_writer(pseq),kit_writer(kseq),phrase_writer(phrase);
 assert(write_pseq_section(seq_writer,track) && write_kit_sequence(kit_writer,track.kit.sequence()));
 assert(phrase_writer.u8(0) && write_framed(phrase_writer,kChunkPseq,4,pseq) && write_framed(phrase_writer,kChunkKseq,1,kseq));
 Work work{sink.bytes,*decoded,*reordered,trck,unknown,phrase};
 pthread_attr_t attrs;assert(pthread_attr_init(&attrs)==0);
 assert(pthread_attr_setstacksize(&attrs,128*1024)==0);
 pthread_t worker;assert(pthread_create(&worker,&attrs,run_on_target_stack,&work)==0);
 assert(pthread_attr_destroy(&attrs)==0 && pthread_join(worker,nullptr)==0);
 std::puts("Project codec 128 KiB stack, reordered chunks and fallback: PASS");
}
