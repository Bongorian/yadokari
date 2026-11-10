#!/usr/bin/env python3
"""Stop MK3 input before requeueing a faulted xHCI endpoint; recover in task context."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);hp=root/'include/circle/usb/usbhiddevice.h';cp=root/'lib/usb/usbhiddevice.cpp'
h=hp.read_text();s=cp.read_text();marker='GROOVE_HID_TASK_RECOVERY'
if (marker in h)!=(marker in s):raise SystemExit('Incomplete HID recovery patch')
def once(text,a,b):
 if text.count(a)!=1:raise SystemExit('Unexpected HID recovery anchor '+repr(a))
 return text.replace(a,b,1)
if marker not in h:
 h=once(h,'\tvirtual void ReportTransferError (TUSBError) {}','\tvirtual void ReportTransferError (TUSBError) {}\n\t// '+marker+': a device can forbid IRQ-side requeue after a fault.\n\tvirtual boolean RetryTransferError (TUSBError) { return TRUE; }\n\tboolean RecoverInputEndpoint (void);')
 s=once(s,'#include <circle/usb/usbhid.h>','#include <circle/usb/usbhid.h>\n#if RASPPI >= 4\n#include <circle/usb/xhciendpoint.h>\n#endif')
 s=once(s,'\t\tReportTransferError (pURB->GetUSBError ());','\t\tReportTransferError (pURB->GetUSBError ());\n\t\tbRestart = RetryTransferError (pURB->GetUSBError ());')
 a='boolean CUSBHIDDevice::StartRequest (void)'
 body='''// GROOVE_HID_TASK_RECOVERY: only after polling stopped and its URB dequeued.
 // Never reset a still-owned DMA request or run control transfers in the IRQ.
 boolean CUSBHIDDevice::RecoverInputEndpoint (void)
 {
  if (!m_pPoll || __atomic_load_n (&m_pPoll->pending, __ATOMIC_ACQUIRE)) return FALSE;
 #if RASPPI >= 4
  const u16 address = m_pReportEndpoint->GetNumber () | 0x80;
  if (GetHost ()->ControlMessage (GetEndpoint0 (), REQUEST_OUT | REQUEST_TO_ENDPOINT,
                                CLEAR_FEATURE, 0, address, 0, 0) < 0) return FALSE;
  if (!m_pReportEndpoint->GetXHCIEndpoint ()->ResetFromHalted ()) return FALSE;
 #endif
  m_nErrorCount = 0;
  return TRUE;
 }
 
 '''
 s=once(s,a,body+a);
hp.write_text(h);cp.write_text(s)

xp=root/'lib/usb/xhciendpoint.cpp';x=xp.read_text();error_marker='GROOVE_HID_ERROR_KIND'
if error_marker not in x:
 anchor='\t\t// GROOVE_ISO_ERROR_DIAGNOSTICS: avoid unbounded logging in callbacks.'
 replacement='\t\t// GROOVE_HID_ERROR_KIND: distinguish a missed service from a halted endpoint.\n\t\tpURB->SetUSBError (uchCompletionCode == XHCI_TRB_COMPLETION_CODE_MISSED_SERVICE_ERROR\n\t\t                   ? USBErrorFrameOverrun : USBErrorUnknown);\n'+anchor
 x=once(x,anchor,replacement);xp.write_text(x)

print('Circle task-owned HID recovery and missed-service classification verified')
