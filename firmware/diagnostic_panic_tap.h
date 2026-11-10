#pragma once
#include <circle/device.h>
#include <circle/usb/gadget/usbcdcgadget.h>
#include <circle/timer.h>
// A panic halts every core and destroys the remote LOG reader before the UART
// log queue is drained. During an explicitly armed audit only, also enqueue the
// panic line directly on the existing CDC endpoint. Never wait for CDC space,
// allocate, write DATA or change the normal UART destination.
class CDiagnosticPanicTap:public CDevice {
 CDevice& uart_;CUSBCDCGadget* gadget_;unsigned armed_=0;
public:
 CDiagnosticPanicTap(CDevice& uart,CUSBCDCGadget* gadget):uart_(uart),gadget_(gadget){}
 void arm(bool value){__atomic_store_n(&armed_,unsigned(value),__ATOMIC_RELEASE);}
 int Write(const void* buffer,size_t count)override{
  const auto* s=static_cast<const char*>(buffer);
  bool panic=count>=4 && s[0]=='\x1b' && s[1]=='[' && ((s[2]=='1' && s[3]=='m') || (count>=5 && s[2]=='9' && s[3]=='1' && s[4]=='m'));
  if(panic && __atomic_load_n(&armed_,__ATOMIC_ACQUIRE) && gadget_){auto* serial=gadget_->GetSerialDevice();if(serial)serial->Write(buffer,count>512?512:count);}
  return uart_.Write(buffer,count);
 }
};
