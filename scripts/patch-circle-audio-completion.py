#!/usr/bin/env python3
"""Distinguish failed USB audio transfers from valid empty capture packets.

Applied after the pinned Circle priming patch. Physical USB behavior is unverified.
"""
from pathlib import Path
import sys
root = Path(sys.argv[1])
stream = root / 'lib/usb/usbaudiostreaming.cpp'
sound = root / 'lib/sound/usbsoundbasedevice.cpp'
header = root / 'include/circle/sound/usbsoundbasedevice.h'
s = stream.read_text(); c = sound.read_text(); h = header.read_text()
marker = 'GROOVE_AUDIO_COMPLETION_GUARD'
if marker in c:
    if marker not in s or marker not in h:
        raise SystemExit('Incomplete Circle audio completion patch')
    old_tx = '''boolean bFailed = nBytesTransferred != m_nTXExpected[m_nTXCompletedBuffer];
\tm_nTXCompletedBuffer ^= 1;

\tm_SpinLock.Acquire ();'''
    new_tx = '''boolean bFailed = FALSE;

\tm_SpinLock.Acquire ();
\tbFailed = nBytesTransferred != m_nTXExpected[m_nTXCompletedBuffer];
\tm_nTXCompletedBuffer ^= 1;'''
    if old_tx in c:
        c = c.replace(old_tx, new_tx, 1)
        sound.write_text(c)
    old_rx = 'boolean bFailed = nBytesTransferred == ~0u;'
    if old_rx in c:
        c = c.replace(old_rx, 'boolean bFailed = nBytesTransferred == ~0u\n\t\t|| nBytesTransferred > m_nRXChunkSizeBytes * 2\n\t\t|| nBytesTransferred % m_nSubframeSize != 0;', 1)
        sound.write_text(c)
        print('Circle audio completion input bounds upgraded')
    else:
        print('Circle audio completion guard already applied')
    raise SystemExit(0)
def once(src, old, new):
    if src.count(old) != 1:
        raise SystemExit('Unexpected pinned Circle audio completion block: ' + old[:100])
    return src.replace(old, new, 1)
s = once(s, 'u32 nBytesTransferred = pURB->GetStatus () ? pURB->GetResultLength () : 0;',
         '// ' + marker + ': preserve successful zero-length capture packets.\n\tu32 nBytesTransferred = pURB->GetStatus () ? pURB->GetResultLength () : ~0u;')
h = once(h, '\tunsigned m_nTXCurrentBuffer;', '\tunsigned m_nTXCurrentBuffer;\n\t// ' + marker + ': match completion with its submitted buffer.\n\tunsigned m_nTXExpected[2];\n\tunsigned m_nTXCompletedBuffer;')
c = once(c, '\t\tm_nTXCurrentBuffer = 0;', '\t\tm_nTXCurrentBuffer = 0;\n\t\tm_nTXCompletedBuffer = 0;\n\t\tm_nTXExpected[0] = m_nTXExpected[1] = 0;')
c = once(c, '\tAtomicIncrement (&m_nOutstanding);\n\n\tif (!m_pTXUSBDevice->SendChunk',
         '\t// ' + marker + ': record the exact output size before submission.\n\tm_nTXExpected[m_nTXCurrentBuffer] = nChunkSize * m_nSubframeSize;\n\tAtomicIncrement (&m_nOutstanding);\n\n\tif (!m_pTXUSBDevice->SendChunk')
start = c.index('void CUSBSoundBaseDevice::TXCompletionRoutine')
end = c.index('void CUSBSoundBaseDevice::RXCompletionStub', start)
part = c[start:end]
part = once(part, 'void CUSBSoundBaseDevice::TXCompletionRoutine (unsigned nBytesTransferred)\n{\n\tboolean bContinue = FALSE;',
            'void CUSBSoundBaseDevice::TXCompletionRoutine (unsigned nBytesTransferred)\n{\n\tboolean bContinue = FALSE;\n\tboolean bFailed = FALSE;')
lock = '\tm_SpinLock.Acquire ();\n\n\tswitch (m_State)'
if part.count(lock) != 2:
    raise SystemExit('Unexpected callback locks')
part = part.replace(lock, '\tm_SpinLock.Acquire ();\n\tbFailed = nBytesTransferred != m_nTXExpected[m_nTXCompletedBuffer];\n\tm_nTXCompletedBuffer ^= 1;\n\n\tswitch (m_State)', 1)
part = once(part, 'void CUSBSoundBaseDevice::RXCompletionRoutine (unsigned nBytesTransferred)\n{\n\tboolean bContinue = FALSE;',
            'void CUSBSoundBaseDevice::RXCompletionRoutine (unsigned nBytesTransferred)\n{\n\tboolean bContinue = FALSE;\n\tboolean bFailed = nBytesTransferred == ~0u\n\t\t|| nBytesTransferred > m_nRXChunkSizeBytes * 2\n\t\t|| nBytesTransferred % m_nSubframeSize != 0;')
old = '''\tcase StateRunning:
\t\tAtomicDecrement (&m_nOutstanding);
\t\tbContinue = TRUE;
\t\tbreak;'''
new = '''\tcase StateRunning:
\t\tAtomicDecrement (&m_nOutstanding);
\t\tif (bFailed)
\t\t{
\t\t\tm_State = m_nOutstanding ? StateCanceled : StateIdle;
\t\t}
\t\telse
\t\t{
\t\t\tbContinue = TRUE;
\t\t}
\t\tbreak;'''
if part.count(old) != 2:
    raise SystemExit('Unexpected Circle audio callbacks')
part = part.replace(old, new)
old = '''\tif (!bContinue)
\t{
\t\treturn;
\t}'''
new = '''\tif (!bContinue)
\t{
\t\tif (bFailed) LOGWARN ("USB audio transfer failed or short");
\t\treturn;
\t}'''
if part.count(old) != 2:
    raise SystemExit('Unexpected Circle callback returns')
part = part.replace(old, new)
# A failed submission after the other direction has drained must not leave a
# permanently active object; otherwise the application can never reconnect.
old = '\t\tm_State = StateCanceled;'
new = '\t\tm_State = m_nOutstanding ? StateCanceled : StateIdle;'
if part.count(old) != 2:
    raise SystemExit('Unexpected Circle callback submission failure')
part = part.replace(old, new)
c = c[:start] + part + c[end:]
stream.write_text(s); sound.write_text(c); header.write_text(h)
print('Circle audio completion failure handling patched (physical transfers unverified)')
