#!/usr/bin/env python3
"""Use actual input wire width and convert to the common internal PCM format."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';cp=root/'lib/sound/usbsoundbasedevice.cpp';sp=root/'lib/usb/usbaudiostreaming.cpp';conv=root/'include/circle/sound/groove_usb_pcm_convert.h'
helper=(Path(__file__).resolve().parent.parent/'src/usb_pcm_convert.h').read_text()
if not conv.exists() or conv.read_text()!=helper:conv.write_text(helper)
h=hp.read_text();s=cp.read_text();stream=sp.read_text();marker='GROOVE_AUDIO_MIXED_SUBSLOT'
if marker in s:
 if marker not in h or marker not in stream:raise SystemExit('Incomplete mixed subslot patch')
 anchor='\t    // GROOVE_AUDIO_MIXED_SUBSLOT: cadence is in frames, not bytes.\n'
 guard='\t    || m_nBitResolution != pInput->m_nBitResolution\n'
 if guard not in stream:
  if stream.count(anchor)!=1:raise SystemExit('Unexpected mixed sample resolution anchor')
  stream=stream.replace(anchor,anchor+guard,1);sp.write_text(stream)
 print('Circle mixed USB audio subslot support already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected mixed subslot anchor '+old[:100])
 return src.replace(old,new,1)
h=once(h,'#include <circle/sound/groove_implicit_cadence.h>','#include <circle/sound/groove_implicit_cadence.h>\n#include <circle/sound/groove_usb_pcm_convert.h>')
h=once(h,'\tunsigned m_nSubframeSize;','\tunsigned m_nSubframeSize;\n\t// GROOVE_AUDIO_MIXED_SUBSLOT: capture wire width may differ from internal PCM.\n\tunsigned m_nRXSubframeSize;')
s=once(s,'\tm_nSubframeSize (m_nBitResolution / 8),','\tm_nSubframeSize (m_nBitResolution / 8),\n\tm_nRXSubframeSize (m_nBitResolution / 8),')
s=once(s,'''\t\t\tnHWRXChannels = Info.NumChannels;
\t\t\tif (m_DeviceMode == DeviceModeRXOnly)
\t\t\t\tm_nSubframeSize = m_pRXUSBDevice->GetSampleSubslotBytes ();''','''\t\t\tnHWRXChannels = Info.NumChannels;
\t\t\tm_nRXSubframeSize = m_pRXUSBDevice->GetSampleSubslotBytes ();
\t\t\tif (m_DeviceMode == DeviceModeRXOnly)
\t\t\t\tm_nSubframeSize = m_nRXSubframeSize;''')
s=once(s,'''\t\t\tif (m_pRXUSBDevice->GetSampleSubslotBytes () != m_nSubframeSize)
\t\t\t{
\t\t\t\tLOGWARN ("Input and output subslot widths differ");
\t\t\t\treturn FALSE;
\t\t\t}''','''\t\t\t// GROOVE_AUDIO_MIXED_SUBSLOT: the selected input must agree with
\t\t\t// the width used to construct the receive buffer and callbacks.
\t\t\tif (m_pRXUSBDevice->GetSampleSubslotBytes () != m_nRXSubframeSize)
\t\t\t{
\t\t\t\tLOGWARN ("Input subslot width changed");
\t\t\t\treturn FALSE;
\t\t\t}''')
s=once(s,'m_pRXBuffer[0] = new u8[m_nRXChunkSizeBytes * 2];\n\t\t\tm_pRXBuffer[1] = new u8[m_nRXChunkSizeBytes * 2];','''// Up to 2x nominal USB bytes; expanding 3-byte samples to 4
\t\t\t// requires at most 8/3x the nominal capacity.
\t\t\tm_pRXBuffer[0] = new u8[m_nRXChunkSizeBytes * 3];
\t\t\tm_pRXBuffer[1] = new u8[m_nRXChunkSizeBytes * 3];''')
start=s.index('boolean CUSBSoundBaseDevice::ReceiveChunk');end=s.index('void CUSBSoundBaseDevice::DrainImplicit',start);part=s[start:end]
part=once(part,'GetHWRXChannels () * m_nSubframeSize','GetHWRXChannels () * m_nRXSubframeSize');s=s[:start]+part+s[end:]
start=s.index('void CUSBSoundBaseDevice::RXCompletionRoutine');end=s.index('void CUSBSoundBaseDevice::RXCompletionStub',start);part=s[start:end]
part=once(part,'nBytesTransferred % m_nSubframeSize != 0','nBytesTransferred % m_nRXSubframeSize != 0')
part=once(part,'assert (nBytesTransferred % m_nSubframeSize == 0);','assert (nBytesTransferred % m_nRXSubframeSize == 0);')
old='''\t\tif (m_nSubframeSize == 2)
\t\t{
\t\t\tPutChunk (reinterpret_cast<s16 *> (m_pRXBuffer[nRXCurrentBuffer]),
\t\t\t\t\t\t\t   nBytesTransferred / m_nSubframeSize);
\t\t}
\t\telse
\t\t{
\t\t\tassert (m_nSubframeSize == 3 || m_nSubframeSize == 4);
\t\t\tif (m_nSubframeSize == 4)
\t\t\t{
\t\t\t\tu32 *pSamples = reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]);
\t\t\t\tfor (unsigned i = 0; i < nBytesTransferred / 4; ++i) pSamples[i] >>= 8;
\t\t\t}
\t\t\tPutChunk (reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]),
\t\t\t\t\t\t\t   nBytesTransferred / m_nSubframeSize);
\t\t}'''
new='''\t\tconst unsigned nSamples = nBytesTransferred / m_nRXSubframeSize;
\t\tif (m_nSubframeSize == 2)
\t\t{
\t\t\tassert (m_nRXSubframeSize == 2);
\t\t\tPutChunk (reinterpret_cast<s16 *> (m_pRXBuffer[nRXCurrentBuffer]), nSamples);
\t\t}
\t\telse
\t\t{
\t\t\tif (!groove::usb_pcm24_capture_inplace (m_pRXBuffer[nRXCurrentBuffer],
\t\t\t\t\t nBytesTransferred, m_nRXSubframeSize, m_nSubframeSize,
\t\t\t\t\t m_nRXChunkSizeBytes * 3))
\t\t\t{
\t\t\t\tm_SpinLock.Acquire ();
\t\t\t\tif (m_State == StateRunning)
\t\t\t\t\tm_State = m_nOutstanding || m_nCallbacksActive > 1 ? StateCanceled : StateIdle;
\t\t\t\tm_SpinLock.Release ();
\t\t\t\tLOGWARN ("Invalid USB input conversion");
\t\t\t\tCallbackFinished ();
\t\t\t\treturn;
\t\t\t}
\t\t\tPutChunk (reinterpret_cast<u32 *> (m_pRXBuffer[nRXCurrentBuffer]), nSamples);
\t\t}'''
part=once(part,old,new)
part=once(part,'GetHWRXChannels () * m_nSubframeSize','GetHWRXChannels () * m_nRXSubframeSize');s=s[:start]+part+s[end:]
stream=once(stream,'\t    || m_nSubframeSize != pInput->m_nSubframeSize\n','\t    // GROOVE_AUDIO_MIXED_SUBSLOT: cadence is in frames, not bytes.\n\t    || m_nBitResolution != pInput->m_nBitResolution\n')
hp.write_text(h);cp.write_text(s);sp.write_text(stream)
print('Circle mixed input/output PCM subslots patched (hardware unverified)')
