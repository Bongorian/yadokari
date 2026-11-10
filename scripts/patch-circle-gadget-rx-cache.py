#!/usr/bin/env python3
"""Complete AArch64 USB gadget OUT DMA ownership before CPU consumers read it."""
from pathlib import Path
import sys
path=Path(sys.argv[1])/'lib/usb/gadget/dwusbgadgetendpoint.cpp'
source=path.read_text();marker='GROOVE_GADGET_RX_DMA_COMPLETE'
insert='''
#if AARCH == 64
	// GROOVE_GADGET_RX_DMA_COMPLETE: a pre-DMA clean/invalidate does not
	// prevent speculative CPU refills while DMA owns the receive buffer.
	// Invalidate only (never clean stale lines back over the DMA result).
	// FinishTransfer runs before CDC Enqueue and EP0 OnControlMessage.
	if (m_TransferMode != TransferDataIn && nXferSize)
	{
		InvalidateDataCacheRange ((uintptr) m_pTransferBuffer, nXferSize);
	}
#endif
'''
anchor='\tnXferSize = m_nTransferLength - nXferSize;\n'
if marker in source:
 if source.count(marker)!=1 or 'InvalidateDataCacheRange ((uintptr) m_pTransferBuffer, nXferSize);' not in source:raise SystemExit('Incomplete gadget RX cache patch')
 print('Circle gadget RX DMA completion cache patch already applied');raise SystemExit(0)
if source.count(anchor)!=1:raise SystemExit('Unexpected Circle gadget FinishTransfer; refusing patch')
path.write_text(source.replace(anchor,anchor+insert));print('Circle gadget OUT/SETUP post-DMA invalidation patched (physical verification pending)')
