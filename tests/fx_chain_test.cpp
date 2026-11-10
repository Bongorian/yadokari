// Unit 17: F-01 insert bypass (ids 102/103 + TrackState.insert_disabled,
// TCOM optional tail) and the SCR-EDT-07 FX CHAIN page (fx_page 5).
#include "performance.h"
#include "project_codec.h"
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstring>
struct Sink { std::vector<uint8_t> bytes; bool write(const uint8_t* p,size_t n) { bytes.insert(bytes.end(),p,p+n); return true; } };
struct Source { const std::vector<uint8_t>& bytes; size_t pos=0; bool read(uint8_t* p,size_t n) { if(n>bytes.size()-pos) return false; for(size_t i=0;i<n;++i) p[i]=bytes[pos++]; return true; } bool at_end() const { return pos==bytes.size(); } };
using namespace groove;
int main(){
 // ENABLE parameters are registered, default to enabled (1).
 assert(parameter_descriptor(102) && parameter_descriptor(103));
 assert(std::strcmp(parameter_descriptor(102)->name,"FX1 ENABLE")==0);
 ProjectState project; make_empty_project(project);
 assert(parameter_base(project,track_at(project,0,0),102)==1.0f);
 assert(parameter_base(project,track_at(project,0,0),103)==1.0f);
 // The morph index sentinel keeps ENABLE ids out of the morph tables.
 assert(parameter_index(102)>=kMorphParameters && parameter_index(103)>=kMorphParameters);
 // set_track_parameter flips the persisted bits.
 auto trk=track_at(project,0,0);
 assert(set_track_parameter(trk,102,0.0f) && trk.insert_disabled==1);
 assert(set_track_parameter(trk,103,0.0f) && trk.insert_disabled==3);
 assert(set_track_parameter(trk,102,1.0f) && trk.insert_disabled==2);
 assert(valid_project(project));
 // Codec round trip preserves the mask; validation accepts it.
 Sink sink; assert(encode_project(sink,project));
 ProjectState back; Source source{sink.bytes}; assert(decode_project(source,back));
 assert(track_at(back,0,0).insert_disabled==2);
 assert(valid_project(back));
 // CHAIN page navigation and actions through the real Performance surface.
 Engine engine(48000); ProjectState live; make_empty_project(live);
 Performance perf(engine,live);
 perf.set_mode(UiMode::Effects);
 assert(perf.fx_page()==0);
 for(int i=0;i<4;++i) perf.encoder_direction(0x08); // +1 page step each
 assert(perf.fx_page()==5); // page 4 (pad insert) skipped for non-kit tracks
 assert(perf.chain_node()==0);
 perf.encoder(1); assert(perf.chain_node()==1);
 assert(perf.knob_parameter(4)==97); // INSERT 1 MIX
 std::printf("fx-chain: page walk + node + key param ok\n");
 // BYPASS soft key toggles the persisted bit through the parameter path.
 assert(std::strcmp(perf.soft_label(5),"BYPASS")==0);
 assert(perf.soft_key(5));
 assert(track_at(live,live.active_pattern,0).insert_disabled==1);
 assert(parameter_base(live,track_at(live,0,0),102)==0.0f);
 assert(perf.soft_key(5));
 assert(track_at(live,live.active_pattern,0).insert_disabled==0);
 // EDIT soft key jumps to the SLOT page focused on the node slot.
 assert(perf.soft_key(6));
 assert(perf.fx_page()==3 && perf.fx_focus_slot()==0);
 std::printf("fx-chain test passed\n");
 return 0;
}
