#!/usr/bin/env python3
"""Reject a full xHCI request queue and remove only the rejected request.

This does not roll back partially enqueued multi-TRB transfers.
"""
from pathlib import Path
import sys
path=Path(sys.argv[1])/'lib/usb/xhciendpoint.cpp';s=path.read_text();marker='GROOVE_XHCI_QUEUE_IDENTITY'
if marker in s:
 if 'GROOVE_XHCI_DEEP_QUEUE' not in s and ('m_pURB[0] == pURB' not in s or 'm_pURB[1] == pURB' not in s):raise SystemExit('Incomplete xHCI queue identity patch')
 print('Circle xHCI request queue bounds and identity already applied');raise SystemExit(0)
def once(s,a,b):
 if s.count(a)!=1:raise SystemExit('Unexpected xHCI queue anchor: '+repr(a))
 return s.replace(a,b)
s=once(s,'\t\tassert (m_pURB[1] == 0);\n\t\tm_pURB[1] = pURB;','\t\tif (m_pURB[1] != 0)\n\t\t{\n\t\t\tm_SpinLock.Release ();\n\t\t\treturn FALSE; // GROOVE_XHCI_QUEUE_IDENTITY: no slot was taken.\n\t\t}\n\t\tm_pURB[1] = pURB;')
s=once(s,'''EnqueueError:
\tm_SpinLock.Acquire ();
\tif (m_pURB[1] != 0)
\t{
\t\tm_pURB[1] = 0;
\t}
\telse
\t{
\t\tm_pURB[0] = 0;
\t}
\tm_SpinLock.Release ();''','''EnqueueError:
\tm_SpinLock.Acquire ();
\t// GROOVE_XHCI_QUEUE_IDENTITY: another completion/submission may have
\t// shifted the failed request since its slot was assigned.
\tif (m_pURB[0] == pURB)
\t{
\t\tm_pURB[0] = m_pURB[1];
\t\tm_pURB[1] = 0;
\t}
\telse if (m_pURB[1] == pURB)
\t{
\t\tm_pURB[1] = 0;
\t}
\tm_SpinLock.Release ();''')
path.write_text(s);print('Circle xHCI rejects full request queues and removes failed requests by identity; partial TRBs unhandled')
