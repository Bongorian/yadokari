#!/usr/bin/env python3
"""Keep cache work outside xHCI endpoint lock for non-blocking transfers."""
from pathlib import Path
import sys
cp=Path(sys.argv[1])/'lib/usb/xhciendpoint.cpp';c=cp.read_text();marker='GROOVE_XHCI_ASYNC_CRITICAL'
if marker in c:
 if 'if (!bGrooveBlocking) m_SpinLock.Acquire ();' not in c:raise SystemExit('Incomplete xHCI async critical patch')
 print('Circle xHCI async critical section already shortened');raise SystemExit(0)
start=c.index('void CXHCIEndpoint::TransferEvent (');end=c.index('\nboolean CXHCIEndpoint::ResetFromHalted',start);part=c[start:end]
def once(s,a,b):
 if s.count(a)!=1:raise SystemExit('Unexpected xHCI async critical anchor: '+repr(a))
 return s.replace(a,b)
part=once(part,'\tif (   XHCI_TRB_SUCCESS (uchCompletionCode)','\t// The queued async request prevents a blocking reservation until dequeue.\n\t// Preserve the upstream assumption that endpoint events are serialized.\n\tconst boolean bGrooveBlocking = pURB == m_pGrooveBlocking; // GROOVE_XHCI_ASYNC_CRITICAL\n\tif (!bGrooveBlocking) m_SpinLock.Release ();\n\n\tif (   XHCI_TRB_SUCCESS (uchCompletionCode)')
part=once(part,'\t\t// these events are not URB related, so just ignore them\n\t\tm_SpinLock.Release ();','\t\t// these events are not URB related, so just ignore them\n\t\tif (bGrooveBlocking) m_SpinLock.Release ();')
part=once(part,'\tm_pURB[0] = m_pURB[1];\n\tm_pURB[1] = 0;\n\tif (pURB == m_pGrooveBlocking)','\tif (!bGrooveBlocking) m_SpinLock.Acquire ();\n\tm_pURB[0] = m_pURB[1];\n\tm_pURB[1] = 0;\n\tif (bGrooveBlocking)')
c=c[:start]+part+c[end:];cp.write_text(c);print('Circle xHCI async cache handling moved outside endpoint lock; real audio timing unverified')
