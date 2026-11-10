#!/usr/bin/env python3
"""Bound and validate UAC2 clock-range responses in the pinned Circle driver."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/usb/usbaudiostreaming.cpp'
source = path.read_text()
marker = 'GROOVE_AUDIO_RANGE_RESPONSE'
if marker in source:
    print('Circle UAC2 range response guard already applied')
    raise SystemExit(0)
start = source.index('\t\t// fetch supported sampling frequency ranges from clock source,')
end = source.index('\n\t\tuchTerminalLink = pGeneralDesc->Ver200.bTerminalLink;', start)
part = source[start:end]
if part.count('NumSubRanges, 2) < 0)') != 1 or part.count('RangesBuffer, nBufferSize) < 0)') != 1:
    raise SystemExit('Unexpected pinned Circle UAC2 range response')
part = part.replace('NumSubRanges, 2) < 0)', 'NumSubRanges, 2) != 2)', 1)
old = '''\t\t// now that we know the number of ranges, request the whole parameter block
\t\tunsigned nSampleRates = NumSubRanges[0];
\t\tunsigned nBufferSize = 2 + 12*nSampleRates;'''
new = '''\t\t// GROOVE_AUDIO_RANGE_RESPONSE: bound the stack buffer before using device data.
\t\tunsigned nSampleRates = NumSubRanges[0];
\t\tif (!nSampleRates)
\t\t{
\t\t\tLOGWARN ("Clock reports no sample-rate ranges");
\t\t\treturn FALSE;
\t\t}
\t\tif (nSampleRates > MaxSampleRatesRanges)
\t\t{
\t\t\tLOGWARN ("Clock range list truncated to %u entries", MaxSampleRatesRanges);
\t\t\tnSampleRates = MaxSampleRatesRanges;
\t\t}
\t\tunsigned nBufferSize = 2 + 12*nSampleRates;'''
if part.count(old) != 1:
    raise SystemExit('Unexpected pinned Circle UAC2 range allocation')
part = part.replace(old, new, 1).replace('RangesBuffer, nBufferSize) < 0)', 'RangesBuffer, nBufferSize) != (int) nBufferSize)', 1)
old = '''\t\t// fill in the m_DeviceInfo struct
\t\tif (nSampleRates > MaxSampleRatesRanges)
\t\t{
\t\t\tnSampleRates = MaxSampleRatesRanges;
\t\t}
\t\tm_DeviceInfo.SampleRateRanges = nSampleRates;

\t\tu32 *pFreq = (u32 *) (RangesBuffer+2);
\t\tfor (unsigned i = 0; i < nSampleRates; i++)
\t\t{
\t\t\tm_DeviceInfo.SampleRateRange[i].Min = *pFreq++;
\t\t\tm_DeviceInfo.SampleRateRange[i].Max = *pFreq++;
\t\t\tm_DeviceInfo.SampleRateRange[i].Resolution = *pFreq++;
\t\t}'''
new = '''\t\tunsigned nReported = (unsigned) RangesBuffer[0] | (unsigned) RangesBuffer[1] << 8;
\t\tif (nReported < nSampleRates)
\t\t{
\t\t\tLOGWARN ("Clock range count changed during read");
\t\t\treturn FALSE;
\t\t}
\t\tm_DeviceInfo.SampleRateRanges = nSampleRates;
\t\tfor (unsigned i = 0; i < nSampleRates; i++)
\t\t{
\t\t\tconst u8 *pFreq = RangesBuffer + 2 + 12*i;
\t\t\tm_DeviceInfo.SampleRateRange[i].Min = GrooveAudioReadLE32 (pFreq);
\t\t\tm_DeviceInfo.SampleRateRange[i].Max = GrooveAudioReadLE32 (pFreq+4);
\t\t\tm_DeviceInfo.SampleRateRange[i].Resolution = GrooveAudioReadLE32 (pFreq+8);
\t\t}'''
if part.count(old) != 1:
    raise SystemExit('Unexpected pinned Circle UAC2 range decoding')
part = part.replace(old, new, 1)
source = source[:start] + part + source[end:]
anchor = 'CUSBAudioStreamingDevice::CUSBAudioStreamingDevice (CUSBFunction *pFunction)'
helper = '''static u32 GrooveAudioReadLE32 (const u8 *p)
{
\treturn (u32) p[0] | (u32) p[1] << 8 | (u32) p[2] << 16 | (u32) p[3] << 24;
}

'''
if source.count(anchor) != 1:
    raise SystemExit('Unexpected pinned Circle USB audio constructor')
source = source.replace(anchor, helper + anchor, 1)
path.write_text(source)
print('Circle UAC2 clock-range reads are bounded, length-checked and byte-aligned')
