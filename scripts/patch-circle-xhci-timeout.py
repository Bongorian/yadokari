#!/usr/bin/env python3
"""Fence an xHCI endpoint after an unretired blocking timeout.

This does not cancel the transfer or prove that a concurrent completion has
retired; it prevents later submissions being mistaken for the old completion.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1]); hp=root/'include/circle/usb/xhciendpoint.h'; cp=root/'lib/usb/xhciendpoint.cpp'
h=hp.read_text(); c=cp.read_text(); marker='GROOVE_XHCI_TIMEOUT_FENCE'
if marker in h or marker in c:
 if marker not in h or marker not in c or 'm_bGrooveTimedOut = TRUE;' not in c or 'if (m_bGrooveTimedOut' not in c or 'm_bTransferCompleted = TRUE; // GROOVE_XHCI_SUBMIT_FAILURE' not in c: raise SystemExit('Incomplete xHCI timeout fence')
 print('Circle xHCI timeout fence already applied'); raise SystemExit(0)
def once(s,old,new,count=1):
 if s.count(old)!=count: raise SystemExit('Unexpected xHCI timeout anchor: '+repr(old))
 return s.replace(old,new)
h=once(h,'\tvolatile boolean m_bTransferCompleted;','\tvolatile boolean m_bTransferCompleted;\n\tboolean m_bGrooveTimedOut; // GROOVE_XHCI_TIMEOUT_FENCE')
c=once(c,'\tm_bTransferCompleted (TRUE),','\tm_bTransferCompleted (TRUE),\n\tm_bGrooveTimedOut (FALSE),',2)
c=once(c,'\tif (!TransferAsync (pURB, nTimeoutMs))\n\t{\n\t\treturn FALSE;\n\t}','\tif (!TransferAsync (pURB, nTimeoutMs))\n\t{\n\t\tm_bTransferCompleted = TRUE; // GROOVE_XHCI_SUBMIT_FAILURE\n\t\treturn FALSE;\n\t}')
c=once(c,'\t\t\tm_SpinLock.Acquire ();\n\t\t\tm_pURB[0] = m_pURB[1];','\t\t\tm_SpinLock.Acquire ();\n\t\t\tm_bGrooveTimedOut = TRUE; // GROOVE_XHCI_TIMEOUT_FENCE\n\t\t\tm_pURB[0] = m_pURB[1];')
c=once(c,'\tm_SpinLock.Acquire ();\n\tif (m_pURB[0] == 0)','\tm_SpinLock.Acquire ();\n\tif (m_bGrooveTimedOut)\n\t{\n\t\tm_SpinLock.Release ();\n\t\treturn FALSE;\n\t}\n\tif (m_pURB[0] == 0)')
hp.write_text(h);cp.write_text(c);print('Circle xHCI timeout fences later endpoint submissions; cancellation remains unverified')
