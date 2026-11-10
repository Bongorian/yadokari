// Optional host probe of the current command ring. Not hardware qualification.
#include "engine.h"
#include "spsc_queue.h"
#include <chrono>
#include <cstdio>
using namespace groove;
using Clock = std::chrono::steady_clock;
template<class F> static double best_ns_per(unsigned bursts, unsigned per, F&& fn) {
 double best = 1e300;
 for (int rep = 0; rep < 9; ++rep) {
  auto t0 = Clock::now(); fn(); auto t1 = Clock::now();
  double ns = double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / (bursts * per);
  if (ns < best) best = ns;
 }
 return best;
}
int main() {
 std::printf("sizeof(CommandState)=%zu\n", sizeof(CommandState));
 CommandState proto{CommandType::SetParameter, 3, 0, 0};
 proto.scene.pattern_for_track[3] = 2; // pattern stays 255 -> fix-up path
 SpscQueue<CommandState, 256> q;
 const unsigned bursts = 60;
 volatile unsigned acc = 0;

 double claim_ns = best_ns_per(bursts, 255, [&] {
  for (unsigned b = 0; b < bursts; ++b) {
   for (unsigned i = 0; i < 255; ++i) { CommandState* s = q.push_begin(); *s = proto; if (s->pattern == 255) s->pattern = 4; q.push_commit(); }
   CommandState c; while (q.pop(c)) acc += c.value;
  }
 });
 std::printf("producer claim+commit (slot copy only): %.1f ns/command\n", claim_ns);
 double submit_ns = best_ns_per(bursts, 255, [&] {
  for (unsigned b = 0; b < bursts; ++b) {
   for (unsigned i = 0; i < 255; ++i) q.push(proto);
   CommandState c; while (q.pop(c)) acc += c.value;
  }
 });
 std::printf("producer push(const&) + pop drain: %.1f ns/command\n", submit_ns);

 double pop_ns = best_ns_per(bursts, 255, [&] {
  for (unsigned b = 0; b < bursts; ++b) {
   for (unsigned i = 0; i < 255; ++i) q.push(proto);
   CommandState c; while (q.pop(c)) acc += c.value;
  }
 });
 std::printf("consumer pop (by value): %.1f ns/command\n", pop_ns);

 double peek_ns = best_ns_per(bursts, 255, [&] {
  for (unsigned b = 0; b < bursts; ++b) {
   for (unsigned i = 0; i < 255; ++i) q.push(proto);
   while (const CommandState* f = q.peek_front()) { acc += f->value; q.pop_advance(); }
  }
 });
 std::printf("consumer peek+advance (in place): %.1f ns/command\n", peek_ns);

 std::printf("acc=%u\n", unsigned(acc));
 return 0;
}
