#!/usr/bin/env python3
"""Support descriptor-selected 24-bit PCM in three- and four-byte subslots."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);sp=root/'lib/usb/usbaudiostreaming.cpp';sh=root/'include/circle/usb/usbaudiostreaming.h';cp=root/'lib/sound/usbsoundbasedevice.cpp';ch=root/'include/circle/sound/usbsoundbasedevice.h'
s=sp.read_text();h=sh.read_text();c=cp.read_text();bh=ch.read_text();marker='GROOVE_AUDIO_SUBSLOT'
if marker in s:
 if marker not in h or marker not in c or marker not in bh:raise SystemExit('Incomplete Circle subslot patch')
 if 'Output subslot width changed' not in c:
  anchor='\t\t\tif (m_pTXUSBDevice->NeedsImplicitFeedback ())'
  if c.count(anchor)!=1:raise SystemExit('Unexpected Circle output subslot guard anchor')
  guard='\t\t\tif (m_pTXUSBDevice->GetSampleSubslotBytes () != m_nSubframeSize)\n\t\t\t{\n\t\t\t\tLOGWARN ("Output subslot width changed");\n\t\t\t\treturn FALSE;\n\t\t\t}\n'
  c=c.replace(anchor,guard+anchor,1);cp.write_text(c)
 print('Circle 24-bit subslot selection already applied');raise SystemExit(0)
def once(text,old,new):
 if text.count(old)!=1:raise SystemExit('Unexpected Circle subslot anchor: '+old[:100])
 return text.replace(old,new,1)
h=once(h,'\tTDeviceInfo GetDeviceInfo (void) const;','\tTDeviceInfo GetDeviceInfo (void) const;\n\t// GROOVE_AUDIO_SUBSLOT\n\tunsigned GetSampleSubslotBytes (void) const { return m_nSubframeSize; }')
s=once(s,'''\t// We take the first alternate interface, which meets our expected parameters.

\tif (GetInterfaceProtocol () != USB_PROTO_AUDIO_VER_200)''','''\t// GROOVE_AUDIO_SUBSLOT: 24 valid PCM bits may use three or four bytes.
\t// The actual alternate descriptor selects the width; incompatible widths
\t// are rejected in the existing checks below.
\tif (m_nBitResolution == 24)
\t{
\t\tunsigned nBytes = GetInterfaceProtocol () == USB_PROTO_AUDIO_VER_200
\t\t\t? pFormatTypeDesc->Ver200.bSubslotSize : pFormatTypeDesc->Ver100.bSubframeSize;
\t\tif (nBytes == 3 || nBytes == 4) m_nSubframeSize = nBytes;
\t}

\t// We take the first alternate interface, which meets our expected parameters.

\tif (GetInterfaceProtocol () != USB_PROTO_AUDIO_VER_200)''')
bh=once(bh,'\tboolean IsRunning (void) const;','\tboolean IsRunning (void) const;\n\t// GROOVE_AUDIO_SUBSLOT: selected USB PCM bytes per sample.\n\tunsigned GetUSBSubslotBytes (void) const { return m_nSubframeSize; }')
c=once(c,'\t\t\tnHWTXChannels = Info.NumChannels;','\t\t\tnHWTXChannels = Info.NumChannels;\n\t\t\tm_nSubframeSize = m_pTXUSBDevice->GetSampleSubslotBytes ();')
c=once(c,'\t\t\tnHWRXChannels = Info.NumChannels;','\t\t\tnHWRXChannels = Info.NumChannels;\n\t\t\tif (m_DeviceMode == DeviceModeRXOnly)\n\t\t\t\tm_nSubframeSize = m_pRXUSBDevice->GetSampleSubslotBytes ();')
c=once(c,'''\tSetup (m_nBitResolution == 24 ? SoundFormatSigned24 : SoundFormatSigned16, 0,''','''\t// GROOVE_AUDIO_SUBSLOT: SoundBase queues use right-justified 24-in-32;
\t// wire conversion below shifts the valid bits to USB's most significant bits.
\tSetup (m_nBitResolution == 24 ? (m_nSubframeSize == 4 ? SoundFormatSigned24_32 : SoundFormatSigned24) : SoundFormatSigned16, 0,''')
c=once(c,'''\t\t\tif (m_pTXUSBDevice->NeedsImplicitFeedback ())''','''\t\t\tif (m_pTXUSBDevice->GetSampleSubslotBytes () != m_nSubframeSize)
\t\t\t{
\t\t\t\tLOGWARN ("Output subslot width changed");
\t\t\t\treturn FALSE;
\t\t\t}
\t\t\tif (m_pTXUSBDevice->NeedsImplicitFeedback ())''')
c=once(c,'''\t\t\tif (!m_pRXUSBDevice->Setup (m_nSampleRate))''','''\t\t\tif (m_pRXUSBDevice->GetSampleSubslotBytes () != m_nSubframeSize)
\t\t\t{
\t\t\t\tLOGWARN ("Input and output subslot widths differ");
\t\t\t\treturn FALSE;
\t\t\t}
\t\t\tif (!m_pRXUSBDevice->Setup (m_nSampleRate))''')
if c.count('assert (m_nSubframeSize == 3);')!=2:raise SystemExit('Unexpected Circle subslot assertions')
c=c.replace('assert (m_nSubframeSize == 3);','assert (m_nSubframeSize == 3 || m_nSubframeSize == 4);')
c=once(c,'''\t// The device can disappear while GetChunk is rendering.''','''\tif (m_nSubframeSize == 4)
\t{
\t\tu32 *pSamples = reinterpret_cast<u32 *> (m_pTXBuffer[m_nTXCurrentBuffer]);
\t\tfor (unsigned i = 0; i < nChunkSize; ++i) pSamples[i] = (pSamples[i] & 0xFFFFFFu) << 8;
\t}

\t// The device can disappear while GetChunk is rendering.''')
c=once(c,'''\t\t\tPutChunk (reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]),''','''\t\t\tif (m_nSubframeSize == 4)
\t\t\t{
\t\t\t\tu32 *pSamples = reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]);
\t\t\t\tfor (unsigned i = 0; i < nBytesTransferred / 4; ++i) pSamples[i] >>= 8;
\t\t\t}
\t\t\tPutChunk (reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]),''')
sp.write_text(s);sh.write_text(h);cp.write_text(c);ch.write_text(bh)
print('Circle 24-bit subslot selection and USB alignment patched (hardware unverified)')
