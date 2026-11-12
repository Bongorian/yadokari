#!/usr/bin/env python3
"""CPU-only Pi4/Pi5 AArch64 arena; preserve Circle's DMA heaps and PCIe alias.

Pi 4 uses the fixed window above its (deliberately capped) high heap;
Pi 5 carves the same baseline capacity out of the top of its high heap.
Apply after patch-circle-fallible-heap.py to the pinned checkout. HEAP_ANY and
all default allocators deliberately exclude this arena. Global free/realloc
route explicit CPU allocations back to their owner.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1])
paths=['include/circle/memorymap64.h','include/circle/memory.h','lib/memory64.cpp','lib/translationtable64.cpp','include/circle/heapallocator.h','lib/heapallocator.cpp']
t={p:(root/p).read_text() for p in paths}
marker='GROOVE_CPU_HEAP_PATCH'
mp,mh,mc,tt,hh,hc=paths
def current_shape():
 return ('MEM_CPU_HEAP_SIZE' in t[mp] and 'IsCpuHeapPointer' in t[mh]
  and 'm_nCpuHeapStart' in t[mh] and 'm_nCpuHeapStart' in t[mc])
# A tree patched by an older version of this script carries the marker in
# all six files without the current definitions (0.9.0: no Pi 5 arena, no
# IsCpuHeapPointer). That shape must migrate, never silently skip: the
# (stale, current) replacement pairs below are exactly the difference
# between the two applied shapes, verified against both.
migration=[
('include/circle/memorymap64.h','#if RASPPI != 4 || AARCH != 64 || defined(KASAN_SUPPORTED)\n#error GROOVE_CPU_HEAP requires Pi4 AArch64 without KASAN\n#endif\n#define MEM_CPU_HEAP_START 0xC0000000ULL\n#define MEM_CPU_HEAP_END   0xFC000000ULL // exclusive, 960 MiB\n','#if (RASPPI != 4 && RASPPI != 5) || AARCH != 64 || defined(KASAN_SUPPORTED)\n#error GROOVE_CPU_HEAP requires Pi4/Pi5 AArch64 without KASAN\n#endif\n#if RASPPI == 4\n#define MEM_CPU_HEAP_START 0xC0000000ULL\n#define MEM_CPU_HEAP_END   0xFC000000ULL // exclusive, 960 MiB\n#elif RASPPI == 5\n// BCM2712 places no peripheral hole inside DRAM, so instead of a fixed\n// window the arena is the top MEM_CPU_HEAP_SIZE bytes of installed DRAM,\n// carved out of the high heap in CMemorySystem::SetupHighMem. Only the\n// capacity is fixed here, at the Pi 4 baseline value; keep it equal to\n// groove::hardware::kCpuHeapBytes (src/hardware_profile.h).\n#define MEM_CPU_HEAP_SIZE  (960ULL * MEGABYTE)\n#endif\n'),
('include/circle/memory.h','#endif\n\t// GROOVE_FALLIBLE_HEAP: no logging/allocation in an out-of-memory path.\n','#endif\n#ifdef GROOVE_CPU_HEAP\n\tstatic boolean IsCpuHeapPointer (const void *pBlock)\n\t{\n#if RASPPI == 5\n\t\treturn (uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu;\n#else\n\t\treturn (uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END;\n#endif\n\t}\n#endif\n\t// GROOVE_FALLIBLE_HEAP: no logging/allocation in an out-of-memory path.\n'),
('include/circle/memory.h','#ifdef GROOVE_CPU_HEAP\n\t\tif ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)\n\t\t{ return s_pThis->m_HeapCpu.ReAllocate (pBlock, nSize); }\n','#ifdef GROOVE_CPU_HEAP\n#if RASPPI == 5\n\t\tif ((uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu)\n#else\n\t\tif ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)\n#endif\n\t\t{ return s_pThis->m_HeapCpu.ReAllocate (pBlock, nSize); }\n'),
('include/circle/memory.h','#ifdef GROOVE_CPU_HEAP\n\t\tif ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)\n\t\t{ if (!s_pThis->m_HeapCpu.TryFreeTail (pBlock)) s_pThis->m_HeapCpu.Free (pBlock); return; }\n','#ifdef GROOVE_CPU_HEAP\n#if RASPPI == 5\n\t\tif ((uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu)\n#else\n\t\tif ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)\n#endif\n\t\t{ if (!s_pThis->m_HeapCpu.TryFreeTail (pBlock)) s_pThis->m_HeapCpu.Free (pBlock); return; }\n'),
('include/circle/memory.h','\tsize_t m_nMemSizeCpu;\n','\tsize_t m_nMemSizeCpu;\n#if RASPPI == 5\n\tuintptr m_nCpuHeapStart;\n#endif\n'),
('lib/memory64.cpp','\tm_nMemSizeCpu (0), m_HeapCpu ("heapcpu"), // GROOVE_CPU_HEAP_PATCH\n','\tm_nMemSizeCpu (0),\n#if RASPPI == 5\n\tm_nCpuHeapStart (0),\n#endif\n\tm_HeapCpu ("heapcpu"), // GROOVE_CPU_HEAP_PATCH\n'),
('lib/memory64.cpp','#ifdef GROOVE_CPU_HEAP\n\t// Only the installed >=4GiB Pi4 exposes this entire physical interval.\n','#ifdef GROOVE_CPU_HEAP\n#if RASPPI == 4\n\t// Only the installed >=4GiB Pi4 exposes this entire physical interval.\n'),
('lib/memory64.cpp','\t\tm_HeapCpu.Setup (MEM_CPU_HEAP_START, m_nMemSizeCpu, 0);\n\t}\n#endif\n',"\t\tm_HeapCpu.Setup (MEM_CPU_HEAP_START, m_nMemSizeCpu, 0);\n\t}\n#elif RASPPI == 5\n\t// BCM2712: decide the arena capacity here; the top slice of the\n\t// high heap's window is carved out and set up below, so no DRAM is\n\t// owned by two allocators. 16 GiB boards keep the pre-arena\n\t// behaviour: Circle maps at most 8 GiB (src/hardware_profile.h).\n\tif (nRAMSize >= 4096 && nRAMSize <= 8192)\n\t{\n\t\tm_nMemSizeCpu = MEM_CPU_HEAP_SIZE;\n\t}\n#endif\n#endif\n"),
('lib/memory64.cpp','\t\t}\n',"\t\t}\n#if defined(GROOVE_CPU_HEAP) && RASPPI == 5\n\t\t// The CPU arena takes the top slice of the high heap's window.\n\t\tif (m_nMemSizeCpu)\n\t\t{\n\t\t\tif (nHighSize > m_nMemSizeCpu) nHighSize -= m_nMemSizeCpu;\n\t\t\telse m_nMemSizeCpu = 0;\n\t\t}\n#endif\n"),
('lib/memory64.cpp','\t\tm_HeapHigh.Setup (MEM_HIGHMEM_START, (size_t) nHighSize, 0);\n','\t\tm_HeapHigh.Setup (MEM_HIGHMEM_START, (size_t) nHighSize, 0);\n#if defined(GROOVE_CPU_HEAP) && RASPPI == 5\n\t\tif (m_nMemSizeCpu)\n\t\t{\n\t\t\tm_nCpuHeapStart = MEM_HIGHMEM_START + m_nMemSizeHigh;\n\t\t\tm_HeapCpu.Setup (m_nCpuHeapStart, m_nMemSizeCpu, 0);\n\t\t}\n#endif\n'),
('lib/translationtable64.cpp','\n#ifdef GROOVE_CPU_HEAP\n\tconst bool cpuRAM = CMachineInfo::GetRAMSizeEarly () >= 4096;\n','\n#if defined(GROOVE_CPU_HEAP) && RASPPI == 4\n\tconst bool cpuRAM = CMachineInfo::GetRAMSizeEarly () >= 4096;\n'),
('lib/translationtable64.cpp','\t\t\t    || (nBaseAddress > MEM_HIGHMEM_END\n#ifdef GROOVE_CPU_HEAP\n','\t\t\t    || (nBaseAddress > MEM_HIGHMEM_END\n#if defined(GROOVE_CPU_HEAP) && RASPPI == 4\n'),
]
if all(marker in s for s in t.values()):
 if current_shape():raise SystemExit(0)
 if all(t[p].count(a)==1 for p,a,b in migration):
  for p,a,b in migration:t[p]=t[p].replace(a,b)
  if not current_shape():raise SystemExit('CPU heap patch migration did not reach the current shape; inspect before rebuilding')
  for p,s in t.items():(root/p).write_text(s)
  print('CPU heap patch migrated from the 0.9.0 shape to the current Pi4/Pi5 shape')
  raise SystemExit(0)
 raise SystemExit('Unrecognised CPU heap patch state: marker present but neither the 0.9.0 nor the current shape; reset the Circle checkout to the pinned revision and rebuild')
if any(marker in s for s in t.values()):raise SystemExit('Partial CPU heap patch; inspect before rebuilding')
def change(p,a,b):
 if t[p].count(a)!=1:raise SystemExit('Unexpected pinned Circle anchor: '+p+' '+repr(a))
 t[p]=t[p].replace(a,b)
change(mp,'// PCIe memory range (outbound)', '''// GROOVE_CPU_HEAP_PATCH: explicitly allocated, never submitted to DMA.
// BCM2711 low-peripheral mode reserves FC000000..FFFFFFFF; Circle also
// maps PCIe separately at 0x600000000 in AArch64. Preserve low peripherals.
#if defined(GROOVE_CPU_HEAP)
#if (RASPPI != 4 && RASPPI != 5) || AARCH != 64 || defined(KASAN_SUPPORTED)
#error GROOVE_CPU_HEAP requires Pi4/Pi5 AArch64 without KASAN
#endif
#if RASPPI == 4
#define MEM_CPU_HEAP_START 0xC0000000ULL
#define MEM_CPU_HEAP_END   0xFC000000ULL // exclusive, 960 MiB
#elif RASPPI == 5
// BCM2712 places no peripheral hole inside DRAM, so instead of a fixed
// window the arena is the top MEM_CPU_HEAP_SIZE bytes of installed DRAM,
// carved out of the high heap in CMemorySystem::SetupHighMem. Only the
// capacity is fixed here, at the Pi 4 baseline value; keep it equal to
// groove::hardware::kCpuHeapBytes (src/hardware_profile.h).
#define MEM_CPU_HEAP_SIZE  (960ULL * MEGABYTE)
#endif
#endif

// PCIe memory range (outbound)''')
change(mh,'\t// GROOVE_FALLIBLE_HEAP:', '''	// GROOVE_CPU_HEAP_PATCH: no fallback to an unsafe/default DMA heap.
#define HEAP_CPU 3
#ifdef GROOVE_CPU_HEAP
	static size_t GetCpuMemSize () { return s_pThis->m_nMemSizeCpu; }
#else
	static size_t GetCpuMemSize () { return 0; }
#endif
#ifdef GROOVE_CPU_HEAP
	static boolean IsCpuHeapPointer (const void *pBlock)
	{
#if RASPPI == 5
		return (uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu;
#else
		return (uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END;
#endif
	}
#endif
	// GROOVE_FALLIBLE_HEAP:''')
for name in ['HeapTryAllocate','HeapCanAllocate']:
 method='TryAllocate (nSize)' if name=='HeapTryAllocate' else 'CanAllocate (nSize, nCount, nHeadroom)'
 start=t[mh].index('\tstatic '+('void *' if name=='HeapTryAllocate' else 'bool ')+name)
 end=t[mh].index('\n\t}',start)
 piece=t[mh][start:end]
 a='#if RASPPI >= 4'
 b='''#ifdef GROOVE_CPU_HEAP
		if (nType == HEAP_CPU) return s_pThis->m_HeapCpu.'''+method+''';
#else
		if (nType == HEAP_CPU) return '''+('0' if name=='HeapTryAllocate' else 'false')+''';
#endif
#if RASPPI >= 4'''
 assert piece.count(a)==1
 t[mh]=t[mh][:start]+piece.replace(a,b)+t[mh][end:]
change(mh,'\t\tcase HEAP_LOW:\treturn s_pThis->m_HeapLow.Allocate (nSize);','''#ifdef GROOVE_CPU_HEAP
		case HEAP_CPU: return s_pThis->m_HeapCpu.Allocate (nSize);
#endif
		case HEAP_LOW:	return s_pThis->m_HeapLow.Allocate (nSize);''')
# There is one pointer test in each routing function.
for name,body in [('HeapReAllocate','return s_pThis->m_HeapCpu.ReAllocate (pBlock, nSize);'),('HeapFree','if (!s_pThis->m_HeapCpu.TryFreeTail (pBlock)) s_pThis->m_HeapCpu.Free (pBlock); return;')]:
 start=t[mh].index('\tstatic '+('void *' if name=='HeapReAllocate' else 'void ')+name)
 end=t[mh].index('\n\t}',start)
 piece=t[mh][start:end]
 a='#if RASPPI >= 4'
 b='''#ifdef GROOVE_CPU_HEAP
#if RASPPI == 5
		if ((uintptr) pBlock >= s_pThis->m_nCpuHeapStart && (uintptr) pBlock < s_pThis->m_nCpuHeapStart + s_pThis->m_nMemSizeCpu)
#else
		if ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)
#endif
		{ '''+body+''' }
#endif
#if RASPPI >= 4'''
 assert piece.count(a)==1
 t[mh]=t[mh][:start]+piece.replace(a,b)+t[mh][end:]
change(mh,'\t\tcase HEAP_LOW:\treturn s_pThis->m_HeapLow.GetFreeSpace ();','''#ifdef GROOVE_CPU_HEAP
		case HEAP_CPU: return s_pThis->m_HeapCpu.GetFreeSpace ();
#endif
		case HEAP_LOW:	return s_pThis->m_HeapLow.GetFreeSpace ();''')
change(mh,'\tsize_t m_nMemSizeHigh;','''	size_t m_nMemSizeHigh;
#ifdef GROOVE_CPU_HEAP
	size_t m_nMemSizeCpu;
#if RASPPI == 5
	uintptr m_nCpuHeapStart;
#endif
	CHeapAllocator m_HeapCpu;
#endif''')
# Declaration order is important: CPU allocator precedes low/high members.
change(mc,'\tm_nMemSizeHigh (0),','''	m_nMemSizeHigh (0),
#ifdef GROOVE_CPU_HEAP
	m_nMemSizeCpu (0),
#if RASPPI == 5
	m_nCpuHeapStart (0),
#endif
	m_HeapCpu ("heapcpu"), // GROOVE_CPU_HEAP_PATCH
#endif''')
change(mc,'\tif (nRAMSize > 1024)','''#ifdef GROOVE_CPU_HEAP
#if RASPPI == 4
	// Only the installed >=4GiB Pi4 exposes this entire physical interval.
	if (nRAMSize >= 4096)
	{
		m_nMemSizeCpu = MEM_CPU_HEAP_END - MEM_CPU_HEAP_START;
		m_HeapCpu.Setup (MEM_CPU_HEAP_START, m_nMemSizeCpu, 0);
	}
#elif RASPPI == 5
	// BCM2712: decide the arena capacity here; the top slice of the
	// high heap's window is carved out and set up below, so no DRAM is
	// owned by two allocators. 16 GiB boards keep the pre-arena
	// behaviour: Circle maps at most 8 GiB (src/hardware_profile.h).
	if (nRAMSize >= 4096 && nRAMSize <= 8192)
	{
		m_nMemSizeCpu = MEM_CPU_HEAP_SIZE;
	}
#endif
#endif
	if (nRAMSize > 1024)''')
change(mc,'\t\t\tnHighSize = MEM_HIGHMEM_END+1 - MEM_HIGHMEM_START;\n\t\t}','''			nHighSize = MEM_HIGHMEM_END+1 - MEM_HIGHMEM_START;
		}
#if defined(GROOVE_CPU_HEAP) && RASPPI == 5
		// The CPU arena takes the top slice of the high heap's window.
		if (m_nMemSizeCpu)
		{
			if (nHighSize > m_nMemSizeCpu) nHighSize -= m_nMemSizeCpu;
			else m_nMemSizeCpu = 0;
		}
#endif''')
change(mc,'\t\tm_HeapHigh.Setup (MEM_HIGHMEM_START, (size_t) nHighSize, 0);','''		m_HeapHigh.Setup (MEM_HIGHMEM_START, (size_t) nHighSize, 0);
#if defined(GROOVE_CPU_HEAP) && RASPPI == 5
		if (m_nMemSizeCpu)
		{
			m_nCpuHeapStart = MEM_HIGHMEM_START + m_nMemSizeHigh;
			m_HeapCpu.Setup (m_nCpuHeapStart, m_nMemSizeCpu, 0);
		}
#endif''')
change(mc,'\treturn s_pThis->m_nMemSize + s_pThis->m_nMemSizeHigh;','''	return s_pThis->m_nMemSize + s_pThis->m_nMemSizeHigh
#ifdef GROOVE_CPU_HEAP
		+ s_pThis->m_nMemSizeCpu
#endif
		;''')
change(tt,'#include <circle/translationtable64.h>','#include <circle/translationtable64.h>\n#include <circle/machineinfo.h> // GROOVE_CPU_HEAP_PATCH')
change(tt,'\tfor (unsigned nPage = 0;', '''#if defined(GROOVE_CPU_HEAP) && RASPPI == 4
	const bool cpuRAM = CMachineInfo::GetRAMSizeEarly () >= 4096;
#endif
	for (unsigned nPage = 0;''')
change(tt,'\t\t\t    || nBaseAddress > MEM_HIGHMEM_END)', '''			    || (nBaseAddress > MEM_HIGHMEM_END
#if defined(GROOVE_CPU_HEAP) && RASPPI == 4
			        && !(cpuRAM && nBaseAddress >= MEM_CPU_HEAP_START && nBaseAddress < MEM_CPU_HEAP_END)
#endif
			       ))''')
# Tail release is opt-in for CPU ownership; defaults and DMA heaps are unchanged.
change(hh,'\tvoid Free (void *pBlock);','''\t// GROOVE_CPU_HEAP_PATCH: rewind a still-allocated tail, not a freelist.
\tbool TryFreeTail (void *pBlock);
\tvoid Free (void *pBlock);''')
change(hc,'void CHeapAllocator::Free (void *pBlock)','''// GROOVE_CPU_HEAP_PATCH
bool CHeapAllocator::TryFreeTail (void *pBlock)
{
\tif (!pBlock) return true;
\tauto *h = (THeapBlockHeader *) ((uintptr) pBlock - sizeof (THeapBlockHeader));
\tm_SpinLock.Acquire ();
\tassert (h->nMagic == HEAP_BLOCK_ALLOC_MAGIC);
\tsize_t span = (sizeof (THeapBlockHeader) + size_t (h->nSize) + HEAP_BLOCK_ALIGN-1) & ~size_t (HEAP_ALIGN_MASK);
\tif ((u8 *) h + span != m_pNext) { m_SpinLock.Release (); return false; }
\tm_pNext = (u8 *) h;
\th->nMagic = 0;
#ifdef HEAP_DEBUG
\tfor (THeapBlockBucket *p = m_Bucket; p->nSize; ++p)
\t\tif (p->nSize == h->nSize) { --p->nCount; break; }
#endif
\tm_SpinLock.Release ();
\treturn true;
}
void CHeapAllocator::Free (void *pBlock)''')
for p,s in t.items():(root/p).write_text(s)
print('Pi4 explicit CPU-only heap patched; DMA heaps and MMIO aliases unchanged')
