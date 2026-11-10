# Copy to Config.mk only in a fresh, isolated Circle checkout.
# Every library and firmware object must use this same memory layout.
DEFINE += -DGROOVE_CPU_HEAP -DARM_ALLOW_MULTI_CORE -DGROOVE_MULTICORE_DISPLAY -DGROOVE_SINGLE_CORE_AUDIO_IRQ -DKERNEL_MAX_SIZE=0x400000
# Circle cannot recycle allocations larger than its largest bucket.
# ProjectState (~30 MiB), codec buffers and sample caches must be reusable.
DEFINE += -DHEAP_BLOCK_BUCKET_SIZES=0x40,0x400,0x1000,0x4000,0x10000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000

# One target-wide code-cache budget. Bodies stay at O3; avoid cloning large
# UI/model orchestrators into each caller. Explicit always-inline DSP remains inline.
OPTIMIZE = -O3
GC_SECTIONS = 1
CFLAGS += -finline-limit=24 --param=inline-unit-growth=5 --param=inline-heuristics-hint-percent=100 --param=inline-min-speedup=100 -fno-ipa-cp-clone -fno-partial-inlining -fno-inline-functions-called-once
