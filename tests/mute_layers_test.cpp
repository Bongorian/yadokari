// Stage 4 S4-3: two-layer mute. The global layer (muted_tracks)
// applies everywhere; the pattern layer (PatternState.mute_mask)
// applies while a track plays that pattern; track_muted is the
// union. MUTE+pad edits the global layer (unchanged behaviour),
// Shift+MUTE+pad edits the pattern layer of the track's current
// pattern. While MUTE is held the pads colour-code the layers:
// dim red = global, mid red = pattern, bright red = both.
#include "performance.h"
#include "project_codec.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace groove;
namespace {
struct Buffer {
 std::vector<uint8_t> bytes; size_t at = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (at + n > bytes.size()) return false; __builtin_memcpy(p, bytes.data() + at, n); at += n; return true; }
 bool at_end() const { return at == bytes.size(); }
};
} // namespace
int main() {
 Engine engine(8000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 // Global layer on track 3 (the classic MUTE action).
 assert(perf.mute_or_solo(3, false));
 assert(track_muted_global(project, 3) && track_muted(project, 3));
 assert(!track_muted_pattern(project, 3));
 // Pattern layer on track 4 (Shift+MUTE), pattern 0 is its clip.
 assert(perf.mute_or_solo(4, false, true));
 assert(track_muted_pattern(project, 4) && track_muted(project, 4));
 assert(!track_muted_global(project, 4));
 assert(project.patterns[0].mute_mask & (1u << 4));
 assert(!(project.muted_tracks & (1u << 4)));
 // The pattern layer follows the pattern, not the track: moving
 // track 4 to pattern 1 un-mutes it; pattern 1 can mute it too.
 project.clips[4].pattern = 1;
 assert(!track_muted(project, 4));
 assert(perf.mute_or_solo(4, false, true));
 assert(project.patterns[1].mute_mask & (1u << 4));
 assert(track_muted(project, 4));
 project.clips[4].pattern = 0;
 assert(track_muted(project, 4)); // pattern 0 layer still holds it
 // Toggling the pattern layer off again (undo path via the API).
 assert(perf.mute_or_solo(4, false, true));
 assert(!track_muted(project, 4));
 // Global toggle still round-trips.
 assert(perf.mute_or_solo(3, false));
 assert(!track_muted(project, 3));
 std::printf("mute layers ok\n");
 // Codec round trip: pattern masks persist per pattern.
 assert(perf.mute_or_solo(5, false, true)); // pattern 0 (clips[5])
 project.clips[6].pattern = 2;
 assert(perf.mute_or_solo(6, false, true)); // pattern 2
 Buffer b; assert(encode_project(b, project));
 ProjectState back; Buffer r; r.bytes = b.bytes;
 assert(decode_project(r, back));
 assert(back.patterns[0].mute_mask & (1u << 5));
 assert(back.patterns[2].mute_mask & (1u << 6));
 assert(!(back.patterns[1].mute_mask & (1u << 6)));
 std::printf("mute layers codec ok\n");
 std::printf("mute layers test passed\n");
 return 0;
}
