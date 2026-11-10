#!/usr/bin/env python3
"""Select a descriptor-validated input stream ordinal in the Circle USB backend."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';cp=root/'lib/sound/usbsoundbasedevice.cpp';sp=root/'include/circle/usb/usbaudiostreaming.h'
h=hp.read_text();s=cp.read_text();u=sp.read_text();marker='GROOVE_AUDIO_RX_SELECTION'
if marker in h:
 if marker not in s or marker not in u:raise SystemExit('Incomplete input selection patch')
 print('Circle input selection already applied');raise SystemExit(0)
def replace_once(src,a,b):
 if src.count(a)!=1:raise SystemExit('Unexpected input selection anchor: '+a[:120])
 return src.replace(a,b,1)
h=replace_once(h,'unsigned nDevice       = 0);','unsigned nDevice       = 0,\n\t\t\t     unsigned nRXInterface  = 0);')
h=replace_once(h,'\tunsigned m_nTXInterface;','\tunsigned m_nTXInterface;\n\tunsigned m_nRXInterface; // GROOVE_AUDIO_RX_SELECTION: ordinal among RX streams')
s=replace_once(s,'unsigned nDevice)\n:', 'unsigned nDevice, unsigned nRXInterface)\n:')
s=replace_once(s,'\tm_nTXInterface (0),','\tm_nTXInterface (0),\n\tm_nRXInterface (nRXInterface), // GROOVE_AUDIO_RX_SELECTION')
old='GetStreamingDevice (FALSE, 0)'
if s.count(old)!=3:raise SystemExit('Unexpected receive stream lookup count')
s=s.replace(old,'GetStreamingDevice (FALSE, m_nRXInterface)')
u=replace_once(u,'\tunsigned GetSampleSubslotBytes (void) const { return m_nSubframeSize; }','\tunsigned GetSampleSubslotBytes (void) const { return m_nSubframeSize; }\n\t// GROOVE_AUDIO_RX_SELECTION: endpoint address comes from the selected descriptor.\n\tunsigned GetDataEndpointAddress (void) const { return m_pEndpointData ? (m_pEndpointData->GetNumber () | (m_pEndpointData->IsDirectionIn () ? 0x80u : 0u)) : 0u; }')
hp.write_text(h);cp.write_text(s);sp.write_text(u)
print('Circle descriptor-selected input ordinal patched; physical operation unverified')
