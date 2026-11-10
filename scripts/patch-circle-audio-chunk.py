#!/usr/bin/env python3
"""Reject invalid USB audio chunk lengths before touching the fixed buffers."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
p=root/'lib/sound/usbsoundbasedevice.cpp'
stream=root/'lib/usb/usbaudiostreaming.cpp'
s=p.read_text()
marker='GROOVE_AUDIO_CHUNK_BOUNDS'
source=stream.read_text()
old_capture='''\t\t\tm_nChunkSizeBytes =   m_pEndpointData->GetMaxPacketSize ()
\t\t\t\t\t    - m_pEndpointData->GetMaxPacketSize () % m_nSubframeSize;'''
new_capture='''\t\t\t// GROOVE_AUDIO_CAPTURE_FRAME: endpoint capacity may contain padding;
\t\t\t// only complete interleaved frames can be delivered to the DSP.
\t\t\tunsigned nFrameSize = m_nChannels * m_nSubframeSize;
\t\t\tm_nChunkSizeBytes =   m_pEndpointData->GetMaxPacketSize ()
\t\t\t\t\t    - m_pEndpointData->GetMaxPacketSize () % nFrameSize;\n\t\t\tif (!m_nChunkSizeBytes) return FALSE;'''
if 'GROOVE_AUDIO_CAPTURE_FRAME' not in source:
    if source.count(old_capture)!=1: raise SystemExit('Unexpected Circle capture capacity')
    source=source.replace(old_capture,new_capture,1)
    stream.write_text(source)
old_getter='''unsigned CUSBAudioStreamingDevice::GetChunkSizeBytes (void) const
{
\tassert (m_nChunkSizeBytes);
\treturn m_nChunkSizeBytes;
}'''
new_getter='''unsigned CUSBAudioStreamingDevice::GetChunkSizeBytes (void) const
{
\t// GROOVE_AUDIO_ZERO_FEEDBACK: let the caller stop a malformed stream.
\treturn m_nChunkSizeBytes;
}'''
if 'GROOVE_AUDIO_ZERO_FEEDBACK' not in source:
    if source.count(old_getter)!=1: raise SystemExit('Unexpected Circle audio getter')
    source=source.replace(old_getter,new_getter,1)
    stream.write_text(source)
if marker in s:
    print('Circle audio chunk bounds already applied')
    raise SystemExit(0)
old='''\tassert (nChunkSizeBytes);
\tassert (nChunkSizeBytes % m_nSubframeSize == 0);
\tassert (nChunkSizeBytes <= m_nTXChunkSizeBytes * 2);'''
new='''\t// GROOVE_AUDIO_CHUNK_BOUNDS: feedback is external data, not an assertion.
\tconst unsigned nFrameSize = GetHWTXChannels () * m_nSubframeSize;
\tif (!nFrameSize || !nChunkSizeBytes || nChunkSizeBytes % nFrameSize
\t    || nChunkSizeBytes > m_nTXChunkSizeBytes * 2)
\t{
\t\tLOGWARN ("Invalid USB audio output chunk");
\t\treturn FALSE;
\t}'''
if s.count(old)!=1:raise SystemExit('Unexpected pinned Circle TX bounds')
s=s.replace(old,new,1)
old='''\tif (!nChunkSize)
\t{
\t\treturn FALSE;
\t}'''
new='''\tif (!nChunkSize || nChunkSize > nChunkSizeBytes / m_nSubframeSize
\t    || nChunkSize % GetHWTXChannels () != 0)
\t{
\t\treturn FALSE;
\t}'''
if s.count(old)!=1:raise SystemExit('Unexpected pinned Circle TX result')
s=s.replace(old,new,1)
old='''\tassert (nChunkSizeBytes);
\tassert (nChunkSizeBytes % m_nSubframeSize == 0);
\tassert (nChunkSizeBytes <= m_nRXChunkSizeBytes * 2);'''
new='''\tconst unsigned nFrameSize = GetHWRXChannels () * m_nSubframeSize;
\tif (!nFrameSize || !nChunkSizeBytes || nChunkSizeBytes % nFrameSize
\t    || nChunkSizeBytes > m_nRXChunkSizeBytes * 2)
\t{
\t\tLOGWARN ("Invalid USB audio input chunk");
\t\treturn FALSE;
\t}'''
if s.count(old)!=1:raise SystemExit('Unexpected pinned Circle RX bounds')
s=s.replace(old,new,1)
p.write_text(s)
print('Circle USB audio chunk bounds patched (physical feedback unverified)')
