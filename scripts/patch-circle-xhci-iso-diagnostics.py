#!/usr/bin/env python3
"""Count xHCI ring starvation by endpoint without logging from callbacks."""
from pathlib import Path
import sys
path = Path(sys.argv[1]) / 'lib/usb/xhciendpoint.cpp'
source = path.read_text()
marker='GROOVE_XHCI_ISO_COUNTERS'
if marker not in source:
    old='''\t// GROOVE_XHCI_ISO_STARVATION_DIAG: ring events need not have a URB.
\tif (   uchCompletionCode == XHCI_TRB_COMPLETION_CODE_RING_UNDERRUN
\t    || uchCompletionCode == XHCI_TRB_COMPLETION_CODE_RING_OVERRUN)
\t{
\t\tstatic unsigned nReported = 0;
\t\tif (__atomic_fetch_add (&nReported, 1, __ATOMIC_RELAXED) < 12)
\t\t\tCLogger::Get ()->Write (From, LogWarning, "Iso ring event %u endpoint %u",
\t\t\t\t(unsigned) uchCompletionCode, (unsigned) m_uchEndpointID);
\t}
'''
    new='''\t// GROOVE_XHCI_ISO_STARVATION_DIAG: ring events need not have a URB.
\tif (m_uchEndpointID < 32 && (uchCompletionCode == XHCI_TRB_COMPLETION_CODE_RING_UNDERRUN
\t    || uchCompletionCode == XHCI_TRB_COMPLETION_CODE_RING_OVERRUN))
\t\t__atomic_add_fetch (&s_GrooveIsoEvents[uchCompletionCode == XHCI_TRB_COMPLETION_CODE_RING_OVERRUN][m_uchEndpointID], 1, __ATOMIC_RELAXED);
'''
    anchor='void CXHCIEndpoint::TransferEvent (u8 uchCompletionCode, u32 nTransferLength)\n{\n'
    definition='''// GROOVE_XHCI_ISO_COUNTERS: completion events are counted in the hot path;
// sampling/logging happens in the application task, not the callback.
static unsigned s_GrooveIsoEvents[2][32] = {};
extern "C" unsigned GrooveXHCIIsoEvents (unsigned kind, unsigned endpoint)
{
\treturn kind < 2 && endpoint < 32 ? __atomic_load_n (&s_GrooveIsoEvents[kind][endpoint], __ATOMIC_RELAXED) : 0;
}

'''
    if source.count(anchor)!=1: raise SystemExit('Unexpected xHCI transfer event anchor')
    if 'GROOVE_XHCI_ISO_STARVATION_DIAG' in source:
        if source.count(old)!=1: raise SystemExit('Unexpected existing iso diagnostics')
        source=source.replace(old,new,1)
    else:
        source=source.replace(anchor,anchor+new,1)
    source=source.replace(anchor,definition+anchor,1)
    path.write_text(source)
