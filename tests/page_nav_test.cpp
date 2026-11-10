// Stage 1 navigation test (SPEC 2.4/2.6): the previously dead PAGE
// buttons drive EditPage Back/Forward, a both-buttons long-press is
// HOME (to Pad, storing a return ticket outside the history) and
// RETURN on Pad, the ticket is discarded by mode navigation, a lone
// long-press never fires HOME, and a pending project confirm (the
// Modal) suppresses HOME. Driven through the real control() byte
// stream: arrow_right = byte 7 mask 0x04, arrow_left = byte 8 mask
// 0x08 (src/mk3_surface.h).
#include "performance.h"
#include <cassert>
#include <cstdio>
using namespace groove;
int main() {
 Engine engine(48000);
 ProjectState project; make_empty_project(project);
 Performance perf(engine, project);
 uint64_t t = 1000;
 auto tick = [&](uint64_t dt) { t += dt; perf.animate(t); };
 auto both_home = [&]() {
  perf.control(7, 0x04); perf.control(8, 0x08); tick(450);
  perf.control(7, 0x00); perf.control(8, 0x00);
 };
 // History: Pad -> Step -> Synth.
 perf.set_mode(UiMode::Step);
 perf.set_mode(UiMode::Synth);
 assert(perf.mode() == UiMode::Synth);
 assert(perf.axes().history.can_back());
 // PAGE LEDs report history availability (offsets 6/7).
 uint8_t leds[62]; perf.button_leds(leds);
 assert(leds[6] != 0 && leds[7] == 0);
 // Back: press PAGE-left and let the grace period elapse.
 perf.control(8, 0x08); tick(100);
 assert(perf.mode() == UiMode::Step);
 perf.control(8, 0x00);
 // Forward: a quick tap fires on release, before the grace period.
 perf.control(7, 0x04); perf.control(7, 0x00);
 assert(perf.mode() == UiMode::Synth);
 // HOME: both held -> Pad, ticket stored.
 both_home();
 assert(perf.mode() == UiMode::Pad);
 assert(perf.return_ticket_valid());
 // RETURN: same gesture on Pad -> back to Synth, ticket consumed.
 both_home();
 assert(perf.mode() == UiMode::Synth);
 assert(!perf.return_ticket_valid());
 // Ticket is discarded by mode-button navigation to another page.
 both_home();
 assert(perf.mode() == UiMode::Pad && perf.return_ticket_valid());
 perf.set_mode(UiMode::Mixer);
 assert(!perf.return_ticket_valid());
 // A lone long-press fires Back once and never HOME.
 perf.control(8, 0x08); tick(1000);
 assert(perf.mode() == UiMode::Synth);
 assert(!perf.return_ticket_valid());
 perf.control(8, 0x00);
 // Modal gate: a pending project confirm suppresses HOME.
 perf.set_mode(UiMode::Project);
 perf.soft_key(2); // request New -> confirm pending
 both_home();
 assert(perf.mode() == UiMode::Project);
 assert(!perf.return_ticket_valid());
 perf.encoder_direction(0x20); // cancel the pending action
 std::printf("page nav (Back/Forward, HOME/RETURN ticket, modal gate) passed\n");
 // W5: Perform subpages are separate visits and restore focus/cursor.
 perf.set_mode(UiMode::Perform);perf.encoder_direction(0x08);assert(perf.performance_page()==1);
 perf.set_focus(Focus::Fx);perf.set_mode(UiMode::Synth);assert(perf.page_back());assert(perf.mode()==UiMode::Perform && perf.performance_page()==1 && perf.axes().focus==Focus::Fx);
 assert(perf.home_or_return());assert(perf.home_or_return());assert(perf.mode()==UiMode::Perform && perf.performance_page()==1 && perf.axes().focus==Focus::Fx);
 return 0;
}
