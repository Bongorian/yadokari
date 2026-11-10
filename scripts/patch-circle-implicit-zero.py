#!/usr/bin/env python3
"""Allow one zero-length isochronous OUT TD for real zero-frame feedback."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';sp=root/'lib/sound/usbsoundbasedevice.cpp';rp=root/'lib/usb/usbrequest.cpp';xp=root/'lib/usb/xhciendpoint.cpp';qh=root/'include/circle/sound/groove_implicit_cadence.h'
queue=(Path(__file__).resolve().parent.parent/'src/usb_implicit_cadence.h').read_text()
if not qh.exists() or qh.read_text()!=queue:qh.write_text(queue)
h=hp.read_text();s=sp.read_text();r=rp.read_text();x=xp.read_text();marker='GROOVE_IMPLICIT_ZERO'
if marker in s:
 if marker not in h or marker not in r or marker not in x:raise SystemExit('Incomplete zero isochronous patch')
 print('Circle implicit zero-frame transfer already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected zero isochronous anchor '+old[:100])
 return src.replace(old,new,1)
h=once(h,'\tboolean SendChunk (boolean bAlreadyLocked = FALSE, unsigned nFrames = 0);','\t// GROOVE_IMPLICIT_ZERO: ~0 means nominal mode; 0 is a real empty packet.\n\tboolean SendChunk (boolean bAlreadyLocked = FALSE, unsigned nFrames = ~0u);')
start=s.index('boolean CUSBSoundBaseDevice::SendChunk');end=s.index('boolean CUSBSoundBaseDevice::ReceiveChunk',start);part=s[start:end]
part=once(part,'\tif (nFrames)\n\t{','\tconst boolean bZeroImplicit = m_bImplicitFeedback && nFrames == 0;\n\tif (nFrames != ~0u)\n\t{')
part=once(part,'if (!nFrameSize || !nChunkSizeBytes || nChunkSizeBytes % nFrameSize','if (!nFrameSize || (!nChunkSizeBytes && !bZeroImplicit) || nChunkSizeBytes % nFrameSize')
part=once(part,'''\tunsigned nChunkSize;
\tif (m_nSubframeSize == 2)''','''\t// GROOVE_IMPLICIT_ZERO: an empty packet carries no audio frames, so do
\t// not call the DSP or advance its sample clock.
\tunsigned nChunkSize = 0;
\tif (bZeroImplicit)
\t{
\t\t// submit a real zero-length TD below
\t}
\telse if (m_nSubframeSize == 2)''')
part=once(part,'if (!nChunkSize || nChunkSize > nChunkSizeBytes / m_nSubframeSize','if ((!nChunkSize && !bZeroImplicit) || nChunkSize > nChunkSizeBytes / m_nSubframeSize')
s=s[:start]+part+s[end:]
r=once(r,'''\tassert (usPacketSize > 0);
\tassert (usPacketSize <= m_nBufLen);''','''\t// GROOVE_IMPLICIT_ZERO: only a single empty isochronous OUT packet.
\tassert (usPacketSize > 0 || (m_nBufLen == 0 && !m_nNumIsoPackets
\t\t&& m_pEndpoint->GetType () == EndpointTypeIsochronous
\t\t&& !m_pEndpoint->IsDirectionIn ()));
\tassert (!m_nNumIsoPackets || m_usIsoPacketSize[0] != 0);
\tassert (usPacketSize <= m_nBufLen);''')
start=x.index('\t\tassert ((m_uchEndpointType & 3) == 1);');end=x.index('\n\tDataSyncBarrier ();',start);part=x[start:end]
part=once(part,'''\t\tassert (pBuffer != 0);
\t\tassert (nBufLen > 0);
\t\tassert ((uintptr) pBuffer > MEM_KERNEL_END);
\t\tCleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);''','''\t\t// GROOVE_IMPLICIT_ZERO: one zero-length OUT isochronous TD is valid.
\t\tassert (pBuffer != 0);
\t\tassert (nBufLen > 0 || (!pURB->GetEndpoint ()->IsDirectionIn ()
\t\t\t&& pURB->GetNumIsoPackets () == 1 && pURB->GetIsoPacketSize (0) == 0));
\t\tassert ((uintptr) pBuffer > MEM_KERNEL_END);
\t\tif (nBufLen) CleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);''')
x=x[:start]+part+x[end:]
x=once(x,'''\t\tif (pBuffer != 0)
\t\t{
\t\t\tassert (nBufLen > 0);
\t\t\tCleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);
\t\t}''','''\t\tif (pBuffer != 0 && nBufLen > 0)
\t\t{
\t\t\tCleanAndInvalidateDataCacheRange ((uintptr) pBuffer, nBufLen);
\t\t}''')
hp.write_text(h);sp.write_text(s);rp.write_text(r);xp.write_text(x)
print('Circle zero-frame isochronous OUT support patched (hardware unverified)')
