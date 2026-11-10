#!/usr/bin/env python3
"""Reuse completed MIDI DMA buffers and quarantine them on xHCI timeout."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/usb/usbmidihost.h';sp=root/'lib/usb/usbmidihost.cpp';helper=root/'include/circle/usb/groove_midi_dma_buffer.h'
content=(Path(__file__).resolve().parent.parent/'src/midi_dma_buffer.h').read_text()
if not helper.exists() or helper.read_text()!=content:helper.write_text(content)
# midi_dma_buffer.h includes "alloc_policy.h"; keep a sibling copy next to
# the helper so the include resolves inside the Circle tree as well.
alloc=root/'include/circle/usb/alloc_policy.h';alloc_content=(Path(__file__).resolve().parent.parent/'src/alloc_policy.h').read_text()
if not alloc.exists() or alloc.read_text()!=alloc_content:alloc.write_text(alloc_content)
h=hp.read_text();s=sp.read_text();marker='GROOVE_MIDI_DMA_CACHE'
if marker in s:
 if marker not in h:raise SystemExit('Incomplete MIDI DMA cache patch')
 print('Circle MIDI DMA cache already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected MIDI DMA cache anchor '+old[:100])
 return src.replace(old,new,1)
h=once(h,'#include <circle/types.h>','#include <circle/types.h>\n#include <circle/usb/groove_midi_dma_buffer.h>')
h=once(h,'\tCUSBMIDIDevice *m_pInterface;','\t// '+marker+': reused on the task thread; quarantined after a timeout.\n\tgroove::MidiDmaBuffer m_OutputBuffer;\n\tCUSBMIDIDevice *m_pInterface;')
start=s.index('\t// GROOVE_MIDI_TIMEOUT_DMA:');end=s.index('\n\t// GROOVE_MIDI_EXACT_LENGTH:',start)
s=s[:start]+'''\t// GROOVE_MIDI_DMA_CACHE: reuse the GROOVE_MIDI_TIMEOUT_DMA buffer until timeout.
\tif (!pThis->m_OutputBuffer.prepare (nLength, DATA_CACHE_LINE_LENGTH_MAX)) return FALSE;
\tu8 *pBuffer = pThis->m_OutputBuffer.data ();
\tmemcpy (pBuffer, pData, nLength);
\tunsigned nStarted = CTimer::GetClockTicks ();
\tint nResult = pThis->GetHost ()->Transfer (pThis->m_pEndpointOut, pBuffer, nLength);
\tunsigned nElapsed = CTimer::GetClockTicks () - nStarted;
\tif (nResult < 0 && nElapsed >= 2900000u)
\t{
\t\tpThis->m_OutputBuffer.quarantine ();
\t\tCLogger::Get ()->Write (FromMIDI, LogWarning,
\t\t\t\t    "MIDI timeout: retaining DMA buffer; reconnect device");
\t}'''+s[end:]
hp.write_text(h);sp.write_text(s);print('Circle MIDI DMA reuse and timeout quarantine patched (Pi unverified)')
