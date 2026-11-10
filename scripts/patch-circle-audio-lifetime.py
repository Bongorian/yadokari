#!/usr/bin/env python3
"""Keep a USB sound device alive while its completion callbacks are executing.

The existing Circle lock protects state only, never the DSP GetChunk/PutChunk call.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1]); header=root/'include/circle/sound/usbsoundbasedevice.h'; source=root/'lib/sound/usbsoundbasedevice.cpp'
h=header.read_text();s=source.read_text(); marker='GROOVE_AUDIO_CALLBACK_LIFETIME'
if marker in s:
 if marker not in h: raise SystemExit('Incomplete Circle audio lifetime patch')
 start=s.index('void CUSBSoundBaseDevice::TXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::RXCompletionStub',start)
 part=s[start:end]
 part=part.replace('m_State = m_nOutstanding ? StateCanceled : StateIdle;', 'm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;')
 part=part.replace('if (!AtomicDecrement (&m_nOutstanding))','if (!AtomicDecrement (&m_nOutstanding) && m_nCallbacksActive == 1)')
 revised=s[:start]+part+s[end:]
 if revised!=s: source.write_text(revised)
 print('Circle audio callback lifetime guard already applied');raise SystemExit(0)
def once(text, old, new):
 if text.count(old)!=1: raise SystemExit('Unexpected Circle lifetime patch anchor: '+old[:100])
 return text.replace(old,new,1)
h=once(h,'\tvoid TXCompletionRoutine (unsigned nBytesTransferred);','\tvoid CallbackFinished (void);\n\tvoid TXCompletionRoutine (unsigned nBytesTransferred);')
h=once(h,'\tint m_nOutstanding;\n\n\tCSpinLock m_SpinLock;','\tint m_nOutstanding;\n\t// GROOVE_AUDIO_CALLBACK_LIFETIME: requests and executing callbacks differ.\n\tunsigned m_nCallbacksActive;\n\n\tmutable CSpinLock m_SpinLock;')
s=once(s,'\tm_pRXBuffer {nullptr, nullptr},\n\tm_pSoundController (nullptr),','\tm_pRXBuffer {nullptr, nullptr},\n\tm_nCallbacksActive (0),\n\tm_pSoundController (nullptr),')
s=once(s,'''\tif (m_State == StateRunning)
\t{
\t\tm_State = StateCanceled;
\t}''','''\tif (m_State == StateRunning)
\t{
\t\tm_State = m_nOutstanding || m_nCallbacksActive ? StateCanceled : StateIdle;
\t}''')
s=once(s,'''\treturn    m_State == StateRunning
\t       || m_State == StateCanceled;''','''\t// GROOVE_AUDIO_CALLBACK_LIFETIME: an executing callback may have already
\t// retired its USB request while it is still rendering or submitting.
\tm_SpinLock.Acquire ();
\tboolean bActive = m_State == StateRunning || m_State == StateCanceled
\t\t\t  || m_nCallbacksActive != 0;
\tm_SpinLock.Release ();
\treturn bActive;''')
start=s.index('void CUSBSoundBaseDevice::TXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::RXCompletionStub',start);part=s[start:end]
# Other callbacks may already have retired their requests and still be rendering.
# Keep canceled state until they can no longer submit another request.
old_state='m_State = m_nOutstanding ? StateCanceled : StateIdle;'
if part.count(old_state)!=4: raise SystemExit('Unexpected callback failure transitions')
part=part.replace(old_state,'m_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;')
old_drain='if (!AtomicDecrement (&m_nOutstanding))'
if part.count(old_drain)!=2: raise SystemExit('Unexpected callback draining transitions')
part=part.replace(old_drain,'if (!AtomicDecrement (&m_nOutstanding) && m_nCallbacksActive == 1)')
# Count each callback before retiring its USB request. IsActive sees this under
# the same lock, including when the callback cannot queue another request.
old='\tm_SpinLock.Acquire ();'
if part.count(old)!=2:raise SystemExit('Unexpected callback locks')
part=part.replace(old,old+'\n\t++m_nCallbacksActive;')
old='''\tif (!bContinue)
\t{
\t\tif (bFailed) LOGWARN ("USB audio transfer failed or short");
\t\treturn;
\t}'''
new='''\tif (!bContinue)
\t{
\t\tif (bFailed) LOGWARN ("USB audio transfer failed or short");
\t\tCallbackFinished ();
\t\treturn;
\t}'''
if part.count(old)!=2:raise SystemExit('Unexpected callback return blocks')
part=part.replace(old,new)
for direction in ('send','receive'):
 old='''\t\tLOGWARN ("Cannot %s chunk");

\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t}
}'''%direction
 new='''\t\tLOGWARN ("Cannot %s chunk");

\t\tm_SpinLock.Acquire ();
\t\tif (m_State == StateRunning)
\t\t{
\t\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t\t}
\t\tm_SpinLock.Release ();
\t}
\tCallbackFinished ();
}'''%direction
 part=once(part,old,new)
finished='''// The final operation of a completion callback. IsActive() takes the same
// lock, so the owner cannot destroy this object until all callbacks have
// stopped touching it, even if the last USB request was already retired.
void CUSBSoundBaseDevice::CallbackFinished (void)
{
\tm_SpinLock.Acquire ();
\tassert (m_nCallbacksActive);
\t--m_nCallbacksActive;
\tif (!m_nCallbacksActive && !m_nOutstanding && m_State == StateCanceled)
\t{
\t\tm_State = StateIdle;
\t}
\tm_SpinLock.Release ();
}

'''
s=s[:start]+finished+part+s[end:];header.write_text(h);source.write_text(s)
print('Circle audio callback lifetime guard patched (physical transfers unverified)')
