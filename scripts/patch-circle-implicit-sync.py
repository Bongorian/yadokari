#!/usr/bin/env python3
"""Use one capture packet per URB for synchronous implicit-feedback sources."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/usb/usbaudiostreaming.h';cp=root/'lib/usb/usbaudiostreaming.cpp';sound=root/'lib/sound/usbsoundbasedevice.cpp'
h=hp.read_text();s=cp.read_text();b=sound.read_text();marker='GROOVE_IMPLICIT_SYNC_CAPTURE'
if marker in s:
 if marker not in h or marker not in b:raise SystemExit('Incomplete implicit sync capture patch')
 print('Circle synchronous implicit capture patch already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected implicit sync anchor '+old[:120])
 return src.replace(old,new,1)
h=once(h,'\tboolean NeedsImplicitFeedback (void) const { return m_bDeferredImplicit; }','''\tboolean NeedsImplicitFeedback (void) const { return m_bDeferredImplicit; }
\t// GROOVE_IMPLICIT_SYNC_CAPTURE: one completion represents one input packet.
\tvoid SetImplicitCaptureMode (boolean bEnable) { m_bSinglePacketImplicit = bEnable; }
\tboolean UseSynchronousChunks (void) const { return m_bSynchronousSync && !m_bSinglePacketImplicit; }''')
h=once(h,'\tboolean m_bImplicitEnabled;','\tboolean m_bImplicitEnabled;\n\tboolean m_bSinglePacketImplicit;')
s=once(s,'\tm_bImplicitEnabled (FALSE),','\tm_bImplicitEnabled (FALSE),\n\tm_bSinglePacketImplicit (FALSE),')
s=once(s,'\t    || pInput->m_bSynchronousSync || m_bSynchronousSync','\t    || m_bSynchronousSync')
start=s.index('boolean CUSBAudioStreamingDevice::Setup');end=s.index('unsigned CUSBAudioStreamingDevice::GetChunkSizeBytes',start);part=s[start:end]
part=once(part,'\tif (m_bSynchronousSync)','\t// GROOVE_IMPLICIT_SYNC_CAPTURE: preserve each packet\'s actual length.\n\tif (UseSynchronousChunks ())');s=s[:start]+part+s[end:]
start=s.index('boolean CUSBAudioStreamingDevice::ReceiveChunk');end=s.index('boolean CUSBAudioStreamingDevice::SelectInputTerminal',start);part=s[start:end]
part=once(part,'\tif (m_bSynchronousSync)','\tif (UseSynchronousChunks ())')
part=once(part,'\t    && m_bSynchronousSync)','\t    && UseSynchronousChunks ())')
s=s[:start]+part+s[end:]
start=b.index('boolean CUSBSoundBaseDevice::Start');end=b.index('void CUSBSoundBaseDevice::Cancel',start);part=b[start:end]
part=once(part,'''\t\t\tif (!m_pRXUSBDevice->Setup (m_nSampleRate))''','''\t\t\t// GROOVE_IMPLICIT_SYNC_CAPTURE: only pair-selected capture uses one
\t\t\t// packet per request. Restore normal batching for other streams.
\t\t\tm_pRXUSBDevice->SetImplicitCaptureMode (m_bImplicitFeedback);
\t\t\tif (!m_pRXUSBDevice->Setup (m_nSampleRate))''')
b=b[:start]+part+b[end:];hp.write_text(h);cp.write_text(s);sound.write_text(b)
print('Circle single-packet synchronous feedback capture patched (hardware unverified)')
