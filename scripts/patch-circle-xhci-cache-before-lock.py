#!/usr/bin/env python3
"""Move USB buffer cache maintenance before the producer IRQ lock.

The caller must keep its request buffer stable for the duration of submission,
including concurrent submissions; no cache work is done for zero-length TDs.
"""
from pathlib import Path
import sys
path=Path(sys.argv[1])/'lib/usb/xhciendpoint.cpp';s=path.read_text();marker='GROOVE_XHCI_CACHE_BEFORE_LOCK'
if marker in s:
 print('Circle xHCI buffer cache maintenance already outside producer lock');raise SystemExit(0)
start=s.index('boolean CXHCIEndpoint::TransferAsync');end=s.index('\nvoid CXHCIEndpoint::TransferEvent',start);part=s[start:end]
anchor='\tu32 nBufLen = pURB->GetBufLen ();\n\n\t// GROOVE_XHCI_PREFLIGHT'
if part.count(anchor)!=1:raise SystemExit('Unexpected xHCI cache anchor')
part=part.replace(anchor,'''\tu32 nBufLen = pURB->GetBufLen ();

\t// GROOVE_XHCI_CACHE_BEFORE_LOCK: the caller owns this buffer during
\t// submission; avoid holding the producer IRQ lock for cache maintenance.
\tif (pBuffer && nBufLen)
\t{
\t\tassert ((uintptr) pBuffer > MEM_KERNEL_END);
\t\tCleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);
\t}

\t// GROOVE_XHCI_PREFLIGHT''')
old='\t\tCleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);'
pos=part.index('// GROOVE_XHCI_PREFLIGHT');head,tail=part[:pos],part[pos:]
zero='\t\tif (nBufLen) CleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);\n'
# The pinned source is also tested without the optional zero-packet patch.
expected=2 if zero in tail else 3
if tail.count(old)!=expected:raise SystemExit('Unexpected xHCI cache call count')
tail=tail.replace(old+'\n','')
if zero in tail:tail=tail.replace(zero,'')
path.write_text(s[:start]+head+tail+s[end:]);print('Circle xHCI cache maintenance moved before producer IRQ lock; physical timing unverified')
