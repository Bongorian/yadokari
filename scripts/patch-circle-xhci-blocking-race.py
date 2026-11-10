#!/usr/bin/env python3
"""Serialize blocking xHCI completion versus timeout; no DMA cancellation."""
from pathlib import Path
import sys
root=Path(sys.argv[1]); hp=root/'include/circle/usb/xhciendpoint.h'; cp=root/'lib/usb/xhciendpoint.cpp'
h=hp.read_text(); c=cp.read_text(); marker='GROOVE_XHCI_BLOCKING_OWNER'
if marker in h or marker in c:
 if marker not in h or marker not in c or 'm_pGrooveBlocking = 0; // GROOVE_XHCI_BLOCKING_OWNER' not in c:raise SystemExit('Incomplete xHCI blocking owner patch')
 print('Circle xHCI blocking ownership already applied');raise SystemExit(0)
def once(s,a,b,count=1):
 if s.count(a)!=count:raise SystemExit('Unexpected xHCI ownership anchor: '+repr(a))
 return s.replace(a,b)
h=once(h,'\tboolean m_bGrooveTimedOut; // GROOVE_XHCI_TIMEOUT_FENCE','\tboolean m_bGrooveTimedOut; // GROOVE_XHCI_TIMEOUT_FENCE\n\tCUSBRequest *m_pGrooveBlocking; // GROOVE_XHCI_BLOCKING_OWNER')
c=once(c,'\tm_bGrooveTimedOut (FALSE),','\tm_bGrooveTimedOut (FALSE),\n\tm_pGrooveBlocking (0),',2)
old='''\tpURB->SetCompletionRoutine (CompletionRoutine, 0, this);

\tassert (m_bTransferCompleted);
\tm_bTransferCompleted = FALSE;'''
new='''\tpURB->SetCompletionRoutine (CompletionRoutine, 0, this);

\t// Reserve the endpoint for this blocking request before submitting it.
\tm_SpinLock.Acquire ();
\tif (m_bGrooveTimedOut || m_pGrooveBlocking || m_pURB[0] || m_pURB[1])
\t{
\t\tm_SpinLock.Release ();
\t\treturn FALSE;
\t}
\tassert (m_bTransferCompleted);
\tm_pGrooveBlocking = pURB; // GROOVE_XHCI_BLOCKING_OWNER
\tm_bTransferCompleted = FALSE;
\tm_SpinLock.Release ();'''
c=once(c,old,new)
c=once(c,'\t\tm_bTransferCompleted = TRUE; // GROOVE_XHCI_SUBMIT_FAILURE\n\t\treturn FALSE;','\t\tm_SpinLock.Acquire ();\n\t\tm_pGrooveBlocking = 0;\n\t\tm_bTransferCompleted = TRUE; // GROOVE_XHCI_SUBMIT_FAILURE\n\t\tm_SpinLock.Release ();\n\t\treturn FALSE;')
old='''\t\t\tm_SpinLock.Acquire ();
\t\t\tm_bGrooveTimedOut = TRUE; // GROOVE_XHCI_TIMEOUT_FENCE
\t\t\tm_pURB[0] = m_pURB[1];
\t\t\tm_pURB[1] = 0;
\t\t\tm_SpinLock.Release ();
\t\t\tm_bTransferCompleted = TRUE;

\t\t\treturn FALSE;'''
new='''\t\t\tm_SpinLock.Acquire ();
\t\t\tif (m_bTransferCompleted)
\t\t\t{
\t\t\t\tm_SpinLock.Release ();
\t\t\t\tbreak;
\t\t\t}
\t\t\tm_bGrooveTimedOut = TRUE; // GROOVE_XHCI_TIMEOUT_FENCE
\t\t\tassert (m_pGrooveBlocking == pURB && m_pURB[0] == pURB && !m_pURB[1]);
\t\t\tm_pURB[0] = 0;
\t\t\tm_pGrooveBlocking = 0;
\t\t\tm_bTransferCompleted = TRUE;
\t\t\tm_SpinLock.Release ();
\t\t\treturn FALSE;'''
c=once(c,old,new)
c=once(c,'\tDataMemBarrier ();\n\n\treturn pURB->GetStatus () ? TRUE : FALSE;','\t// Wait until the completion handler has released its reference to pURB.\n\tm_SpinLock.Acquire ();\n\tm_SpinLock.Release ();\n\tDataMemBarrier ();\n\n\treturn pURB->GetStatus () ? TRUE : FALSE;')
c=once(c,'\tif (m_bGrooveTimedOut)\n\t{\n\t\tm_SpinLock.Release ();\n\t\treturn FALSE;\n\t}\n\tif (m_pURB[0] == 0)','\tif (m_bGrooveTimedOut || (m_pGrooveBlocking && m_pGrooveBlocking != pURB))\n\t{\n\t\tm_SpinLock.Release ();\n\t\treturn FALSE;\n\t}\n\tif (m_pURB[0] == 0)')
start=c.index('void CXHCIEndpoint::TransferEvent (');end=c.index('\nboolean CXHCIEndpoint::ResetFromHalted',start);part=c[start:end]
part=once(part,'\tCUSBRequest *pURB = m_pURB[0];\n\tif (!pURB)\n\t{\n\t\treturn;\n\t}','\tm_SpinLock.Acquire ();\n\tCUSBRequest *pURB = m_pURB[0];\n\tif (!pURB)\n\t{\n\t\tm_SpinLock.Release ();\n\t\treturn;\n\t}')
part=once(part,'\t\t// these events are not URB related, so just ignore them\n\t\treturn;','\t\t// these events are not URB related, so just ignore them\n\t\tm_SpinLock.Release ();\n\t\treturn;')
part=once(part,'''\tm_SpinLock.Acquire ();
\tm_pURB[0] = m_pURB[1];
\tm_pURB[1] = 0;
\tm_SpinLock.Release ();

\tpURB->CallCompletionRoutine ();''','''\tm_pURB[0] = m_pURB[1];
\tm_pURB[1] = 0;
\tif (pURB == m_pGrooveBlocking)
\t{
\t\tm_pGrooveBlocking = 0; // GROOVE_XHCI_BLOCKING_OWNER
\t\tDataMemBarrier ();
\t\tm_bTransferCompleted = TRUE;
\t\tm_SpinLock.Release ();
\t\treturn;
\t}
\tm_SpinLock.Release ();
\tpURB->CallCompletionRoutine ();''')
c=c[:start]+part+c[end:];hp.write_text(h);cp.write_text(c);print('Circle xHCI blocking timeout/completion ownership serialized; physical USB unverified')
