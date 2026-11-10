#!/usr/bin/env python3
"""Release rejected single-packet implicit USB audio requests on Pi 4.

The pinned xHCI driver either submits the one TRB or fails before placing it
on the ring. Do not extend this to multi-packet requests: a partially enqueued
sequence needs separate ring rollback and request-lifetime handling.
"""
from pathlib import Path
import sys

path=Path(sys.argv[1])/'lib/usb/usbaudiostreaming.cpp'
source=path.read_text();marker='GROOVE_IMPLICIT_SUBMIT_OWNERSHIP'
if marker in source:
    if source.count(marker)!=2:raise SystemExit('Incomplete implicit request ownership patch')
    print('Circle implicit rejected-request ownership already applied');raise SystemExit(0)
start=source.index('boolean CUSBAudioStreamingDevice::SendChunk (')
mid=source.index('boolean CUSBAudioStreamingDevice::ReceiveChunk (',start)
end=source.index('boolean CUSBAudioStreamingDevice::SelectInputTerminal (',mid)
tx,rx=source[start:mid],source[mid:end]
anchor='\tboolean bOK = GetHost ()->SubmitAsyncRequest (pURB);\n'
if tx.count(anchor)!=1 or rx.count(anchor)!=1:raise SystemExit('Unexpected Circle audio submit anchor')
tx=tx.replace(anchor,anchor+'''
\t// GROOVE_IMPLICIT_SUBMIT_OWNERSHIP: a rejected single TRB was not queued.
#if RASPPI == 4
\tif (!bOK && m_bDeferredImplicit && !m_bSynchronousSync)
\t{
\t\tdelete pURB;
\t\treturn FALSE;
\t}
#endif
''',1)
rx=rx.replace(anchor,anchor+'''
\t// GROOVE_IMPLICIT_SUBMIT_OWNERSHIP: capture uses one TRB in this mode.
#if RASPPI == 4
\tif (!bOK && m_bSinglePacketImplicit)
\t{
\t\tdelete pURB;
\t\treturn FALSE;
\t}
#endif
''',1)
path.write_text(source[:start]+tx+rx+source[end:])
print('Circle Pi 4 implicit single-packet failed requests released; hardware unverified')
