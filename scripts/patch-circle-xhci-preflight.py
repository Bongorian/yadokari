#!/usr/bin/env python3
"""Preflight a complete xHCI transfer and serialize its TRB producer.

This follows Circle's existing cycle-bit availability model; it does not prove
hardware retirement or cancel DMA. Same-endpoint producers are serialized.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1]);rp=root/'include/circle/usb/xhciring.h';rc=root/'lib/usb/xhciring.cpp';ep=root/'include/circle/usb/xhciendpoint.h';ec=root/'lib/usb/xhciendpoint.cpp'
rh=rp.read_text();r=rc.read_text();eh=ep.read_text();e=ec.read_text();marker='GROOVE_XHCI_PREFLIGHT'
if marker in r or marker in e:
 if marker not in r or marker not in e or 'm_EnqueueLock' not in eh:raise SystemExit('Incomplete xHCI preflight patch')
 print('Circle xHCI transfer preflight already applied');raise SystemExit(0)
def once(s,a,b):
 if s.count(a)!=1:raise SystemExit('Unexpected xHCI preflight anchor: '+repr(a))
 return s.replace(a,b)
rh=once(rh,'\tTXHCITRB *GetEnqueueTRB (void);\t\t// returns 0 if full','\tTXHCITRB *GetEnqueueTRB (void);\t\t// returns 0 if full\n\tboolean CanEnqueueTransfer (unsigned nCount) const; // GROOVE_XHCI_PREFLIGHT')
anchor='TXHCITRB *CXHCIRing::IncrementDequeue (void)'
method='''// GROOVE_XHCI_PREFLIGHT: use the same cycle-bit test as GetEnqueueTRB.
// The caller serializes all producers through the end of the submission.
boolean CXHCIRing::CanEnqueueTransfer (unsigned nCount) const
{
 assert (m_pFirstTRB != 0);
 assert (m_Type == XHCIRingTypeTransfer);
 if (!nCount || nCount > m_nTRBCount - 1) return FALSE;
 unsigned index = m_nEnqueueIndex;
 u32 cycle = m_nCycleState;
 for (unsigned i = 0; i < nCount; ++i)
 {
  if ((m_pFirstTRB[index].Control & XHCI_TRB_CONTROL_C) == cycle) return FALSE;
  if (++index == m_nTRBCount - 1)
  {
   if (m_pFirstTRB[index].Control & XHCI_LINK_TRB_CONTROL_TC) cycle ^= XHCI_TRB_CONTROL_C;
   index = 0;
  }
 }
 return TRUE;
}

'''
r=once(r,anchor,method+anchor)
eh=once(eh,'\tCSpinLock\t m_SpinLock;','\tCSpinLock\t m_SpinLock;\n\tCSpinLock\t m_EnqueueLock; // GROOVE_XHCI_PREFLIGHT')
start=e.index('boolean CXHCIEndpoint::TransferAsync');end=e.index('\nvoid CXHCIEndpoint::TransferEvent',start);part=e[start:end]
anchor='\tvoid *pBuffer = pURB->GetBuffer ();\n\tu32 nBufLen = pURB->GetBufLen ();\n\n\tm_SpinLock.Acquire ();'
replace='''\tvoid *pBuffer = pURB->GetBuffer ();
\tu32 nBufLen = pURB->GetBufLen ();

\t// GROOVE_XHCI_PREFLIGHT: reserve all ring entries before publishing any.
\t// One producer holds this lock until its request is queued and doorbelled.
\tm_EnqueueLock.Acquire ();
\tconst unsigned nTRBs = (m_uchEndpointType == 4) ? (nBufLen ? 3 : 2)
\t\t: ((m_uchEndpointType & 3) == 1) ? pURB->GetNumIsoPackets () : 1;
\tif (!m_pTransferRing->CanEnqueueTransfer (nTRBs))
\t{
\t\tm_EnqueueLock.Release ();
\t\treturn FALSE;
\t}
\tm_SpinLock.Acquire ();'''
part=once(part,anchor,replace)
part=once(part,'''\tif (m_bGrooveTimedOut || (m_pGrooveBlocking && m_pGrooveBlocking != pURB))
\t{
\t\tm_SpinLock.Release ();
\t\treturn FALSE;''','''\tif (m_bGrooveTimedOut || (m_pGrooveBlocking && m_pGrooveBlocking != pURB))
\t{
\t\tm_SpinLock.Release ();
\t\tm_EnqueueLock.Release ();
\t\treturn FALSE;''')
part=once(part,'''\t\tif (m_pURB[1] != 0)
\t\t{
\t\t\tm_SpinLock.Release ();
\t\t\treturn FALSE;''','''\t\tif (m_pURB[1] != 0)
\t\t{
\t\t\tm_SpinLock.Release ();
\t\t\tm_EnqueueLock.Release ();
\t\t\treturn FALSE;''')
part=once(part,'''\tm_pMMIO->db_write32 (m_pDevice->GetSlotID (), XHCI_REG_DB_TARGET_EP0 + m_uchEndpointID-1);

\treturn TRUE;''','''\tm_pMMIO->db_write32 (m_pDevice->GetSlotID (), XHCI_REG_DB_TARGET_EP0 + m_uchEndpointID-1);
\tm_EnqueueLock.Release ();

\treturn TRUE;''')
part=once(part,'''\telse if (m_pURB[1] == pURB)
\t{
\t\tm_pURB[1] = 0;
\t}
\tm_SpinLock.Release ();

\treturn FALSE;''','''\telse if (m_pURB[1] == pURB)
\t{
\t\tm_pURB[1] = 0;
\t}
\tm_SpinLock.Release ();
\tm_EnqueueLock.Release ();

\treturn FALSE;''')
e=e[:start]+part+e[end:];rp.write_text(rh);rc.write_text(r);ep.write_text(eh);ec.write_text(e);print('Circle xHCI complete-transfer preflight and producer serialization applied; hardware retirement unverified')
