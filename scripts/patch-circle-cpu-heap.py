#!/usr/bin/env python3
"""CPU-only Pi4/AArch64 arena; preserve Circle's <=3GiB DMA heaps and PCIe alias.

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
if all(marker in s for s in t.values()):raise SystemExit(0)
if any(marker in s for s in t.values()):raise SystemExit('Partial CPU heap patch; inspect before rebuilding')
def change(p,a,b):
 if t[p].count(a)!=1:raise SystemExit('Unexpected pinned Circle anchor: '+p+' '+repr(a))
 t[p]=t[p].replace(a,b)
mp,mh,mc,tt,hh,hc=paths
change(mp,'// PCIe memory range (outbound)', '''// GROOVE_CPU_HEAP_PATCH: explicitly allocated, never submitted to DMA.
// BCM2711 low-peripheral mode reserves FC000000..FFFFFFFF; Circle also
// maps PCIe separately at 0x600000000 in AArch64. Preserve low peripherals.
#if defined(GROOVE_CPU_HEAP)
#if RASPPI != 4 || AARCH != 64 || defined(KASAN_SUPPORTED)
#error GROOVE_CPU_HEAP requires Pi4 AArch64 without KASAN
#endif
#define MEM_CPU_HEAP_START 0xC0000000ULL
#define MEM_CPU_HEAP_END   0xFC000000ULL // exclusive, 960 MiB
#endif

// PCIe memory range (outbound)''')
change(mh,'\t// GROOVE_FALLIBLE_HEAP:', '''	// GROOVE_CPU_HEAP_PATCH: no fallback to an unsafe/default DMA heap.
#define HEAP_CPU 3
#ifdef GROOVE_CPU_HEAP
	static size_t GetCpuMemSize () { return s_pThis->m_nMemSizeCpu; }
#else
	static size_t GetCpuMemSize () { return 0; }
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
		if ((uintptr) pBlock >= MEM_CPU_HEAP_START && (uintptr) pBlock < MEM_CPU_HEAP_END)
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
	CHeapAllocator m_HeapCpu;
#endif''')
# Declaration order is important: CPU allocator precedes low/high members.
change(mc,'\tm_nMemSizeHigh (0),','''	m_nMemSizeHigh (0),
#ifdef GROOVE_CPU_HEAP
	m_nMemSizeCpu (0), m_HeapCpu ("heapcpu"), // GROOVE_CPU_HEAP_PATCH
#endif''')
change(mc,'\tif (nRAMSize > 1024)','''#ifdef GROOVE_CPU_HEAP
	// Only the installed >=4GiB Pi4 exposes this entire physical interval.
	if (nRAMSize >= 4096)
	{
		m_nMemSizeCpu = MEM_CPU_HEAP_END - MEM_CPU_HEAP_START;
		m_HeapCpu.Setup (MEM_CPU_HEAP_START, m_nMemSizeCpu, 0);
	}
#endif
	if (nRAMSize > 1024)''')
change(mc,'\treturn s_pThis->m_nMemSize + s_pThis->m_nMemSizeHigh;','''	return s_pThis->m_nMemSize + s_pThis->m_nMemSizeHigh
#ifdef GROOVE_CPU_HEAP
		+ s_pThis->m_nMemSizeCpu
#endif
		;''')
change(tt,'#include <circle/translationtable64.h>','#include <circle/translationtable64.h>\n#include <circle/machineinfo.h> // GROOVE_CPU_HEAP_PATCH')
change(tt,'\tfor (unsigned nPage = 0;', '''#ifdef GROOVE_CPU_HEAP
	const bool cpuRAM = CMachineInfo::GetRAMSizeEarly () >= 4096;
#endif
	for (unsigned nPage = 0;''')
change(tt,'\t\t\t    || nBaseAddress > MEM_HIGHMEM_END)', '''			    || (nBaseAddress > MEM_HIGHMEM_END
#ifdef GROOVE_CPU_HEAP
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
