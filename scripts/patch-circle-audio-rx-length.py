#!/usr/bin/env python3
"""Check each successful input completion against its submitted buffer length."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';sp=root/'lib/sound/usbsoundbasedevice.cpp';qh=root/'include/circle/sound/groove_receive_lengths.h'
helper=(Path(__file__).resolve().parent.parent/'src/usb_receive_lengths.h').read_text()
if not qh.exists() or qh.read_text()!=helper: qh.write_text(helper)
h=hp.read_text();s=sp.read_text();marker='GROOVE_RX_SUBMITTED_LENGTH'
if marker in s:
 if marker not in h:raise SystemExit('Incomplete RX length patch')
 print('Circle RX submitted length guard already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected RX length anchor '+old[:100])
 return src.replace(old,new,1)
h=once(h,'#include <circle/sound/groove_implicit_cadence.h>','#include <circle/sound/groove_implicit_cadence.h>\n#include <circle/sound/groove_receive_lengths.h>')
h=once(h,'\tunsigned m_nRXCurrentBuffer;','\tunsigned m_nRXCurrentBuffer;\n\t// '+marker+': each completion is bounded by its queued request.\n\tgroove::UsbReceiveLengths m_RXLengths;')
s=once(s,'\t\tm_nRXCurrentBuffer = 0;','\t\tm_nRXCurrentBuffer = 0;\n\t\tm_RXLengths.reset ();')
anchor='\tassert (m_pRXBuffer[m_nRXCurrentBuffer]);\n\tif (!m_pRXUSBDevice->ReceiveChunk'
s=once(s,anchor,'\tassert (m_pRXBuffer[m_nRXCurrentBuffer]);\n\t// '+marker+': record before submit while the state lock is held.\n\tm_RXLengths.submitted (m_nRXCurrentBuffer, nChunkSizeBytes);\n\tif (!m_pRXUSBDevice->ReceiveChunk')
start=s.index('void CUSBSoundBaseDevice::RXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::RXCompletionStub',start);part=s[start:end]
part=once(part,'\tm_SpinLock.Acquire ();\n\t++m_nCallbacksActive;','\tm_SpinLock.Acquire ();\n\t++m_nCallbacksActive;\n\tbFailed = !m_RXLengths.completed (nBytesTransferred) || bFailed;')
s=s[:start]+part+s[end:];hp.write_text(h);sp.write_text(s)
print('Circle RX submitted-length guard patched (physical streaming unverified)')
