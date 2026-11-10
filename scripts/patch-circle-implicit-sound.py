#!/usr/bin/env python3
"""Bounded capture-cadenced USB output for explicitly selected UAC2 pairs."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';cp=root/'lib/sound/usbsoundbasedevice.cpp';qh=root/'include/circle/sound/groove_implicit_cadence.h'
queue=(Path(__file__).resolve().parent.parent/'src/usb_implicit_cadence.h').read_text()
if not qh.exists() or qh.read_text()!=queue: qh.write_text(queue)
h=hp.read_text();s=cp.read_text();marker='GROOVE_IMPLICIT_SOUND'
if marker in s:
 if marker not in h:raise SystemExit('Incomplete implicit sound patch')
 print('Circle implicit sound patch already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected implicit sound anchor '+old[:130])
 return src.replace(old,new,1)
h=once(h,'#include <circle/sound/usbsoundcontroller.h>','#include <circle/sound/usbsoundcontroller.h>\n#include <circle/sound/groove_implicit_cadence.h>')
h=once(h,'\tboolean IsRunning (void) const;','''\tboolean IsRunning (void) const;

\t// GROOVE_IMPLICIT_SOUND: explicit provisional USB endpoint addresses.
\tboolean SetImplicitFeedbackProfile (unsigned nOutput, unsigned nInput);''')
h=once(h,'\tboolean SendChunk (boolean bAlreadyLocked = FALSE);','\tboolean SendChunk (boolean bAlreadyLocked = FALSE, unsigned nFrames = 0);')
h=once(h,'\tvoid CallbackFinished (void);','\tvoid DrainImplicit (void);\n\tvoid CallbackFinished (void);')
h=once(h,'\tunsigned m_nTXInterface;','''\tunsigned m_nTXInterface;
\tunsigned m_nImplicitOutput;
\tunsigned m_nImplicitInput;
\tboolean m_bImplicitFeedback;
\tboolean m_bImplicitDraining;
\tgroove::UsbImplicitCadence m_ImplicitCadence;''')
s=once(s,'\tm_nTXInterface (0),','''\tm_nTXInterface (0),
\tm_nImplicitOutput (0),
\tm_nImplicitInput (0),
\tm_bImplicitFeedback (FALSE),
\tm_bImplicitDraining (FALSE),''')
anchor='boolean CUSBSoundBaseDevice::Start (void)'
helper='''// GROOVE_IMPLICIT_SOUND: no implicit endpoint is selected automatically.
boolean CUSBSoundBaseDevice::SetImplicitFeedbackProfile (unsigned nOutput, unsigned nInput)
{
\tif (m_State != StateCreated || !nOutput || nOutput > 15
\t    || nInput < 0x81 || nInput > 0x8F) return FALSE;
\tm_nImplicitOutput = nOutput;
\tm_nImplicitInput = nInput;
\treturn TRUE;
}

'''
s=once(s,anchor,helper+anchor)
old='''\t\t\tif (!m_pTXUSBDevice->Setup (m_nSampleRate))'''
new='''\t\t\tif (m_pTXUSBDevice->NeedsImplicitFeedback ())
\t\t\t{
#if RASPPI < 4 || (defined(ARM_ALLOW_MULTI_CORE) && !defined(GROOVE_SINGLE_CORE_AUDIO_IRQ))
\t\t\t\tLOGWARN ("Implicit feedback requires serialized Pi 4 USB callbacks");
\t\t\t\treturn FALSE;
#else
\t\t\t\tif (m_DeviceMode != DeviceModeTXRX || !m_nImplicitOutput || !m_nImplicitInput)
\t\t\t\t{
\t\t\t\t\tLOGWARN ("Implicit feedback profile not selected");
\t\t\t\t\treturn FALSE;
\t\t\t\t}
\t\t\t\tm_pRXUSBDevice = GetStreamingDevice (FALSE, 0);
\t\t\t\tif (!m_pTXUSBDevice->EnableImplicitFeedback (m_pRXUSBDevice,
\t\t\t\t\t\t\t\t\t m_nImplicitOutput, m_nImplicitInput))
\t\t\t\t{
\t\t\t\t\tLOGWARN ("Implicit feedback profile does not match descriptors");
\t\t\t\t\treturn FALSE;
\t\t\t\t}
\t\t\t\tm_bImplicitFeedback = TRUE;
#endif
\t\t\t}
\t\t\tif (!m_pTXUSBDevice->Setup (m_nSampleRate))'''
s=once(s,old,new)
s=once(s,'\t\tm_nTXExpected[0] = m_nTXExpected[1] = 0;','''\t\tm_nTXExpected[0] = m_nTXExpected[1] = 0;
\t\tm_bImplicitDraining = FALSE;
\t\tm_ImplicitCadence.reset (2);''')
s=once(s,'''\t\tif (   !SendChunk (TRUE)
#if RASPPI >= 4
\t\t    || !SendChunk (TRUE)
#endif
\t\t   )''','''\t\tif (   !m_bImplicitFeedback
\t\t    && (   !SendChunk (TRUE)
#if RASPPI >= 4
\t\t        || !SendChunk (TRUE)
#endif
\t\t       ))''')
s=once(s,'boolean CUSBSoundBaseDevice::SendChunk (boolean bAlreadyLocked)','boolean CUSBSoundBaseDevice::SendChunk (boolean bAlreadyLocked, unsigned nFrames)')
s=once(s,'''\tunsigned nChunkSizeBytes = m_pTXUSBDevice->GetChunkSizeBytes ();
\tif (!bAlreadyLocked) m_SpinLock.Release ();''','''\tunsigned nChunkSizeBytes = m_pTXUSBDevice->GetChunkSizeBytes ();
\tif (nFrames)
\t{
\t\tconst unsigned nFrameSize = GetHWTXChannels () * m_nSubframeSize;
\t\tconst unsigned nPacketMax = m_pTXUSBDevice->GetDataMaxPacketSize ();
\t\tif (!m_bImplicitFeedback || !nFrameSize || nFrames > nPacketMax / nFrameSize)
\t\t{
\t\t\tif (!bAlreadyLocked) m_SpinLock.Release ();
\t\t\treturn FALSE;
\t\t}
\t\tnChunkSizeBytes = nFrames * nFrameSize;
\t}
\tif (!bAlreadyLocked) m_SpinLock.Release ();''')
# Only one callback drains the queue and renders audio; the lock is released
# around GetChunk and USB transfer setup's existing guarded submission.
anchor='void CUSBSoundBaseDevice::CallbackFinished (void)'
helper='''void CUSBSoundBaseDevice::DrainImplicit (void)
{
\tm_SpinLock.Acquire ();
\tif (m_bImplicitDraining || m_State != StateRunning)
\t{
\t\tm_SpinLock.Release ();
\t\treturn;
\t}
\tm_bImplicitDraining = TRUE;
\tm_SpinLock.Release ();
\tfor (;;)
\t{
\t\tunsigned nFrames = 0;
\t\tm_SpinLock.Acquire ();
\t\tif (m_State != StateRunning || !m_ImplicitCadence.take (nFrames))
\t\t{
\t\t\tm_bImplicitDraining = FALSE;
\t\t\tm_SpinLock.Release ();
\t\t\treturn;
\t\t}
\t\tm_SpinLock.Release ();
\t\tif (!SendChunk (FALSE, nFrames))
\t\t{
\t\t\tm_SpinLock.Acquire ();
\t\t\tif (m_State == StateRunning)
\t\t\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t\t\tm_bImplicitDraining = FALSE;
\t\t\tm_SpinLock.Release ();
\t\t\tLOGWARN ("Implicit feedback output failed");
\t\t\treturn;
\t\t}
\t}
}

'''
s=once(s,anchor,helper+anchor)
start=s.index('void CUSBSoundBaseDevice::TXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::TXCompletionStub',start);part=s[start:end]
part=once(part,'''\t\telse
\t\t{
\t\t\tbContinue = TRUE;
\t\t}''','''\t\telse if (m_bImplicitFeedback && !m_ImplicitCadence.completed ())
\t\t{
\t\t\tbFailed = TRUE;
\t\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t\t}
\t\telse
\t\t{
\t\t\tbContinue = TRUE;
\t\t}''')
part=once(part,'\tif (!SendChunk ())','''\tif (m_bImplicitFeedback)
\t{
\t\tDrainImplicit ();
\t\tCallbackFinished ();
\t\treturn;
\t}
\tif (!SendChunk ())''')
s=s[:start]+part+s[end:]
start=s.index('void CUSBSoundBaseDevice::RXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::RXCompletionStub',start);part=s[start:end]
part=once(part,'''\tif (!ReceiveChunk ())''','''\tif (m_bImplicitFeedback)
\t{
\t\tconst unsigned nFrameSize = GetHWRXChannels () * m_nSubframeSize;
\t\tm_SpinLock.Acquire ();
\t\tboolean bQueued = m_State == StateRunning && nFrameSize
\t\t\t\t&& nBytesTransferred % nFrameSize == 0
\t\t\t\t&& m_ImplicitCadence.capture (nBytesTransferred / nFrameSize);
\t\tif (!bQueued && m_State == StateRunning)
\t\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t\tm_SpinLock.Release ();
\t\tif (!bQueued)
\t\t{
\t\t\tLOGWARN ("Implicit feedback input invalid or queue full");
\t\t\tCallbackFinished ();
\t\t\treturn;
\t\t}
\t}
\tif (!ReceiveChunk ())''')
part=once(part,'''\tCallbackFinished ();
}
''','''\tif (m_bImplicitFeedback) DrainImplicit ();
\tCallbackFinished ();
}
''')
s=s[:start]+part+s[end:]
hp.write_text(h);cp.write_text(s)
print('Circle implicit capture cadence scheduler patched (hardware unverified)')
