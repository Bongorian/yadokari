#!/usr/bin/env python3
"""Watch both selected streams during duplex audio and unregister the survivor."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/sound/usbsoundbasedevice.h';cp=root/'lib/sound/usbsoundbasedevice.cpp'
h=hp.read_text();s=cp.read_text();marker='GROOVE_AUDIO_DUPLEX_REMOVAL'
if marker in h:
 if marker not in s:raise SystemExit('Incomplete duplex removal patch')
 print('Circle duplex removal patch already applied');raise SystemExit(0)
def once(src,a,b):
 if src.count(a)!=1:raise SystemExit('Unexpected duplex removal anchor: '+a[:120])
 return src.replace(a,b,1)
h=once(h,'\tTRegistrationHandle m_hRemoveRegistration;','\tTRegistrationHandle m_hRemoveRegistration;\n\tTRegistrationHandle m_hRXRemoveRegistration; // GROOVE_AUDIO_DUPLEX_REMOVAL')
s=once(s,'\tm_hRemoveRegistration (0)','\tm_hRemoveRegistration (0),\n\tm_hRXRemoveRegistration (0) // GROOVE_AUDIO_DUPLEX_REMOVAL')
a='''\tdelete m_pSoundController;
\tm_pSoundController = nullptr;

\tdelete [] m_pTXBuffer[0];'''
b='''\tif (m_hRXRemoveRegistration)
\t{
\t\tassert (m_pRXUSBDevice);
\t\tm_pRXUSBDevice->UnregisterRemovedHandler (m_hRXRemoveRegistration);
\t\tm_hRXRemoveRegistration = 0;
\t}

'''+a
# Destructor only: the same buffer cleanup also exists in the removal handler.
pos=s.index('CUSBSoundBaseDevice::~CUSBSoundBaseDevice');end=s.index('boolean CUSBSoundBaseDevice::SetImplicitFeedbackProfile',pos)
s=s[:pos]+once(s[pos:end],a,b)+s[end:]
a='''\t\tassert (m_hRemoveRegistration);

\t\tm_State = StateIdle;'''
b='''\t\tassert (m_hRemoveRegistration);
\t\t// A duplex input can disappear before the output (or independently).
\t\tassert (!m_hRXRemoveRegistration);
\t\tif (m_pTXUSBDevice && m_pRXUSBDevice && m_pTXUSBDevice != m_pRXUSBDevice)
\t\t{
\t\t\tm_hRXRemoveRegistration = m_pRXUSBDevice->RegisterRemovedHandler (DeviceRemovedStub, this);
\t\t\tassert (m_hRXRemoveRegistration);
\t\t}

\t\tm_State = StateIdle;'''
s=once(s,a,b)
a='''void CUSBSoundBaseDevice::DeviceRemovedHandler (CDevice *pDevice)
{
\tm_hRemoveRegistration = 0;

\tCancel ();'''
b='''void CUSBSoundBaseDevice::DeviceRemovedHandler (CDevice *pDevice)
{
\t// The current device owns its callback entry; unregister only the other
\t// live stream before draining. No stale callback may outlive this object.
\tif (m_hRXRemoveRegistration && pDevice == m_pRXUSBDevice)
\t{
\t\tm_hRXRemoveRegistration = 0;
\t\tif (m_hRemoveRegistration && m_pTXUSBDevice)
\t\t\tm_pTXUSBDevice->UnregisterRemovedHandler (m_hRemoveRegistration);
\t\tm_hRemoveRegistration = 0;
\t}
\telse
\t{
\t\tm_hRemoveRegistration = 0;
\t\tif (m_hRXRemoveRegistration && m_pRXUSBDevice)
\t\t\tm_pRXUSBDevice->UnregisterRemovedHandler (m_hRXRemoveRegistration);
\t\tm_hRXRemoveRegistration = 0;
\t}

\tCancel ();'''
s=once(s,a,b);hp.write_text(h);cp.write_text(s)
print('Circle duplex removal handlers patched; physical behavior unverified')
