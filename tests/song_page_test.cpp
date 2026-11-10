// S2-3 Song page: ARRANGER door, section list editing through the soft
// keys and knobs, lossless Chain -> Song import, pad section mutes.
#include "performance.h"
#include <cassert>
#include <cstring>
using namespace groove;

int main() {
 // Door + LED.
 {
  Engine engine(8000);
  ProjectState project;
  make_empty_project(project);
  Performance ui(engine, project);
  ui.control(8, 0x02);
  ui.control(8, 0x00);
  assert(ui.mode() == UiMode::Song);
  uint8_t led[62]{};
  ui.button_leds(led);
  assert(led[2]);
  // Knob grammar is section/global split and parameter-free.
  for (unsigned i = 0; i < 8; ++i) assert(ui.knob_parameter(i) == 0);
  assert(!std::strcmp(ui.knob_label(0), "SECTION"));
  assert(!std::strcmp(ui.knob_label(4), "SCENE"));
  assert(!std::strcmp(ui.knob_label(7), "TRANSPOSE"));
  assert(!std::strcmp(ui.knob_label(8), "") && ui.knob_value(8)==0 && ui.knob_normalized(8)==0);
  // ADD from the current scene, then section knob edits.
  assert(ui.soft_key(6));
  assert(project.song.length == 1);
  assert(project.song.sections[0].scene == project.active_scene);
  assert(project.song.sections[0].bars == song_scene_bars(project, project.active_scene));
  assert(project.song.enabled);
  assert(ui.knob(5, 4000));
  assert(project.song.sections[0].bars == song_scene_bars(project, project.active_scene) + 4);
  assert(ui.knob(4, 1000));
  assert(project.song.sections[0].scene == 1);
  assert(ui.knob(7, -2000));
  assert(project.song.sections[0].transpose == -2);
  assert(ui.knob_normalized(7)==22.0f/48 && ui.knob_normalized(4)==1.0f/15);
  // REPEATS rounds bars to whole scene lengths.
  unsigned sb = song_scene_bars(project, 1);
  assert(ui.knob(6, 1000));
  assert(project.song.sections[0].bars % sb == 0);
  // Global toggles.
  assert(ui.knob(2, -1000));
  assert(!project.song.enabled);
  assert(ui.knob(2, 1000));
  assert(project.song.enabled);
  // Pad mutes: hardware 5 is logical pad 9.
  assert(ui.pad(5, 0x10, 3000));
  assert(project.song.sections[0].mute_mask == (1u << 9));
  ui.pad(5, 0x20, 0);
  assert(ui.pad(5, 0x10, 3000));
  assert(project.song.sections[0].mute_mask == 0);
  // Encoder push launches the section scene (accepted in Song mode).
  assert(ui.encoder_push());
  // DELETE removes the only section.
  assert(ui.soft_key(7) && ui.soft_key(0));
  assert(project.song.length == 0);
 }

 // Chain ghost preview + lossless import.
 {
  Engine engine(8000);
  ProjectState project;
  make_empty_project(project);
  project.chain.scenes[0] = 0;
  project.chain.scenes[1] = 1;
  project.chain.length = 2;
  project.chain.bars = 2;
  project.chain.follow[1].bars = 3;
  Performance ui(engine, project);
  ui.set_mode(UiMode::Song);
  assert(ui.song_display_count() == 2);
  ui.set_shift(true);assert(ui.soft_enabled(6));
  assert(!std::strcmp(ui.soft_label(6), "IMPORT"));ui.set_shift(false);
  ui.encoder(1);
  assert(ui.song_cursor() == 1);
  ui.encoder(-1);
  assert(ui.song_cursor() == 0);
  // Two-step arm, then the import itself.
  ui.set_shift(true);assert(ui.soft_key(6));ui.set_shift(false);
  assert(!std::strcmp(ui.soft_label(0), "CONFIRM"));
  assert(project.song.length == 0);
  assert(ui.encoder_push());
  assert(project.song.length == 2);
  assert(project.song.sections[0].scene == 0 && project.song.sections[0].bars == 2);
  assert(project.song.sections[1].scene == 1 && project.song.sections[1].bars == 3);
  assert(project.song.enabled && project.song.loop);
  // ADD appends a third section; CLEAR is armed too.
  assert(ui.soft_key(6));
  assert(project.song.length == 3);
  ui.set_shift(true);assert(ui.soft_key(7));ui.set_shift(false);
  assert(!std::strcmp(ui.soft_label(0), "CONFIRM"));
  assert(ui.encoder_push());
  assert(project.song.length == 0);
  // With the song empty again the chain ghosts back.
  assert(ui.song_display_count() == 2);
 }
 // A full Song exposes no ADD action. Captured sections use the same
 // grammar for their labels, limits, normalization and actual edits.
 {
  Engine engine(8000);ProjectState project;make_empty_project(project);
  project.song.length=32;auto& section=project.song.sections[0];section.ticks=kSongMaxTicks;
  Performance ui(engine,project);ui.set_mode(UiMode::Song);
  assert(!ui.soft_enabled(6) && !ui.soft_key(6) && project.song.length==32);
  assert(!std::strcmp(ui.knob_label(4),"TRACK") && !std::strcmp(ui.knob_label(5),"TICKS"));
  assert(ui.knob_normalized(5)==1 && ui.knob(5,1000) && section.ticks==kSongMaxTicks);
  assert(ui.knob(5,-1000) && section.ticks==kSongMaxTicks-1);
  assert(ui.knob(4,1000));assert(!std::strcmp(ui.knob_label(6),"PATTERN"));
  assert(ui.knob(6,1000) && section.patterns[1]==1 && ui.knob_value(6)==2 && ui.knob_normalized(6)==1.0f/15);
  assert(ui.knob(7,-32000) && section.transpose==-24 && ui.knob_normalized(7)==0);
 }
 return 0;
}
