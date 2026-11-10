# Allocation policy

One rule for all groove code in `src/` and `firmware/`:

- Every fallible allocation goes through `groove::alloc_new` /
  `groove::alloc_array` (`src/alloc_policy.h`). Failure is reported as
  `nullptr` identically on every target:
  - Hosted builds use nothrow `new`. Plain `new` would throw there, which
    made the null checks already present in the file/session layer
    unreachable on the host and left failure semantics target-dependent.
  - Under Circle, helpers use the explicitly non-panicking
    `CMemorySystem::HeapTryAllocate` API added by the pinned build patch,
    then placement construction. Plain Circle `new` can panic on reserve
    exhaustion and is not a fallible allocation API.
- The caller must check the result before any use and fail the current
  operation cleanly, leaving previously valid state intact. Dereferencing
  an unchecked allocation result is a policy violation. Boot-critical
  allocations in `firmware/kernel.cpp` log an error and halt instead.
- Allocations stay out of the audio callback; the helpers serve the
  control/file/worker paths that already allocate today.
- Release with the matching `delete` / `delete[]` (`alloc_delete` /
  `alloc_delete_array` also null the pointer).
- `src/alloc_policy.h` itself must stay free of C++ standard-library
  includes outside its hosted branch: Circle compiles with `-nostdinc++`,
  and the header is also copied into the Circle tree (next to the MIDI
  DMA buffer helper) by `scripts/patch-circle-midi-cache.py`, where an
  unresolvable include breaks the library build's dependency pass.

## Native RESONATOR / TEXTURE

Per-track wrappers and upstream DSP storage are allocated with `alloc_array`/`alloc_new` during Engine construction, and freed with matching delete. A failed bank allocation disables/rejects that engine rather than dereferencing it. `init`/`process`, input routing and fixed polyphase rate conversion do not allocate. Original texture memory sizes are retained; no lazy DSP allocation is permitted in the audio path.

## Circle reusable heap profile

The repository Circle profile extends reusable buckets through 512 MiB (17
buckets, below Circle's 20-bucket limit). The upstream default stops at 512 KiB:
larger blocks can be allocated but are lost on `delete`. A ProjectState is
46,032,612 bytes, so the default profile exhausts memory during ordinary repeated
save/load operations. The firmware rejects profiles whose largest bucket
cannot contain ProjectState; the build script rebuilds all Circle libraries,
the application and recovery together after profile changes. Persistent
allocations above the largest bucket must never become repeated transient
allocations. `GetHeapFreeSpace` counts the unallocated tail, not reusable free
lists; expect the tail to stabilize after a bounded workflow's first pass.

Plain Circle allocation may panic on reserve exhaustion; the fallible helper
bypasses that path. `allocation_admitted` checks matching reusable buckets plus
the unallocated tail, retaining 8 MiB diagnostic headroom. This is an early
refusal estimate, not a reservation: callers must still handle a failed actual
allocation. Preset attachment Undo holds two compact TrackState + four-pair TrackMorphState
snapshots instead of two PatternState snapshots. SoundFont cache payloads of at least 1 MiB use
high RAM; task-owned read-ahead feeds the decode worker. Resident payloads are capped at 512 MiB
and only unpinned entries may be evicted.

The data-only default reset of a Project does not build the roughly 62 KiB SoundState in an automatic variable and copy it; for trivially-copyable targets it initialises in place with placement construction. Use it from control/file-service code that owns scratch, and re-acquire borrowed views after the reset. TrackSettings inside `make_empty_project` takes the same path. A regression of 128 KiB of thread stack + 56 KiB of caller frame, and the firmware compiler's boot/reset frame gate, detect any reintroduction of large temporaries.

## Fixed procedure for adding an engine or FX

1. Record the state owner, limits and sharing unit in the descriptor and the state codec, and add the object bytes and opaque arena bytes to the audit table. Do not double-count the wrapper's sizeof and the heap arena.
2. Reserve large fixed state fallibly with `alloc_new` / `alloc_array` at construction time, before audio starts. If the full reservation cannot be completed, reject before attachment and keep the previously valid sound and materials. Do not allocate in render / trigger / parameter automation.
3. If introducing lazy allocation at selection time, wire the control-side reserve / fail / cancel / retry through every selection path — morph/lock/automation/MIDI/Undo included — before publishing to the audio side. An implementation that only covers user operations is not acceptable. For this reason Pitch reserves its 96 slots as a single array at construction time. Separate 262464-byte allocations are consolidated into a 32 MiB pool, reducing Circle bucket slack by 16 MiB.
4. For OFF short-circuits in processing that carries seed/phase/tail, verify PCM identity including after resumption. Do not implicitly cut sound quality, voice count, or the original buffers.
5. The gates are the full host inventory, target PCM / non-render allocation checks, 4 MiB + 64 KiB of headroom, the existing boot/codec stack, and the fixed 24 frame / 500 us and UITEST budgets. The canonical allocation inventory is the audit table.

During a snapshot, the presentation pump runs only on the task-side producer copy. It does not call input dispatch, Performance::poll, the file service, or project mutation. Core 2's encode/buffer copy handles immutable input and does not call this pump. This preserves point-in-time snapshot semantics, and is distinct from reducing actual input-acceptance latency.

### SoundFont decode ownership

SoundFont resident request budget is 512 MiB. A PCM16 FluidR3 GM sample block
expands to about 296 MiB of float data, occupying one reusable 512 MiB high-heap
bucket. The common Circle profile includes 256/512 MiB buckets (17 total).
Reservation remains fallible; pinned readers prevent eviction. Failure keeps the
previous instrument. This limit does not promise every possible SF2 will fit.

Browser SF2 and Preset OPEN use the existing asynchronous asset path. Core 2
owns decode/cache mutation while a job runs; core 0 alone services its reads of
at most 64 KiB. Every published read is acknowledged before the worker frees its
scratch, including cancellation. The file owner continues servicing stale jobs
until collected. Cache lookup/mutation is excluded while the loader owns it;
existing sounding voices retain their immutable pinned bank/context. Public
allocation counters use atomic access. No decode, reservation, file operation or
blocking wait runs on the DSP owner. Direct synchronous loader APIs remain for
boot and bounded host/probe callers; they are not Browser's cold-load path.

The AKWF mip bank is expanded fallibly during control initialisation, from 182913 bytes of ROM to 458752 raw bytes. On the Pi it uses a single 512 KiB bucket, and after a successful CRC the shared immutable bank is kept for the lifetime of the process. On failure the unpublished buffer is freed and the operation can be retried; the Engine refuses attachment, treating it as a reservation defect. The net overall saving is the 16 MiB reduction from the Pitch pool minus this 512 KiB and the increments for loader/cursor and the like; do not describe the total heap as shrinking by the full 16 MiB.

The empty SoundState/Layer constants share a single instance and return only borrowed const references. The independence of mutable Track/voice state is maintained. The SF2-dedicated FIL and the general reader's RAII session are owned by the task alone; no FatFs object is handed to a worker. They are closed on every exit path including read failure and cancellation, and the general session is also closed before write/move/remove. 

The LCD coalesce alternate of 61468 bytes is reserved in control before the worker starts (a 64 KiB bucket on the Pi). Diagnostic traces are reserved fallibly in control: 8192 requests, 4096 frames, 8192 jobs, 4096 tasks of 100 us or more, and 512 slow-audio entries (not reserved on a normal boot). They also pass the admission hook; on failure coalesce is not used, and missing trace data is never read as a pass. No allocation is added inside the worker/audio paths. The VA template/math/track resolution caches are fixed state inside the Engine and do not share mutable per-voice DSP state.

The attribution ledger is reserved by the task before UITEST starts, and the DSP/worker side writes into the existing arrays. For slow audio, the task reads only after the DSP acknowledges the seal request. The detailed sample-loop clock is enabled only in dedicated builds; normal firmware keeps the coarse command/reset/control/total plus the standard DspProfile.

At boot, the Factory LIBRARY reserves an expansion scratch of at most 86943 bytes fallibly in control, reuses it across all 103 masters, and releases it (the current Circle profile has a 256 KiB bucket). Expansion/SHA/file processing happens only before audio starts. No external PCM/SF2 is included in ROM, and the sharing of existing DATA materials and the refusal on missing attachment are maintained.

The SessionFiles body and the large file-scope PreservedStore are held in fallible heap ownership so that the sidecar tables do not overlap on Circle's 128 KiB caller stack. Unknown-definition records are allocated only on the file-service side and referenced from Undo through a runtime cookie. The storage cap is 2048 entries / 8 MiB, and the previous generation is freed when the Project is exchanged. The material ROOT/BPM branches add a separate binding to the DSP-owned immutable WavView without allocating new PCM.

## Pi 4 CPU-only heap

On an AArch64 Pi 4 with 4 GiB or more, HEAP_CPU can be used explicitly. 960 MiB at C0000000..FBFFFFFF is identity-mapped as normal write-back / inner-shareable / NX, and the DMA uses of LOW/HIGH/HEAP_ANY/default malloc are unchanged. The checked root allocation in `firmware/cpu_memory.h` allocates fallibly in the order CPU → HIGH → LOW, and the global delete/realloc returns memory to the owning heap. No allocation is added inside render.

WAV completes I/O in LOW and then copies to CPU (both the synchronous and incremental paths). SoundFont direct-read payloads stay in HIGH. CPU spans of the general file API pass through a LOW bounce of at most 64 KiB, and CPU addresses are never handed to DMA. The CPU heap rewinds only when the most recent tail can be freed; otherwise memory is returned to the conventional buckets. A private RAM check frees only the blocks it owns, in reverse order, with a full 768 MiB read-back and tail restoration. The headroom display shows the unallocated tail and therefore differs from live usage.
