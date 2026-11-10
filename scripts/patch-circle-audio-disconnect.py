#!/usr/bin/env python3
"""Prevent USB audio submissions and buffer release after device removal.

Works on the pinned Circle source after the completion/lifetime patches.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1]); hpath=root/'include/circle/sound/usbsoundbasedevice.h'; path=root/'lib/sound/usbsoundbasedevice.cpp'
h=hpath.read_text();s=path.read_text();marker='GROOVE_AUDIO_DISCONNECT_GUARD'
if marker in s:
 if marker not in h: raise SystemExit('Incomplete Circle disconnect guard')
 print('Circle audio disconnect guard already applied');raise SystemExit(0)
def once(text,old,new):
 if text.count(old)!=1:raise SystemExit('Unexpected disconnect guard anchor: '+old[:120])
 return text.replace(old,new,1)
h=once(h,'\tboolean SendChunk (void);\n\tboolean ReceiveChunk (void);','\t// GROOVE_AUDIO_DISCONNECT_GUARD: startup already owns the state lock.\n\tboolean SendChunk (boolean bAlreadyLocked = FALSE);\n\tboolean ReceiveChunk (boolean bAlreadyLocked = FALSE);')
start=s.index('boolean CUSBSoundBaseDevice::Start (void)');end=s.index('void CUSBSoundBaseDevice::Cancel',start);part=s[start:end]
if part.count('SendChunk ()')!=2 or part.count('ReceiveChunk ()')!=2:raise SystemExit('Unexpected Circle startup submissions')
part=part.replace('SendChunk ()','SendChunk (TRUE)').replace('ReceiveChunk ()','ReceiveChunk (TRUE)')
s=s[:start]+part+s[end:]
start=s.index('boolean CUSBSoundBaseDevice::SendChunk');end=s.index('void CUSBSoundBaseDevice::CallbackFinished',start);part=s[start:end]
part=once(part,'boolean CUSBSoundBaseDevice::SendChunk (void)','boolean CUSBSoundBaseDevice::SendChunk (boolean bAlreadyLocked)')
part=once(part,'''\tassert (m_pTXUSBDevice);
\tunsigned nChunkSizeBytes = m_pTXUSBDevice->GetChunkSizeBytes ();''','''\t// GROOVE_AUDIO_DISCONNECT_GUARD: do not dereference a removed stream.
\tif (!bAlreadyLocked) m_SpinLock.Acquire ();
\tif (m_State != StateRunning || !m_pTXUSBDevice)
\t{
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;
\t}
\tunsigned nChunkSizeBytes = m_pTXUSBDevice->GetChunkSizeBytes ();
\tif (!bAlreadyLocked) m_SpinLock.Release ();''')
part=once(part,'''\t// GROOVE_AUDIO_COMPLETION_GUARD: record the exact output size before submission.
\tm_nTXExpected[m_nTXCurrentBuffer] = nChunkSize * m_nSubframeSize;
\tAtomicIncrement (&m_nOutstanding);''','''\t// The device can disappear while GetChunk is rendering. Serialize the
\t// state recheck and USB submission against Cancel and device removal.
\tif (!bAlreadyLocked) m_SpinLock.Acquire ();
\tif (m_State != StateRunning || !m_pTXUSBDevice)
\t{
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;
\t}
\t// GROOVE_AUDIO_COMPLETION_GUARD: record the exact output size before submission.
\tm_nTXExpected[m_nTXCurrentBuffer] = nChunkSize * m_nSubframeSize;
\tAtomicIncrement (&m_nOutstanding);''')
part=once(part,'''\t\tAtomicDecrement (&m_nOutstanding);

\t\treturn FALSE;
\t}

\tm_nTXCurrentBuffer ^= 1;

\treturn TRUE;''','''\t\tAtomicDecrement (&m_nOutstanding);
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;
\t}

\tm_nTXCurrentBuffer ^= 1;
\tif (!bAlreadyLocked) m_SpinLock.Release ();
\treturn TRUE;''')
part=once(part,'boolean CUSBSoundBaseDevice::ReceiveChunk (void)','boolean CUSBSoundBaseDevice::ReceiveChunk (boolean bAlreadyLocked)')
part=once(part,'''\tassert (m_pRXUSBDevice);
\tunsigned nChunkSizeBytes = m_pRXUSBDevice->GetChunkSizeBytes ();''','''\tif (!bAlreadyLocked) m_SpinLock.Acquire ();
\tif (m_State != StateRunning || !m_pRXUSBDevice)
\t{
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;
\t}
\tunsigned nChunkSizeBytes = m_pRXUSBDevice->GetChunkSizeBytes ();''')
part=once(part,'''\t\tLOGWARN ("Invalid USB audio input chunk");
\t\treturn FALSE;''','''\t\tLOGWARN ("Invalid USB audio input chunk");
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;''')
part=once(part,'''\t\tAtomicDecrement (&m_nOutstanding);

\t\treturn FALSE;
\t}

\tm_nRXCurrentBuffer ^= 1;

\treturn TRUE;''','''\t\tAtomicDecrement (&m_nOutstanding);
\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\treturn FALSE;
\t}

\tm_nRXCurrentBuffer ^= 1;
\tif (!bAlreadyLocked) m_SpinLock.Release ();
\treturn TRUE;''')
s=s[:start]+part+s[end:]
s=once(s,'''\t\t\tLOGWARN ("Timeout while going idle");

\t\t\tm_State = StateIdle;

\t\t\tbreak;''','''\t\t\t// Requests/callbacks may still own these buffers. Quarantine the
\t\t\t// sound object until they drain; the application can delete it only
\t\t\t// after IsActive() reports false. Never forge an idle state here.
\t\t\tLOGWARN ("Timeout while going idle; retaining USB audio buffers");
\t\t\treturn;''')
hpath.write_text(h);path.write_text(s)
print('Circle USB audio disconnect guard patched (physical transfers unverified)')
