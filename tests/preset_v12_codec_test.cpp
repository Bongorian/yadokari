#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using namespace groove;
struct Sink{std::vector<uint8_t> bytes;bool write(const uint8_t* p,size_t n){bytes.insert(bytes.end(),p,p+n);return true;}};
struct Source{const std::vector<uint8_t>& bytes;size_t at=0;bool read(uint8_t* p,size_t n){if(n>bytes.size()-at)return false;__builtin_memcpy(p,bytes.data()+at,n);at+=n;return true;}bool at_end()const{return at==bytes.size();}};
static PresetIdentity identity(){PresetIdentity i;assert(parse_preset_guid("f0123456789abcde0123456789abcdef",i.guid));i.revision=3;i.origin=PresetOrigin::Factory;return i;}
static void roundtrip(ProjectState& p,ProjectState& q){Sink a;assert(encode_project(a,p));assert(a.bytes[4]==kFormatVersion);Source r{a.bytes};PreservedStore store;assert(decode_project(r,q,&store));Sink b;assert(encode_project(b,q,&store));assert(a.bytes==b.bytes);}
int main(){
 auto p=std::make_unique<ProjectState>();auto q=std::make_unique<ProjectState>();make_empty_project(*p);
 auto t=track_at(*p,0,0);t.instrument=Instrument::VirtualAnalog;t.preset_identity=identity();t.preset_slot=9;__builtin_strcpy(t.track_name,"Bass role");__builtin_strcpy(t.preset_name,"Warm VA");t.preset_fx.capture(t);t.preset_fx.inserts[0]=20;t.preset_fx.inserts[1]=.75f;
 snapshot_sound(p->sounds[9],t);p->sounds[9].fx=t.preset_fx;roundtrip(*p,*q);
 assert(!__builtin_strcmp(q->tracks[0].track_name,"Bass role"));assert(same_preset_revision(q->sounds[9].identity,t.preset_identity));assert(q->sounds[9].fx.present && q->sounds[9].fx.inserts[1]==.75f);
 auto mounted=track_at(*q,0,1);__builtin_strcpy(mounted.track_name,"Keep role");mount_sound(mounted,q->sounds[9]);assert(!__builtin_strcmp(mounted.track_name,"Keep role") && mounted.insert_fx[1]==.75f);
 // New envelope children retain revision, flags and raw payload on roundtrip.
 PreservedStore opaque;ChunkBuffer extension;uint32_t f;assert(extension.begin_frame(0x54534554,7,f,0x42));uint8_t payload[]={1,2,3};assert(extension.write(payload,3) && extension.end_frame(f));assert(opaque.tracks[0].auxiliary.set(extension.data,extension.size) && opaque.sounds[9].auxiliary.set(extension.data,extension.size));Sink with_unknown;assert(encode_project(with_unknown,*p,&opaque));Source unknown_source{with_unknown.bytes};PreservedStore restored;assert(decode_project(unknown_source,*q,&restored));Sink again;assert(encode_project(again,*q,&restored) && again.bytes==with_unknown.bytes);p->sounds[10]=p->sounds[9];Sink duplicate;assert(!encode_project(duplicate,*p));p->sounds[10]=SoundState{};
 make_empty_project(*p);p->content_kind=ContentKind::Preset;t=track_at(*p,0,0);t.instrument=Instrument::VirtualAnalog;t.preset_identity=identity();__builtin_strcpy(t.preset_name,"Factory VA");p->preset_fx.capture(t);p->preset_fx.inserts[0]=20;p->preset_fx.inserts[1]=.75f;roundtrip(*p,*q);assert(q->tracks[0].preset_identity.revision==3 && q->tracks[0].preset_fx.present);
 make_empty_project(*p);p->content_kind=ContentKind::Kit;t=track_at(*p,0,0);t.instrument=Instrument::DrumKit;t.preset_identity=identity();__builtin_strcpy(t.preset_name,"Factory Kit");p->preset_fx.capture(t);roundtrip(*p,*q);assert(q->tracks[0].instrument==Instrument::DrumKit && q->preset_fx.present);
 // Portable unknown fields keep their parent scope, including through Song.
 PreservedStore portable;assert(portable.tracks[0].preset_root.set(extension.data,extension.size));assert(portable.tracks[0].preset_track.set(extension.data,extension.size));assert(portable.tracks[0].preset_sound.set(extension.data,extension.size));
 Sink portable_bytes;assert(encode_project(portable_bytes,*p,&portable));Source portable_source{portable_bytes.bytes};PreservedStore portable_read;assert(decode_project(portable_source,*q,&portable_read));assert(q->tracks[0].definition_cookie && portable_read.tracks[0].preset_root.size==extension.size && portable_read.tracks[0].preset_track.size==extension.size && portable_read.tracks[0].preset_sound.size==extension.size);Sink portable_again;assert(encode_project(portable_again,*q,&portable_read) && portable_again.bytes==portable_bytes.bytes);
 // An opaque definition belongs to its cookie, even after its former slot is reused.
 DefinitionExtensions owners;uint32_t cookie=q->tracks[0].definition_cookie;assert(owners.retain(portable_read.tracks[0],cookie));PreservedTrackEntry projected;assert(owners.project(projected,cookie));assert(projected.preset_root.size==extension.size);assert(owners.project(projected,0) && projected.empty());assert(owners.project(projected,cookie) && projected.preset_track.size==extension.size);projected.clear();
 // Count repeated sidecars before allocation/publication; a shared cache entry
 // does not make its repeated emitted bytes free in the 8 MiB file budget.
 auto large=std::make_unique<uint8_t[]>(PreservedStore::kMaxEntryBytes);PreservedTrackEntry big;assert(big.preset_root.set(large.get(),PreservedStore::kMaxEntryBytes));DefinitionExtensions budget;uint32_t big_cookie=0;bool inserted=false;assert(budget.retain(big,big_cookie,&inserted) && inserted);auto model=std::make_unique<ProjectState>();make_empty_project(*model);PreservedStore empty;model->tracks[0].definition_cookie=big_cookie;for(unsigned n=0;n<3;++n)model->sounds[n].definition_cookie=big_cookie;assert(definition_projection_fits(budget,*model,empty));model->sounds[3].definition_cookie=big_cookie;assert(!definition_projection_fits(budget,*model,empty));assert(definition_projection_fits(budget,*model,empty,kTracks,3,0));model->sounds[3].definition_cookie=0;budget.discard_unpublished(big_cookie);assert(!definition_projection_fits(budget,*model,empty));big.clear();
 Sink a;assert(encode_project(a,*p));a.bytes[4]=12;Source old{a.bytes};assert(!decode_project(old,*q));
 p->tracks[0].preset_identity={};Sink missing;assert(!encode_project(missing,*p));
 std::puts("MGXC current: named tracks, standalone identities, pool identity/FX, stable roundtrip, Kit-as-one-engine and old-format rejection passed");
}
