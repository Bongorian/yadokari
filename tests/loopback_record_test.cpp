#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <unistd.h>

int main() {
  using namespace groove;
  char directory[] = "/tmp/mk3-loopback-test-XXXXXX";
  assert(mkdtemp(directory));
  DesktopFiles fs;
  DesktopSampleAssets assets;
  auto project = std::make_unique<ProjectState>();
  auto engine = std::make_unique<Engine>(8000);
  Performance ui(*engine, *project);
  SessionFiles files(fs, assets, *engine, *project, ui, directory);
  assert(files.start_loopback_probe());
  assert(engine->submit({CommandType::DiagnosticTone, 0, 0, 2}));
  float output[128]{}, input[128]{};
  for (unsigned block = 0; block < 130; ++block) {
    for (unsigned i = 0; i < 64; ++i) {
      float v = 0.1f * sinf(6.2831853f * 440 * float(block * 64 + i) / 8000);
      input[2 * i] = v;
      input[2 * i + 1] = v * 0.5f;
    }
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  LoopbackRecordResult result;
  assert(files.take_loopback_probe_result(result));
  assert(result.saved && result.verified && result.cleaned);
  assert(result.frames == 8000 && result.dropped == 0);
  assert(result.rms_l_u > 60000 && result.rms_l_u < 80000);
  assert(result.rms_r_u > 30000 && result.rms_r_u < 40000);
  assert(result.tone_l_u > 95000 && result.tone_l_u < 105000);
  assert(result.tone_r_u > 47000 && result.tone_r_u < 53000);
  assert(!files.take_loopback_probe_result(result));

  // The Sampler shortcut must capture the external input even when the
  // project's normal capture bus points at the silent Main output.
  ui.set_mode(UiMode::Sampler);
  assert(ui.select_capture_source(1) && ui.select_capture_source(1) && ui.select_capture_source(1));assert(ui.capture_source()==3);
  assert(ui.select_capture_source(1,true) && ui.capture_source()==4);assert(ui.select_capture_source(1) && ui.capture_source()==20);assert(ui.select_capture_source(1) && ui.capture_source()==36);
  assert(ui.select_capture_source(1) && ui.capture_source()==51);assert(ui.select_capture_source(1) && ui.capture_source()==52);assert(ui.select_capture_source(-7) && ui.capture_source()==0);
  engine->render(output,0);
  ui.set_shift(true);
  assert(__builtin_strcmp(ui.soft_label(4), "REC INPUT") == 0);
  assert(ui.soft_key(4) && files.service() && ui.live_recording());
  for (unsigned block = 0; block < 125; ++block) {
    for (unsigned i = 0; i < 64; ++i) {
      float v = 0.2f * sinf(6.2831853f * 440 * float(block * 64 + i) / 8000);
      input[2 * i] = v;
      input[2 * i + 1] = v * 0.5f;
    }
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(__builtin_strcmp(ui.soft_label(4), "STOP RECORD") == 0);
  assert(ui.soft_key(4) && files.service());
  for (unsigned block = 0; block < 4 && ui.live_recording(); ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(!ui.live_recording());
  assert(ui.notice() == UiNotice::SampleRecorded);
  RecordingResult recording;
  assert(files.take_recording_result(recording));
  assert(recording.saved && recording.verified && recording.assigned);
  assert(recording.frames >= 8000 && recording.frames <= 8064 && recording.slot == 0 && recording.dropped == 0);
  assert(recording.peak_l_u > 190000 && recording.peak_r_u > 95000);
  const auto track = track_at(*project,0,0);
  assert(track.instrument == Instrument::Sampler);
  const char* path = project->assets[track.sample_slot].path;
  assert(path[0] && fs.exists(path));
  WavView recorded = assets.lookup(path);
  assert(recorded.frames >= 8000 && recorded.data);
  assert(ui.browser().analysis_source == recorded.data);
  assert(fabsf(wav_sample(recorded, 40, 0)) > 0.1f);
  assert(fabsf(wav_sample(recorded, 40, 1)) > 0.05f);
  float cue[128]{};
  assert(__builtin_strcmp(ui.soft_label(5), "PREVIEW") == 0);
  assert(ui.soft_key(5) && files.service());
  assert(__builtin_strcmp(ui.soft_label(5), "STOP PREVIEW") == 0 && ui.soft_selected(5));
  engine->render_ports(output, cue, nullptr, 64);
  float cue_energy = 0;
  for (float frame : cue) cue_energy += fabsf(frame);
  assert(cue_energy > 0.1f);
  assert(ui.soft_key(5) && files.service());
  engine->render(output, 64);
  assert(!ui.browser().previewing && __builtin_strcmp(ui.soft_label(5), "PREVIEW") == 0);
  engine->render(output, 64);
  ui.set_shift(false);
  assert(ui.pad(12, 0x10, 4095));
  engine->render(output, 64);
  float energy = 0;
  for (float frame : output) energy += fabsf(frame);
  assert(energy > 0.1f);

  // The remote diagnostic uses the same saved-WAV and binding path with a
  // bounded one-second input take, then reports a machine-readable outcome.
  assert(files.start_sample_workflow_probe());
  for (unsigned block = 0; block < 130; ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(files.take_recording_result(recording));
  assert(recording.saved && recording.verified && recording.assigned);
  assert(recording.frames == 8000 && recording.slot == 1 && recording.dropped == 0);
  const char* second = project->assets[1].path;
  assert(second[0] && fs.exists(second));

  // A track tap records only that track, even while another track is audible.
  ui.set_mode(UiMode::Routing);
  assert(ui.knob(6, 3000));
  assert(project->routing.capture_bus == 3);
  ui.set_mode(UiMode::Sampler);
  ui.set_shift(true);
  assert(ui.soft_key(6) && files.service());
  assert(engine->submit({CommandType::Trigger, 0, 0, 110}));
  for (unsigned block = 0; block < 16; ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(ui.soft_key(6) && files.service());
  for (unsigned block = 0; block < 4 && ui.live_recording(); ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(files.take_recording_result(recording));
  assert(recording.saved && recording.verified && recording.assigned);
  assert(recording.frames >= 1024 && recording.frames <= 1088 && recording.peak_l_u > 1000);
  const char* track_take = project->assets[2].path;
  assert(track_take[0] && fs.exists(track_take));
  ui.set_mode(UiMode::Routing);
  ui.set_shift(false);
  assert(ui.knob(6, 1000));
  assert(project->routing.capture_bus == 4);
  ui.set_mode(UiMode::Sampler);
  ui.set_shift(true);
  assert(ui.soft_key(6) && files.service());
  assert(engine->submit({CommandType::Trigger, 0, 0, 110}));
  for (unsigned block = 0; block < 16; ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(ui.soft_key(6) && files.service());
  for (unsigned block = 0; block < 4 && ui.live_recording(); ++block) {
    engine->render_ports(output, nullptr, input, 64);
    assert(files.service());
  }
  assert(files.take_recording_result(recording));
  assert(recording.saved && recording.verified && recording.assigned);
  assert(recording.frames >= 1024 && recording.frames <= 1088 && recording.peak_l_u == 0 && recording.peak_r_u == 0);
  const char* silent_track_take = project->assets[3].path;
  assert(silent_track_take[0] && fs.exists(silent_track_take));
  ui.set_mode(UiMode::Step);
  assert(ui.set_step_sample_lock(0, 300, 1));
  assert(ui.set_step_sample_lock(0, 301, 0));
  assert(ui.set_step_sample_lock(0, 302, 1));
  assert(valid_project(*project));
  assert(ui.step_sample_lock_value(0, 300) == 1);
  assert(ui.set_step_sample_lock(0, 300, -1));
  assert(ui.step_sample_lock_value(0, 300) == -1);
  SampleEdit chops = track_at(*project,0,0).sample_edit;
  chops.slice_count = 16;
  for (unsigned i = 0; i <= 16; ++i) chops.markers[i] = float(i) / 16;
  assert(ui.sample_edit(chops));
  ui.set_mode(UiMode::Sampler);
  ui.set_shift(true);
  assert(__builtin_strcmp(ui.soft_label(7), "CHOP STEPS") == 0);
  assert(ui.soft_key(7) && ui.soft_key(0));
  const auto chopped = track_at(*project,0,0);
  assert(chopped.length == 16 && valid_project(*project));
  for (unsigned i = 0; i < 16; ++i) {
    assert(chopped.steps[i].enabled);
    assert(chopped.steps[i].locks[0].enabled && chopped.steps[i].locks[0].id == 301);
    assert(chopped.steps[i].locks[0].value == float(i));
  }
  // The lock chooses asset 2 (audible) over the track's silent asset 4.
  assert(ui.set_step_sample_lock(0, 300, 1));
  assert(engine->submit({CommandType::Stop, 0, 0, 0}));
  for (unsigned block = 0; block < 140; ++block) engine->render(output, 64);
  assert(engine->submit({CommandType::Start, 0, 0, 0}));
  float locked_energy = 0;
  for (unsigned block = 0; block < 6; ++block) {
    engine->render(output, 64);
    for (float frame : output) locked_energy += fabsf(frame);
  }
  assert(locked_energy > 0.1f);
  assert(engine->submit({CommandType::Stop, 0, 0, 0}));
  for (unsigned block = 0; block < 32; ++block) engine->render(output, 64);
  assert(ui.set_step_sample_lock(0, 300, -1));
  assert(engine->submit({CommandType::Start, 0, 0, 0}));
  float unlocked_energy = 0;
  for (unsigned block = 0; block < 6; ++block) {
    engine->render(output, 64);
    for (float frame : output) unlocked_energy += fabsf(frame);
  }
  assert(unlocked_energy < 0.001f);
  {
    auto capture_project = std::make_unique<ProjectState>();make_empty_project(*capture_project);
    auto capture_track_state=track_at(*capture_project,0,0);capture_track_state.instrument=Instrument::Sampler;capture_track_state.sample_slot=0;capture_track_state.level=0;capture_track_state.drive=1;capture_track_state.sample_edit.fade_ms=0;
    uint8_t pcm[4096];for(unsigned i=0;i<2048;++i){pcm[2*i]=0;pcm[2*i+1]=48;}
    WavView sample_slots[kSampleAssets]{};sample_slots[0]={pcm,2048,8000,1,16,1};
    auto peak_at=[&](uint8_t source,float* tail=nullptr,unsigned settle=0){auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*capture_project,sample_slots));float out[2048]{};e->render(out,0);e->render(out,1024);assert(e->submit({CommandType::Trigger,0,0,127}));for(unsigned x=0;x<settle;x+=256)e->render(out,256);assert(e->start_live_record(false,256,source));e->render(out,256);float peak=0,last=0;LiveRecordFrame f;unsigned frames=0;while(e->pop_live_record(f)){float amplitude=fmaxf(fabsf(f.l),fabsf(f.r));peak=fmaxf(peak,amplitude);if(frames>=128)last=fmaxf(last,amplitude);++frames;}assert(frames==256 && e->live_record_done());if(tail)*tail=last;return peak;};
    float dry=peak_at(19),insert=peak_at(35),fader=peak_at(3),returns=peak_at(51),performance=peak_at(52);
    assert(dry>0.1f && insert>dry && fader<0.01f && returns<0.01f && performance<0.01f);
    capture_track_state.level=0.5f;assert(peak_at(51)>0.02f && peak_at(52)>0.02f);
    capture_project->effects.delay_mix=0;capture_project->effects.reverb_mix=0;capture_project->effects.delay_ms=10;
    auto throw_capture=[&](uint8_t source){auto e=std::make_unique<Engine>(8000);assert(e->stage_project(*capture_project,sample_slots));float out[2048]{};e->render(out,0);e->render(out,1024);assert(e->submit({CommandType::Trigger,0,0,127}));Command fx{CommandType::SetPerformanceFx,0,0,uint16_t(PerformanceFx::DelayThrow)};fx.scalar=1;assert(e->submit(fx));e->render(out,256);assert(e->start_live_record(false,256,source));e->render(out,256);float energy=0;unsigned frames=0;LiveRecordFrame f;while(e->pop_live_record(f)){energy+=fabsf(f.l)+fabsf(f.r);++frames;}assert(frames==256 && e->live_record_done());return energy;};
    float return_energy=throw_capture(51),performance_energy=throw_capture(52);assert(return_energy>1 && performance_energy>return_energy*1.1f);
    capture_track_state.insert_fx[0]=2;capture_track_state.insert_fx[1]=0.7f;capture_track_state.insert_fx[2]=0;float filtered_tail=0;peak_at(35,&filtered_tail,1024);assert(filtered_tail<0.02f);
    auto controller_engine=std::make_unique<Engine>(8000);assert(controller_engine->stage_project(*capture_project,sample_slots));Performance controller(*controller_engine,*capture_project);controller.set_mode(UiMode::Effects);assert(controller.soft_key(3) && controller.fx_page()==3);assert(controller.knob(0,1000));assert(capture_track_state.insert_fx[0]==3);
    controller.set_shift(true);assert(controller.knob(1,1000));bool locked=false;for(const auto& lock:capture_track_state.steps[0].locks)locked|=lock.enabled && lock.id==95;assert(locked);controller.set_shift(false);
    capture_project->morph_b[0].values[morph_index(95)]=0.3f;controller.set_shift(true);assert(controller.soft_key(4));assert(capture_track_state.insert_fx[0]==0 && capture_track_state.insert_fx[4]==3 && capture_project->morph_b[0].values[morph_index(99)]==0.3f);controller.set_shift(false);assert(controller.undo());assert(capture_track_state.insert_fx[0]==3 && capture_track_state.insert_fx[4]==0 && capture_project->morph_b[0].values[morph_index(95)]==0.3f);char project_file[160];assert(join_path(project_file,sizeof project_file,directory,"insert.mgx"));FileWriter writer(fs,project_file);assert(encode_project(writer,*capture_project) && writer.finish());auto restored=std::make_unique<ProjectState>();assert(read_project_file(fs,project_file,*restored));assert(track_at(*restored,0,0).insert_fx[0]==3 && restored->morph_b[0].values[morph_index(95)]==0.3f);assert(fs.remove(project_file));
  }
  assert(fs.remove(silent_track_take));
  assert(fs.remove(track_take));
  assert(fs.remove(second));
  assert(fs.remove(path));
  assert(::rmdir(directory) == 0);
}
