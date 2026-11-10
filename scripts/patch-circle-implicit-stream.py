#!/usr/bin/env python3
"""Expose explicitly selected UAC2 implicit-feedback streams in pinned Circle."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/usb/usbaudiostreaming.h';cp=root/'lib/usb/usbaudiostreaming.cpp'
h=hp.read_text();s=cp.read_text();marker='GROOVE_IMPLICIT_STREAM'
if marker in s:
 if marker not in h:raise SystemExit('Incomplete implicit stream patch')
 print('Circle implicit stream patch already applied');raise SystemExit(0)
def once(src,old,new):
 if src.count(old)!=1:raise SystemExit('Unexpected implicit stream anchor '+old[:100])
 return src.replace(old,new,1)
h=once(h,'\tTDeviceInfo GetDeviceInfo (void) const;','''\tTDeviceInfo GetDeviceInfo (void) const;

\t// GROOVE_IMPLICIT_STREAM: requires a deliberately selected endpoint pair.
\tboolean NeedsImplicitFeedback (void) const { return m_bDeferredImplicit; }
\tboolean EnableImplicitFeedback (CUSBAudioStreamingDevice *pInput,
\t\t\t\t\tunsigned nOutputAddress, unsigned nInputAddress);
\tunsigned GetDataMaxPacketSize (void) const { return m_pEndpointData->GetMaxPacketSize (); }''')
h=once(h,'\tboolean m_bVer200;','''\tboolean m_bVer200;
\tboolean m_bDeferredImplicit;
\tboolean m_bImplicitInput;
\tboolean m_bImplicitEnabled;''')
s=once(s,'\tm_pEndpointData (nullptr),','\tm_bDeferredImplicit (FALSE),\n\tm_bImplicitInput (FALSE),\n\tm_bImplicitEnabled (FALSE),\n\tm_pEndpointData (nullptr),')
s=once(s,'\tm_bIsOutput = (pEndpointDesc->bEndpointAddress & 0x80) == 0x00;','''\tm_bIsOutput = (pEndpointDesc->bEndpointAddress & 0x80) == 0x00;
\tm_bImplicitInput = m_bVer200 && !m_bIsOutput
\t\t\t&& (pEndpointDesc->bmAttributes & 0x30) == 0x20;''')
old='''\t\tif (   !pEndpointInDesc
\t\t    || (pEndpointInDesc->bmAttributes     & 0x3F) != 0x11  // Isochronous, Feedback
\t\t    || (pEndpointInDesc->bEndpointAddress & 0x80) != 0x80) // Input EP
\t\t{
\t\t\tLOGWARN ("Isochronous feedback input EP expected");

\t\t\treturn FALSE;
\t\t}

\t\tm_pEndpointSync = new CUSBEndpoint (GetDevice (), (TUSBEndpointDescriptor *) pEndpointInDesc);
\t\tassert (m_pEndpointSync != 0);'''
new='''\t\tif (!pEndpointInDesc && m_bVer200)
\t\t{
\t\t\t// Defer activation; no capture endpoint is inferred here.
\t\t\tm_bDeferredImplicit = TRUE;
\t\t}
\t\telse if (   !pEndpointInDesc
\t\t         || (pEndpointInDesc->bmAttributes & 0x3F) != 0x11
\t\t         || !(pEndpointInDesc->bEndpointAddress & 0x80))
\t\t{
\t\t\tLOGWARN ("Isochronous feedback input EP expected");
\t\t\treturn FALSE;
\t\t}
\t\telse
\t\t{
\t\t\tm_pEndpointSync = new CUSBEndpoint (GetDevice (), (TUSBEndpointDescriptor *) pEndpointInDesc);
\t\t\tassert (m_pEndpointSync != 0);
\t\t}'''
s=once(s,old,new)
anchor='boolean CUSBAudioStreamingDevice::Setup (unsigned nSampleRate)'
helper='''// GROOVE_IMPLICIT_STREAM: explicit provisional selection plus descriptor checks.
boolean CUSBAudioStreamingDevice::EnableImplicitFeedback (CUSBAudioStreamingDevice *pInput,
\t\t\t\t\t\t\t unsigned nOutputAddress, unsigned nInputAddress)
{
\tif (!pInput || !m_bDeferredImplicit || !m_bVer200 || !m_bIsOutput
\t    || !pInput->m_bVer200 || !pInput->m_bImplicitInput || pInput->m_bIsOutput
\t    || !m_pEndpointData || !pInput->m_pEndpointData
\t    || GetDevice () != pInput->GetDevice ()
\t    || GetInterfaceNumber () == pInput->GetInterfaceNumber ()
\t    || m_uchClockSourceID == USB_AUDIO_UNDEFINED_UNIT_ID
\t    || m_uchClockSourceID != pInput->m_uchClockSourceID
\t    || m_nDataIntervalFactor != pInput->m_nDataIntervalFactor
\t    || pInput->m_bSynchronousSync || m_bSynchronousSync
\t    || m_nSubframeSize != pInput->m_nSubframeSize
\t    || m_pEndpointData->GetNumber () != nOutputAddress
\t    || (pInput->m_pEndpointData->GetNumber () | 0x80u) != nInputAddress)
\t{
\t\treturn FALSE;
\t}
\tm_bImplicitEnabled = TRUE;
\treturn TRUE;
}

'''
s=once(s,anchor,helper+anchor)
s=once(s,'''\tassert (m_pEndpointData);

\t// is sample rate supported?''','''\tassert (m_pEndpointData);
\tif (m_bDeferredImplicit && !m_bImplicitEnabled)
\t{
\t\tLOGWARN ("Implicit feedback endpoint pair not selected");
\t\treturn FALSE;
\t}

\t// is sample rate supported?''')
hp.write_text(h);cp.write_text(s)
print('Circle explicit implicit-feedback stream selection patched (hardware unverified)')
