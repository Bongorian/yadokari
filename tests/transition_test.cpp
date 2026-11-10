// Transition acceptance test (SPEC 2.5 / Stage 1 gate): input
// injected during an active transition is applied immediately (not
// delayed or dropped), transition frames repaint only the body band
// (the knob band follows its own rules - the dirty budget), and the
// landed frame is pixel-identical to a fresh single draw.
#include "performance.h"
#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace groove;
static constexpr uint32_t kSentinel = 0xFFDEBEEFu;
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);project.tracks[0].instrument=project.tracks[1].instrument=Instrument::DaisyAnalogDrums;
 Performance perf(engine, project);
 auto renderer = std::make_unique<UiRenderer>();
 auto fresh = std::make_unique<UiRenderer>();
 auto landed = [&]() {
  fresh->invalidate();fresh->draw(project, perf);
  return std::memcmp(fresh->pixels, renderer->pixels, sizeof(uint32_t) * 2 * 480 * 272) == 0;
 };
 perf.set_mode(UiMode::Settings);
 perf.animate(1000); renderer->draw(project, perf);
 perf.animate(2000); renderer->draw(project, perf);
 // --- EditPage slide (three frames) with input injected mid-transition ---
 perf.set_mode(UiMode::Synth);
 perf.animate(4000); renderer->draw(project, perf); // fade frame 0
 perf.animate(4050); renderer->draw(project, perf); // mid fade
 // Track selection applies NOW, not after the transition.
 perf.control(2, 0x02); perf.control(2, 0x00);
 assert(perf.selected_track() == 1);
 // A subpage soft key also applies NOW (and retargets the visuals).
 assert(perf.soft_key(1));
 assert(perf.instrument_view() == 1);
 perf.animate(4300); renderer->draw(project, perf);
 assert(landed());
 // Opening an overlay cancels the slide under the existing motion rule.
 // Settled floating cells follow their own dirty budget.
 assert(perf.soft_key(0)); // back to GENERATOR subpage: slide starts
 assert(perf.instrument_view() == 0);
 perf.animate(5000); perf.dispatch({ControllerInput::Type::KnobTouch,0,1});perf.dispatch({ControllerInput::Type::KnobTouch,0,0});
 renderer->draw(project, perf); // visible knob overlay, slide frame 0
 renderer->pixels[0][240 * 480 + 10] = kSentinel; // knob band
 renderer->pixels[1][240 * 480 + 470] = kSentinel;
 for (unsigned t = 5050; t <= 5100; t += 50) {
  perf.animate(t); renderer->draw(project, perf);assert(perf.knob_overlay_mask()==1);
  assert(renderer->pixels[0][240 * 480 + 10] == kSentinel);
  assert(renderer->pixels[1][240 * 480 + 470] == kSentinel);
 }
 perf.animate(5300); renderer->draw(project, perf);
 // Restore the two sentinel pixels from the fresh canvas before the
 // byte comparison (they were corrupted on purpose above).
 fresh->draw(project, perf);
 renderer->pixels[0][240 * 480 + 10] = fresh->pixels[0][240 * 480 + 10];
 renderer->pixels[1][240 * 480 + 470] = fresh->pixels[1][240 * 480 + 470];
 assert(landed());
 // A knob turn during the NEXT slide changes the value immediately.
 assert(perf.soft_key(1));
 perf.animate(6000); renderer->draw(project, perf);
 float before = track_at(project,0,1).cutoff;
 assert(perf.knob(2, -4000)); // TONE page knob 3 = CUTOFF, turn down
 assert(track_at(project,0,1).cutoff < before);
 perf.animate(6300); renderer->draw(project, perf);
 // The edited knob carries renderer animation state (flash / arc
 // smoothing) that a fresh renderer cannot share; a context change
 // resets it deterministically, so land the byte comparison there.
 perf.control(2, 0x01); perf.control(2, 0x00); // select track 1 (Kick)
 assert(perf.selected_track() == 0);
 perf.animate(7000); renderer->draw(project, perf);
 assert(landed());
 std::printf("transition (input injection, dirty budget) test passed\n");
 return 0;
}
