#!/usr/bin/env python3
"""Fix advertised USB Audio sample-rate range matching in pinned Circle."""
from pathlib import Path
import sys

path = Path(sys.argv[1]) / 'lib/usb/usbaudiostreaming.cpp'
source = path.read_text()
marker = 'GROOVE_AUDIO_RATE_RANGE'
if marker in source:
    print('Circle audio rate-range guard already applied')
    raise SystemExit(0)
anchor = 'boolean CUSBAudioStreamingDevice::Setup (unsigned nSampleRate)'
old = '''\t\tif (   m_DeviceInfo.SampleRateRange[i].Min >= nSampleRate
\t\t    && m_DeviceInfo.SampleRateRange[i].Max <= nSampleRate)'''
new = '''\t\tif (GrooveAudioRateSupported (nSampleRate,
\t\t\t\tm_DeviceInfo.SampleRateRange[i].Min,
\t\t\t\tm_DeviceInfo.SampleRateRange[i].Max,
\t\t\t\tm_DeviceInfo.SampleRateRange[i].Resolution))'''
helper = '''// GROOVE_AUDIO_RATE_RANGE: inclusive range and discrete step, zero = unknown.
static boolean GrooveAudioRateSupported (unsigned rate, unsigned minimum,
\t\t\t\t\t unsigned maximum, unsigned resolution)
{
\treturn minimum <= rate && rate <= maximum
\t    && (!resolution || (rate - minimum) % resolution == 0);
}

'''
if source.count(anchor) != 1 or source.count(old) != 1:
    raise SystemExit('Unexpected pinned Circle USB audio rate selection')
source = source.replace(anchor, helper + anchor, 1).replace(old, new, 1)
path.write_text(source)
print('Circle USB audio sample-rate ranges now checked inclusively')
