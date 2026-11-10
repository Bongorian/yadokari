#!/usr/bin/env python3
"""Log a bounded number of otherwise silent isochronous transfer errors."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/usb/xhciendpoint.cpp'
source = path.read_text()
marker = 'GROOVE_ISO_ERROR_DIAGNOSTICS'
if marker not in source:
    old = '''\telse if (pURB->GetEndpoint ()->GetType () != EndpointTypeIsochronous)
\t{
\t\tCLogger::Get ()->Write (From, LogWarning, "Transfer error %u on endpoint %u",
\t\t\t\t\t(unsigned) uchCompletionCode, (unsigned) m_uchEndpointID);
\t}'''
    new = '''\telse
\t{
\t\t// GROOVE_ISO_ERROR_DIAGNOSTICS: avoid unbounded logging in callbacks.
\t\tstatic unsigned nIsoErrors = 0;
\t\tconst boolean bIso = pURB->GetEndpoint ()->GetType () == EndpointTypeIsochronous;
\t\tif (!bIso || __atomic_fetch_add (&nIsoErrors, 1, __ATOMIC_RELAXED) < 8)
\t\t\tCLogger::Get ()->Write (From, LogWarning, "Transfer error %u on endpoint %u%s",
\t\t\t\t(unsigned) uchCompletionCode, (unsigned) m_uchEndpointID, bIso ? " isochronous" : "");
\t}'''
    if source.count(old) != 1:
        raise SystemExit('Unexpected xHCI isochronous error anchor')
    path.write_text(source.replace(old, new, 1))
