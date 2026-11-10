// Stage 3 S3-1: SCR-EDT-05 WAVE EDIT (Sampler page 3) and SCR-EDT-06
// SLICE & ZONES (Sampler page 4). Page cycling reaches 3/4, the wave
// knobs edit the SampleEdit through the existing setter, zones are
// created on first zone-knob use (one per slice, even key split) and
// edited per slice, with the encoder push swapping K6/K7 between the
// key range and the velocity range.
#include "performance.h"
#include <cassert>
#include <cmath>
#include <cstring>
using namespace groove;

static bool near(float a, float b) { return std::fabs(a - b) < 0.0011f; }

int main() {
 Engine engine(8000);
 ProjectState project;
 make_empty_project(project);
 track_at(project,0,0).instrument = Instrument::Sampler;
 Performance ui(engine, project);
 ui.set_mode(UiMode::Sampler);
 auto page_fwd = [&]() { ui.encoder_direction(0x08); };
 page_fwd();
 page_fwd();
 page_fwd();
 assert(ui.sampler_page() == 3);
 // WAVE EDIT grammar.
 assert(!std::strcmp(ui.knob_label(0), "START"));
 assert(!std::strcmp(ui.knob_label(2), "LOOP"));
 assert(!std::strcmp(ui.knob_label(3), "SPEED"));
 assert(!std::strcmp(ui.knob_label(7), "RELEASE"));
 for (unsigned i = 0; i < 8; ++i) assert(ui.knob_parameter(i) == 0);
 const auto trk = track_at(project,0,0);
 assert(near(trk.sample_edit.start, 0.0f));
 assert(ui.knob(0, 10000));
 assert(near(trk.sample_edit.start, 0.1f));
 assert(ui.knob(4, 1000));  // fine start
 assert(near(trk.sample_edit.start, 0.1002f));
 assert(ui.knob(3, 1000));
 assert(near(trk.sample_edit.speed, 1.05f));
 assert(ui.knob(2, 1000));
 assert(trk.sample_edit.mode == SampleMode::Gate);
 assert(ui.knob(6, 1000));
 assert(near(trk.sample_edit.attack_ms, 5.0f));
 assert(ui.knob(7, 1000));
 assert(near(trk.sample_edit.release_ms, 13.0f));

 page_fwd();
 assert(ui.sampler_page() == 4);
 page_fwd();
 assert(ui.sampler_page() == 4);  // bound holds
 // SLICE & ZONES grammar.
 assert(!std::strcmp(ui.knob_label(0), "SLICE"));
 assert(!std::strcmp(ui.knob_label(1), "SLICE START"));
 assert(!std::strcmp(ui.knob_label(5), "KEY LOW"));
 assert(!std::strcmp(ui.knob_label(7), "ROOT"));
 // Four slices via the public setter (page 2's own path is covered by
 // the sampler tests).
 auto es = trk.sample_edit;
 es.slice_count = 4;
 for (unsigned i = 0; i <= 4; ++i) es.markers[i] = float(i) * 0.25f;
 assert(ui.sample_edit(es));
 assert(trk.sample_edit.slice_count == 4);
 // Cursor to slice 1.
 assert(ui.knob(0, 1000));
 assert(ui.slice_cursor() == 1);
 // First zone-knob turn materialises one zone per slice (even key
 // split, full velocity, roots ascending from the track root).
 int root0 = trk.root_note;
 assert(ui.knob(5, 10000));
 assert(trk.sample_edit.zone_count == 4);
 assert(trk.sample_edit.zones[1].key_low == 42);
 assert(trk.sample_edit.zones[1].key_high == 63);
 assert(trk.sample_edit.zones[1].velocity_low == 1);
 assert(trk.sample_edit.zones[1].velocity_high == 127);
 assert(trk.sample_edit.zones[1].root == root0 + 1);
 assert(ui.knob(7, 2000));
 assert(trk.sample_edit.zones[1].root == root0 + 3);
 // Encoder push swaps K6/K7 to the velocity range.
 assert(ui.encoder_push());
 assert(ui.zone_vel());
 assert(!std::strcmp(ui.knob_label(5), "VEL LOW"));
 assert(!std::strcmp(ui.knob_label(6), "VEL HIGH"));
 assert(ui.knob(5, -1000));
 assert(trk.sample_edit.zones[1].velocity_low == 1);  // clamped: velocity_low >= 1
 assert(ui.knob(6, -1000));
 assert(trk.sample_edit.zones[1].velocity_high == 126);
 assert(ui.encoder_push());
 assert(!ui.zone_vel());
 // Slice boundary and choke edits.
 assert(ui.knob(1, 1000));
 assert(near(trk.sample_edit.markers[1], 0.252f));
 assert(ui.knob(3, 2000));
 assert(trk.sample_edit.choke == 2);
 // SHIFT+encoder reaches Warp before the general shifted-input guard.
 ui.set_shift(true);bool before_warp=trk.sample_edit.slice_warp[ui.slice_cursor()];
 assert(ui.encoder_push());assert(bool(trk.sample_edit.slice_warp[ui.slice_cursor()])!=before_warp);
 ui.set_shift(false);
 // Clear zones is a confirmed Top action, not a value knob.
 ui.set_shift(true);
 assert(ui.soft_key(7) && ui.soft_key(0));
 assert(trk.sample_edit.zone_count == 0);
 ui.set_shift(false);
 return 0;
}
