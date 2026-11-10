// Partial redraw test (SPEC 2.5 / 9-4): with a static page, stopped
// transport and no live input, a second draw must leave the canvas
// byte-identical; sentinel pixels prove the body and knob bands are
// actually skipped while the header band repaints; a context change
// (page switch) repaints everything; and the multi-frame result is
// pixel-identical to a fresh renderer's single draw of the same
// state.
#include "performance.h"
#include "ui_renderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace groove;
static constexpr uint32_t kSentinel = 0xFFDEBEEFu; // opaque: the fade composite normalizes alpha to 0xFF
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 perf.set_mode(UiMode::Settings);
 // Touch/release after settling keeps the overlay visible without a held-touch repaint.
 auto renderer = std::make_unique<UiRenderer>();
 auto copy = std::make_unique<uint32_t[]>(2 * 480 * 272);
 auto snap = [&]() { std::memcpy(copy.get(), renderer->pixels, sizeof(uint32_t) * 2 * 480 * 272); };
 auto same = [&]() { return std::memcmp(copy.get(), renderer->pixels, sizeof(uint32_t) * 2 * 480 * 272) == 0; };

 // Settle first: the page-change frame draws the transition line's
 // zero-length start dot (pre-existing behaviour, gone next frame).
 perf.browser().status[0]=0;perf.animate(1000);perf.notify(UiNotice::None); // isolate static caching from the new discovery toast
 renderer->draw(project, perf);
 perf.animate(2000);perf.dispatch({ControllerInput::Type::KnobTouch,0,1});perf.dispatch({ControllerInput::Type::KnobTouch,0,0});
 renderer->draw(project, perf);
 snap();
 // Static frame: identical output.
 perf.animate(2500);
 renderer->draw(project, perf);
 assert(same());
 // Sentinels: header repaints, body and knob bands are skipped.
 renderer->pixels[0][10 * 480 + 10] = kSentinel;   // header band
 renderer->pixels[0][100 * 480 + 10] = kSentinel;  // body band
 renderer->pixels[0][240 * 480 + 10] = kSentinel;  // knob band
 perf.animate(3000);
 renderer->draw(project, perf);
 assert(renderer->pixels[0][10 * 480 + 10] != kSentinel);
 assert(renderer->pixels[0][100 * 480 + 10] == kSentinel);
 assert(renderer->pixels[0][240 * 480 + 10] == kSentinel);
 // A backward page switch slides new content left in the fixed body.
 // The overlay remains above it; after at most three frames the body lands.
 perf.set_mode(UiMode::Project);
 perf.animate(3100);perf.dispatch({ControllerInput::Type::KnobTouch,0,0});
 renderer->draw(project, perf);
 assert(renderer->pixels[0][100 * 480 + 10] != kSentinel);
 assert(renderer->pixels[0][240 * 480 + 10] != kSentinel);
 perf.animate(3300);
 renderer->draw(project, perf);
 assert(renderer->pixels[0][100 * 480 + 10] != kSentinel);
 snap();
 // A fresh renderer drawing the same state once matches exactly.
 auto fresh = std::make_unique<UiRenderer>();
 fresh->draw(project, perf);
 assert(std::memcmp(copy.get(), fresh->pixels, sizeof(uint32_t) * 2 * 480 * 272) == 0);
 // Turning a knob while stopped must update the diagram as well as its value.
 perf.set_mode(UiMode::Routing);perf.soft_key(0);perf.soft_key(3);perf.animate(5000);renderer->draw(project,perf);perf.animate(5300);renderer->draw(project,perf);
 renderer->pixels[0][140*480+100]=kSentinel;assert(perf.knob(7,1000));perf.animate(5400);renderer->draw(project,perf);assert(renderer->pixels[0][140*480+100]!=kSentinel);
 std::printf("partial redraw (band skip, pixel identity) passed\n");
 return 0;
}
