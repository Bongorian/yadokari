#pragma once
#include <stdint.h>
namespace groove {
// A shared-pool voice's random stream is a function of the note event that
// started it: the project seed, the track, the event's musical position
// (the track's step counter at trigger time), its pitch, and the ordinal
// among same-pitch note-ons at that position on that track (so layered
// duplicates of one pitch still differ, while the seed is invariant
// under any permutation of distinct-pitch triggers). It never depends
// on the pool
// slot the voice happened to land in, on that slot's history, or on
// processing time (sample_count_ and friends are not seed material).
// The mix follows step_probability_draw in generative.h.
inline uint32_t note_event_seed(uint32_t project_seed,unsigned track,uint64_t tick,unsigned pitch,unsigned ordinal){
 uint32_t x=project_seed^(0x9e3779b9u*(track+1))^(0x85ebca6bu*(pitch+1))^(0x27d4eb2fu*(ordinal+1))^uint32_t(tick)^(uint32_t(tick>>32)*0xc2b2ae35u);
 x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
 return x?x:0x7319ab35u;
}
}
