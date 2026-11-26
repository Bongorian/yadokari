#pragma once
#include <stdint.h>
#include <stddef.h>
namespace groove { namespace hardware {
// Boot-time hardware profile (PI5_SUPPORT_POLICY.md + 2026-11-12 addendum):
// optimization and performance scale by board type AND installed RAM, both
// decoded from the board revision code the firmware already reads via the
// mailbox (CMachineInfo::GetRevisionRaw() on device; the pure decode here
// is host-testable and shared by every target).
//
// The baseline is the Pi 4 with 4 GB: every guarantee is defined there and
// its behaviour is bit-identical to the pre-profile build -- the arena
// capacity below equals the existing Pi 4 CPU-heap window and every budget
// factor is 1.0. Cost unit prices (EnginePolyphony) stay common on all
// boards; only budget-side values scale. Layer admission scales with the
// same factor (perf port, decision A 2026-11-15): a layer admitted on a
// larger board may be refused on the Pi 4 baseline. That asymmetry is
// accepted and documented as a known constraint -- the same class as the
// shared-budget scaling -- rather than pinning admission to the floor.

inline constexpr unsigned kPi5BudgetScale=2984;

enum class Board:uint8_t { Unknown, Pi4B, Pi5 };

struct HardwareProfile {
 Board board=Board::Unknown;
 unsigned ram_mb=0;                    // decoded installed RAM (0 = unknown)
 uint32_t cpu_heap_bytes=0;             // CPU-only arena capacity (RAM-driven)
 unsigned budget_scale_permille=1000;   // voice/shared budget factor (board-driven)
 bool baseline=true;                    // guarantees are defined on this profile
};

// CPU-heap arena capacity for every supported (>= 4 GB) board. The Pi 4's
// fixed window is capped at 960 MiB by the BCM2711 low-peripheral hole at
// 0xFC000000, and the firmware's CPU-memory check (CCpuMemoryCheck) is
// sized for exactly that span (15 x 64 MiB), so the capacity is held at
// the baseline value on Pi 5 as well until on-device measurement justifies
// a change; this constant is the one-line edit when it does.
constexpr uint32_t kCpuHeapBytes=960u*1024u*1024u;

// RAM size codes (revision bits 20-22): 0=256MB, 1=512MB, 2=1GB, 3=2GB,
// 4=4GB, 5=8GB, 6=16GB, 7=32GB.
inline unsigned revision_ram_mb(uint32_t revision){return 256u<<((revision>>20)&7u);}

// Pure decode of the new-style revision encoding (bit 23 set; board type
// in bits 4-11: 0x11 = Pi 4B, 0x17 = Pi 5). Anything not positively
// identified fails safe to the baseline profile.
inline HardwareProfile decode(uint32_t revision){
 HardwareProfile p;
 p.cpu_heap_bytes=kCpuHeapBytes;
 if(!(revision&(1u<<23)))return p;
 const unsigned type=(revision>>4)&0xffu;
 if(type==0x11)p.board=Board::Pi4B;
 else if(type==0x17)p.board=Board::Pi5;
 else return p;
 p.ram_mb=revision_ram_mb(revision);
 p.baseline=p.board==Board::Pi4B&&p.ram_mb==4096;
 // MEASURED on Pi 5 (DSPTEST 2 runs, worst-across-runs, safety x1.25;
 // binding input: Plaits model 18 per-voice cost). Derivation:
 // PI5_PERF_PROFILE.md S9-S10. Pi 4 keeps 1000 (baseline definition).
 p.budget_scale_permille=1000;
 if(p.board==Board::Pi5)p.budget_scale_permille=kPi5BudgetScale;
 // RAM-scaled arena (perf port; verified on device 2026-11-15:
 // cpu_ram_mb=2048, heap headroom 1979 MB): Pi 5 / 8 GB carves 2 GiB;
 // the 4 GB floor and all Pi 4 profiles keep the 960 MiB baseline. The
 // real carve is decided in scripts/patch-circle-cpu-heap.py by the
 // same rule; this value is the reported profile and must agree.
 if(p.board==Board::Pi5&&p.ram_mb>=8192)p.cpu_heap_bytes=2048u*1024u*1024u;
 if(p.ram_mb<4096){
  // Below the supported minimum (Pi 4 with 4 GB or more): no arena.
  p.cpu_heap_bytes=0;
 }else if(p.board==Board::Pi5&&p.ram_mb>8192){
  // Circle maps at most 8 GiB of DRAM, so a top-of-DRAM arena has no
  // mapped home on a 16 GB Pi 5 yet; report no arena and allocations
  // fall back to the high heap, exactly like the pre-arena Pi 5 build.
  p.cpu_heap_bytes=0;
 }
 return p;
}

// The active profile. Firmware glue sets it once at boot, before the
// engine is allocated; until then -- and on hosted builds, which never
// set it -- the baseline profile applies, so every decision made without
// a decoded board is identical to the pre-profile build.
inline HardwareProfile active_profile=decode(0);
inline void set_active_profile(const HardwareProfile& p){active_profile=p;}

// Budget-side scaling: budget * scale, integer-exact at the shipped
// factor (1000 permille returns the input unchanged).
inline unsigned scaled(unsigned value){
 return unsigned((uint64_t(value)*active_profile.budget_scale_permille)/1000u);
}
// Per-model board-specific Plaits cost tables were measured on Pi 5
// (DSPTEST v1016 factors) and prototyped, but showed no measurable
// effect in two A/B rounds (L3 identical to L2 on every stress case),
// so they are deliberately NOT ported (decision B, 2026-11-15). Cost
// unit prices stay common on all boards.
// RAM-scaled SoundFont resident budget (perf port). Baseline
// 512 MiB everywhere (the 4 GB floor is unchanged); Pi 5 / 8 GB gets
// 1536 MiB so GB-class fonts stay resident instead of evicting.
inline size_t sf2_budget_bytes(){
 return (active_profile.board==Board::Pi5&&active_profile.ram_mb>=8192)?size_t(1536)*1024*1024:size_t(512)*1024*1024;
}
// Slot counts additionally cap at the physical pool capacity.
inline unsigned scaled_slots(unsigned slots,unsigned capacity){
 const unsigned s=scaled(slots);return s<capacity?s:capacity;
}
}}
