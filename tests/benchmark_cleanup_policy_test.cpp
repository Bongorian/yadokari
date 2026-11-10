#include "benchmark_cleanup_policy.h"
#include <cassert>
int main(){using namespace groove;
 assert(benchmark_keep_file("GROOVE/CURRENT.TXT"));
 assert(benchmark_keep_file("BENCH.MGXS") && benchmark_keep_file("remote.on"));
 assert(benchmark_keep_file("assets/samples/demo-slices.wav"));
 assert(benchmark_keep_file("assets/factory/808-KICK.WAV"));
 assert(benchmark_keep_file("assets/presets/VA-Bass.mgxp"));
 assert(!benchmark_keep_file("BENCHOLD.MGX") && !benchmark_keep_file("BENCH.MGXS.bak"));
 assert(!benchmark_keep_file("GROOVE/SESS.MGX") && !benchmark_keep_file("assets/demo-projects/factory-session.mgx"));
 assert(!benchmark_keep_file("assets/samples/user.wav") && !benchmark_keep_file("assets/presets/user.mgx"));
 assert(benchmark_keep_directory("assets") && benchmark_keep_directory("assets/samples"));
 assert(!benchmark_keep_directory("assets/demo-projects") && benchmark_keep_directory("GROOVE"));
 assert(benchmark_keep_subtree("firmware") && !benchmark_keep_subtree("firmware-old"));
}
