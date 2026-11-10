#!/usr/bin/env python3
"""Keep timed-out xHCI MIDI DMA buffers alive after a synchronous return."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'lib/usb/usbmidihost.cpp';s=p.read_text();marker='GROOVE_MIDI_TIMEOUT_DMA'
if marker in s:
 print('Circle MIDI timeout DMA guard already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected Circle MIDI DMA anchor '+old[:100])
 return src.replace(old,new,1)
s=once(s,'#include <circle/synchronize.h>','#include <circle/synchronize.h>\n#include <circle/timer.h>')
old='''\tDMA_BUFFER (u8, Buffer, nLength);
\tmemcpy (Buffer, pData, nLength);

\t// GROOVE_MIDI_EXACT_LENGTH: retain MIDI messages on a short transfer.
\treturn pThis->GetHost ()->Transfer (pThis->m_pEndpointOut, Buffer, nLength) == (int) nLength;'''
new='''\t// GROOVE_MIDI_TIMEOUT_DMA: xHCI may leave a timed-out TD in the ring.
\t// The ordinary stack DMA buffer would be invalid immediately on return.
\tif (nLength > ~0u - 2 * DATA_CACHE_LINE_LENGTH_MAX) return FALSE;
\tu8 *pAllocation = new u8[nLength + 2 * DATA_CACHE_LINE_LENGTH_MAX];
\tif (!pAllocation) return FALSE;
\tuintptr nAligned = ((uintptr) pAllocation + DATA_CACHE_LINE_LENGTH_MAX - 1)
\t\t\t & ~(uintptr) (DATA_CACHE_LINE_LENGTH_MAX - 1);
\tu8 *pBuffer = reinterpret_cast<u8 *> (nAligned);
\tmemcpy (pBuffer, pData, nLength);
\tunsigned nStarted = CTimer::GetClockTicks ();
\tint nResult = pThis->GetHost ()->Transfer (pThis->m_pEndpointOut, pBuffer, nLength);
\tunsigned nElapsed = CTimer::GetClockTicks () - nStarted;
\tif (nResult >= 0 || nElapsed < 2900000u) delete [] pAllocation;
\telse CLogger::Get ()->Write (FromMIDI, LogWarning,
\t\t\t	    "MIDI timeout: retaining DMA buffer; reconnect device");
\t// GROOVE_MIDI_EXACT_LENGTH: retain MIDI messages on a short transfer.
\treturn nResult == (int) nLength;'''
s=once(s,old,new);p.write_text(s);print('Circle MIDI timeout DMA guard applied (Pi behavior unverified)')
