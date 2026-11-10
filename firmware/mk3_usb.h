#pragma once
#include <circle/usb/usbhiddevice.h>
#include <circle/usb/usbfunction.h>
#include <circle/usb/usbendpoint.h>
#include <circle/usb/usbrequest.h>
#include "../src/mk3_input_queue.h"
#include "../src/backends.h"
#include "../src/hardware_diagnostics.h"
#include "../src/sync_output_guard.h"
#include "../src/latest_led_state.h"
#include "../src/output_retry.h"
using groove::Mk3EventType;
using groove::Mk3Pad;
struct Mk3LedOwner;
void GrooveReapUsbOutput();
class CMk3Hid : public CUSBHIDDevice, public groove::IControllerOutput {
public:
 explicit CMk3Hid(CUSBFunction* parent):CUSBHIDDevice(parent) {}
 ~CMk3Hid();
 boolean Configure() override;
 bool PopDiagnostic(groove::DiagnosticEvent& e){return m_Diagnostics.pop(e);}
 void Describe(groove::HardwareDiagnostics&);
 bool Pop(Mk3Pad& e) { return m_Events.pop(e); }
 void ServiceOutput();
 bool SetPadLeds(const u8 pads[16]);
 bool set_pad_leds(const uint8_t values[16]) override { return SetPadLeds(values); }
 bool set_pad_strip_leds(const uint8_t pads[16],const uint8_t strip[25])override;
 bool set_button_leds(const uint8_t values[62]) override;
 unsigned invalid_knobs() const { return __atomic_load_n(&m_InvalidKnobs,__ATOMIC_RELAXED); }
 unsigned dropped() const { return m_Events.dropped(); }
 u32 RequestInputResync(){return m_Events.request_resync();}
 bool InputStopped()const{return __atomic_load_n(&m_PollState,__ATOMIC_ACQUIRE)==1;}
 unsigned InputProgress()const{return __atomic_load_n(&m_PollProgress,__ATOMIC_ACQUIRE);}
 bool RestartInput(u32& resync);
 bool InputRecoveryFailed()const{return (m_InputRecoveryFailures>=3 && m_InputFailureProgress==InputProgress()) || !m_OutputGuard.ready();}
 void InputRecovered(){m_InputRecoveryFailures=0;m_InputFailureProgress=0;}
protected: void ReportHandler(const u8*,unsigned) override;void ReportTransferError(TUSBError) override {m_Events.mark_loss();}
 boolean RetryTransferError(TUSBError error) override {return error==USBErrorFrameOverrun;}void ReportPollingStopped()override{m_Events.mark_loss();__atomic_store_n(&m_PollState,1,__ATOMIC_RELEASE);}
private:
 static void LedCompleted(CUSBRequest*,void*,void*);
 Mk3LedOwner* m_Output=nullptr;groove::LatestLedState m_Latest;groove::OutputRetry m_LedRetry;unsigned m_LedStarted=0;
 groove::SyncOutputGuard m_OutputGuard;groove::SpscQueue<groove::DiagnosticEvent,32> m_Diagnostics;groove::Mk3InputQueue m_Events; unsigned m_InvalidKnobs=0,m_PadReportBytes=0,m_ButtonReportBytes=0,m_ControlReportBytes=0,m_PadInputReportBytes=0,m_PollState=0,m_PollProgress=0,m_InputRecoveryFailures=0,m_InputFailureProgress=0; bool m_Registered=false; u8 m_LastButtons[2]={}; u8 m_LastRaw[10]={}; u16 m_Knobs[8]={}; u16 m_Strip=0; u8 m_Encoder=0; bool m_InputInitialized=false; u8* m_LedBuffer=nullptr;
};
struct Mk3DisplayOwner;
struct Mk3DisplayTransfer { unsigned state=0,expected=0,completed_at=0,submitted_at=0,sequence=0,predecessor=0,queue_us=0,commands=0,progress_token=0,audio_callbacks=0,audio_gap=0,accepted_at=0; Mk3DisplayOwner* owner=nullptr; };
struct Mk3DisplayRequest { CUSBRequest* request=nullptr;Mk3DisplayTransfer* active=nullptr; };
struct Mk3DisplayOwner { unsigned submitted=0,last_sequence=0,last_completed_at=0;CUSBEndpoint* endpoint=nullptr;bool detached=false;Mk3DisplayRequest requests[2]{};bool busy()const{return requests[0].active || requests[1].active;} };
void Mk3DisplayRelease(Mk3DisplayTransfer*);
class CMk3Display : public CUSBFunction {
public:
 explicit CMk3Display(CUSBFunction* parent):CUSBFunction(parent) {}
 ~CMk3Display();
 boolean Configure() override;
 // The caller owns the buffer and completion context until completion.
 bool Submit(const void* data,unsigned size,Mk3DisplayTransfer* transfer);
 bool OutputReady()const{return !m_AsyncFenced;}
 void FenceOutput(){m_AsyncFenced=true;}
private:
 static void Completed(CUSBRequest*,void*,void*);
 CUSBEndpoint* m_Out=nullptr;Mk3DisplayOwner* m_Lifetime=nullptr; bool m_Registered=false,m_AsyncFenced=false;
};
