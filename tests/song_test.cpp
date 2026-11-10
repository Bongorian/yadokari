// Stage 2 S2-1: Song state validation, MGXC SONG chunk round-trip, and
// sample-accurate section playback (transpose + section mute) in the
// engine. Section lengths are explicit bars; the Scene Chain stays the
// fallback when the song is empty or disabled.
#include "engine.h"
#include "project_codec.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
using namespace groove;

struct Buffer {
 std::vector<uint8_t> bytes; size_t cursor = 0;
 bool write(const uint8_t* p, size_t n) { bytes.insert(bytes.end(), p, p + n); return true; }
 bool read(uint8_t* p, size_t n) { if (cursor + n > bytes.size()) return false; for (size_t i = 0; i < n; ++i) p[i] = bytes[cursor + i]; cursor += n; return true; }
 bool at_end() const { return cursor == bytes.size(); }
};

int main() {
 // Validation bounds.
 SongState song{};
 assert(valid_song(song));
 song.length = 33; assert(!valid_song(song));
 song.length = 1; song.sections[0] = {0, 1, 0, 0}; assert(valid_song(song));
 song.sections[0].scene = 16; assert(!valid_song(song));
 song.sections[0].scene = 0; song.sections[0].bars = 0; assert(!valid_song(song));
 song.sections[0].bars = 1025; assert(!valid_song(song));
 song.sections[0].bars = 1024; song.sections[0].transpose = 25; assert(!valid_song(song));
 song.sections[0].transpose = -24; assert(valid_song(song));

 // Codec round-trip: extreme-but-valid sections survive byte-stably,
 // and a chain-only project keeps an empty song (lossless as a Song
 // superset: chain data untouched, song defaults).
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p);
  p->chain.enabled = true; p->chain.length = 2; p->chain.scenes[1] = 3;
  p->song.sections[0] = {3, 5, -7, 0x0005};
  p->song.sections[1] = {15, 1024, 24, 0xffff};
  p->song.length = 2; p->song.enabled = true; p->song.loop = false;
  Buffer one, two;
  assert(encode_project(one, *p));
  auto restored = std::make_unique<ProjectState>();
  assert(decode_project(one, *restored));
  assert(restored->song.length == 2 && restored->song.enabled && !restored->song.loop);
  assert(restored->song.sections[0].scene == 3 && restored->song.sections[0].bars == 5 &&
         restored->song.sections[0].transpose == -7 && restored->song.sections[0].mute_mask == 0x0005);
  assert(restored->song.sections[1].scene == 15 && restored->song.sections[1].bars == 1024 &&
         restored->song.sections[1].transpose == 24 && restored->song.sections[1].mute_mask == 0xffff);
  assert(restored->chain.enabled && restored->chain.length == 2 && restored->chain.scenes[1] == 3);
  assert(encode_project(two, *restored) && one.bytes == two.bytes);
  auto plain = std::make_unique<ProjectState>(); make_empty_project(*plain);
  Buffer three; assert(encode_project(three, *plain));
  auto plain_back = std::make_unique<ProjectState>();
  assert(decode_project(three, *plain_back) && plain_back->song.length == 0 && !plain_back->song.enabled);
 }

 // Computed scene length: a 33-step track makes its scene 3 bars.
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p);
  track_at(*p,0,0).length = 33;
  assert(song_scene_bars(*p, 0) == 3);
  track_at(*p,0,0).length = 16;
  assert(song_scene_bars(*p, 0) == 1);
  SongSection section{0, 7, 0, 0};
  assert(song_section_bars(*p, section) == 7);
 }

 // Engine playback: section 0 (scene 0, 2 bars) -> section 1
 // (scene 1, 1 bar, +12 transpose, track 3 muted) -> loop.
 {
  auto p = std::make_unique<ProjectState>(); make_empty_project(*p);
  p->tempo = 120; // at 8 kHz one bar is 16000 samples
  for (unsigned t = 0; t < 16; ++t) p->scenes[0].pattern_for_track[t] = 0;
  p->scenes[1].pattern_for_track[0] = 1;
  p->scenes[1].pattern_for_track[3] = 2;
  auto enable = [&](unsigned pattern, unsigned track, uint8_t note) {
   auto tr = track_at(*p,pattern,track);
   tr.instrument = Instrument::ExternalMidi; tr.root_note = 60;
   tr.steps[0].enabled = true; tr.steps[0].note = note;
  };
  enable(0, 0, 60); enable(0, 3, 65);
  enable(1, 0, 60);
  enable(2, 3, 65);
  p->song.sections[0] = {0, 2, 0, 0};
  p->song.sections[1] = {1, 1, 12, uint16_t(1u << 3)};
  p->song.length = 2; p->song.enabled = true; p->song.loop = true;
  auto e = std::make_unique<Engine>(8000);
  assert(e->stage_project(*p));
  assert(e->submit({CommandType::Start, 0, 0, 0}));
  std::vector<float> out(1024);
  std::vector<uint64_t> change_at; std::vector<unsigned> change_to;
  std::vector<std::pair<uint64_t, unsigned>> notes; // (sample, pitch)
  int last_section = -1; bool saw_active = false;
  for (unsigned rendered = 0; rendered < 120000; rendered += 512) {
   e->render(out.data(), 512);
   AudioVisualFrame frame;
   while (e->pop_visual(frame)) {
    if (frame.song_active) {
     saw_active = true;
     if (int(frame.song_section) != last_section) {
      last_section = frame.song_section;
      change_at.push_back(frame.sample);
      change_to.push_back(frame.song_section);
     }
    }
   }
   MidiEvent m;
   while (e->pop_midi(m))
    if ((m.bytes[0] & 0xf0) == 0x90 && m.bytes[2]) notes.emplace_back(m.sample, m.bytes[1]);
  }
  assert(saw_active);
  // Steady-state section sequence 0 -> 1 -> 0 -> 1 (the first entry
  // is the initially-reported section 0; boundaries drive the rest).
  assert(change_to.size() >= 4 && change_to[0] == 0 && change_to[1] == 1 && change_to[2] == 0 && change_to[3] == 1);
  uint64_t t1 = change_at[1], t2 = change_at[2], t3 = change_at[3];
  assert(t2 > t1 && t2 - t1 > 10000 && t2 - t1 < 22000); // section 1: 1 bar
  assert(t3 > t2 && t3 - t2 > 26000 && t3 - t2 < 38000); // section 0: 2 bars
  auto has_note = [&](uint64_t from, uint64_t to, unsigned pitch) {
   for (auto& n : notes) if (n.first >= from && n.first < to && n.second == pitch) return true;
   return false;
  };
  // Windows start slightly before the reported change sample: the
  // visual frame timestamp trails the boundary by a frame, while
  // notes fire exactly on it. Steps fire once per bar, so 1500
  // samples of slack cannot leak a neighbouring section's notes.
  // Section 1 window: transposed 72 only; track 3 is section-muted.
  assert(has_note(t1 - 1500, t2 - 1500, 72) && !has_note(t1 - 1500, t2 - 1500, 65) && !has_note(t1 - 1500, t2 - 1500, 60));
  // Section 0 window (steady state): plain 60 and 65, no 72.
  assert(has_note(t2 - 1500, t3 - 1500, 60) && has_note(t2 - 1500, t3 - 1500, 65) && !has_note(t2 - 1500, t3 - 1500, 72));
 }
 return 0;
}
