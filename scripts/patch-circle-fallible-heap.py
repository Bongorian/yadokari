#!/usr/bin/env python3
"""Add explicit non-panicking allocation and bucket-aware admission to pinned Circle.

Default Circle allocation semantics are unchanged. These APIs are for fallible
application/file operations only, never the DSP owner.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1])
marker='GROOVE_FALLIBLE_HEAP'
paths=['include/circle/heapallocator.h','lib/heapallocator.cpp','include/circle/memory.h']
texts={p:(root/p).read_text() for p in paths}
if all(marker in s for s in texts.values()):raise SystemExit(0)
if any(marker in s for s in texts.values()):raise SystemExit('Partial fallible heap patch; inspect before rebuilding')
def change(path,old,new):
 s=texts[path]
 if s.count(old)!=1:raise SystemExit('Unexpected pinned Circle anchor in '+path)
 texts[path]=s.replace(old,new)
change(paths[0],'\tvoid *DoAllocate (size_t nSize);','\tvoid *DoAllocate (size_t nSize, bool bFallible = false);')
change(paths[0],'\tvoid *Allocate (size_t nSize);','''\tvoid *Allocate (size_t nSize);
	// GROOVE_FALLIBLE_HEAP: explicit refusal preserves the diagnostic reserve.
	void *TryAllocate (size_t nSize) { return DoAllocate (nSize, true); }
	bool CanAllocate (size_t nSize, unsigned nCount, size_t nHeadroom);
''')
change(paths[1],'void *CHeapAllocator::DoAllocate (size_t nSize)','''// GROOVE_FALLIBLE_HEAP
bool CHeapAllocator::CanAllocate (size_t nSize, unsigned nCount, size_t nHeadroom)
{
	m_SpinLock.Acquire ();
	if (!m_pNext) { m_SpinLock.Release (); return false; }
	for (THeapBlockBucket *p = m_Bucket; p->nSize; ++p)
	{
		if (nSize > p->nSize) continue;
		nSize = p->nSize;
		for (THeapBlockHeader *f = p->pFreeList; f && nCount; f = f->pNext) --nCount;
		break;
	}
	size_t available = m_pLimit - m_pNext;
	bool ok = nSize <= size_t (-1) - sizeof (THeapBlockHeader) - HEAP_BLOCK_ALIGN;
	size_t block = (nSize + sizeof (THeapBlockHeader) + HEAP_BLOCK_ALIGN-1) & ~size_t (HEAP_ALIGN_MASK);
	ok = ok && available >= m_nReserve && available - m_nReserve >= nHeadroom;
	if (ok) ok = !nCount || (block && nCount <= (available-m_nReserve-nHeadroom)/block);
	m_SpinLock.Release ();
	return ok;
}
void *CHeapAllocator::DoAllocate (size_t nSize, bool bFallible)''')
change(paths[1],'\t\t\tif (m_nReserve == 0)','\t\t\tif (bFallible || m_nReserve == 0)')
change(paths[2],'\tstatic void *HeapAllocate (size_t nSize, int nType)','''	// GROOVE_FALLIBLE_HEAP: no logging/allocation in an out-of-memory path.
	static void *HeapTryAllocate (size_t nSize, int nType)
	{
#if RASPPI >= 4
		if (nType == 1) return s_pThis->m_HeapHigh.TryAllocate (nSize);
#endif
		return s_pThis->m_HeapLow.TryAllocate (nSize);
	}
	static bool HeapCanAllocate (size_t nSize, unsigned nCount, size_t nHeadroom, int nType)
	{
#if RASPPI >= 4
		if (nType == 1) return s_pThis->m_HeapHigh.CanAllocate (nSize, nCount, nHeadroom);
#endif
		return s_pThis->m_HeapLow.CanAllocate (nSize, nCount, nHeadroom);
	}
	static void *HeapAllocate (size_t nSize, int nType)''')
for p,s in texts.items():(root/p).write_text(s)
print('Explicit fallible heap and bucket admission patched; default allocator unchanged')
