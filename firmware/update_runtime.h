#pragma once
#include "../src/file_bulk.h"
#include "storage_ui_wait.h"
#include "../src/update/fat_storage.h"
#include "wlan_preflight.h"
#include "../src/update/receiver.h"
#include "../src/update/input.h"
#include "../src/update/serial_control.h"
#include "../src/update/benchmark_requests.h"
#include "../src/update/usb_control.h"
#include "../src/software_canvas.h"
#include <circle/usb/gadget/usbcdcgadget.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/net/in.h>
#include <circle/sched/task.h>
#include <circle/bcmwatchdog.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/timer.h>
// Identity data must be emitted once per image by update_runtime.cpp.
unsigned GrooveFirmwareVersion();
class CUpdateDisk:public groove::update::BlockDevice {
 u8* m_Buffer;
public:
 CUpdateDisk();
 bool sector(unsigned lba,uint8_t* bytes,bool write)override;
};
class CUpdateReboot:public groove::update::Rebooter {
public:bool supported()const override;bool restart(unsigned partition,bool trial)override;
};
class CUpdateListener;
class CBcm4343Device;
class CWPASupplicant;
class CUpdateRuntime {
 CWlanPreflight m_WlanPreflight;
 CBcm4343Device* m_FileWlan=nullptr;CWPASupplicant* m_FileWpa=nullptr;bool m_FileAP=false,m_FileWlanInitialized=false;
 CUpdateDisk m_Disk;CUpdateReboot m_Reboot;groove::update::FatStorage m_Storage;
 groove::update::Manager m_Manager;groove::update::Receiver m_Receiver;
 groove::update::PadLatch m_PadLatch;
 groove::update::ButtonLatch m_ButtonLatch;groove::update::BenchmarkRequests m_BenchmarkRequests;
 CInterruptSystem* m_Interrupt;CUSBCDCGadget* m_Gadget=nullptr;CNetSubSystem* m_Net=nullptr;CUpdateListener* m_Listener=nullptr;CUpdateListener* m_ControlListener=nullptr;CSocket* m_Client=nullptr;CSocket* m_ControlClient=nullptr;
 CUSBSerialDevice* m_LastSerial=nullptr;CString m_Status;FATFS m_DataFs{};FIL m_Offline{};bool m_OfflineOpen=false,m_GadgetReady=false,m_NetworkWifi=false;
 unsigned m_Partition=0,m_LastActivity=0;bool m_Tryboot=false,m_Identity=false,m_Ready=false,m_Active=false,m_Autoboot=false,m_Remote=false,m_TrialFallback=false,m_JournalTrial=false;unsigned m_Start=0,m_FallbackStart=0;
 char m_Reply[64]{};unsigned m_ReplyUsed=0,m_ReplySent=0;enum Channel { Idle,Serial,TCP,OfflineChannel } m_Channel=Idle;
 static constexpr unsigned kLogChunkBytes=240;
 groove::update::UsbControlMux m_UsbControl;CUSBSerialDevice* m_UsbPeer=nullptr;char m_UsbReply[512]{};unsigned m_UsbReplyUsed=0,m_UsbReplySent=0;
 groove::update::SerialControl m_NetControl;char m_NetReply[512]{};unsigned m_NetReplyUsed=0,m_NetReplySent=0,m_NetControlActivity=0;
 static_assert(kLogChunkBytes*2+11<=sizeof m_UsbReply && kLogChunkBytes*2+11<=sizeof m_NetReply,"LOG framing must include newline and terminator");
 bool m_DiagnosticBusy=false;
 void StartNetwork(bool wifi,bool diagnostics=false);void Reply(int result);void Disconnect();void Offline();
 CSocket* TakeControl();void CloseControl();
 bool StartGadget();
 void Action(unsigned pad);
public:
 explicit CUpdateRuntime(CInterruptSystem* interrupt);
 bool Initialize(bool recovery);
 bool AdoptBootGadget(CUSBCDCGadget* gadget){if(!gadget || m_Gadget)return false;m_Gadget=gadget;m_GadgetReady=true;return true;}
 CNetSubSystem* BeginFilesNetwork(const char* ssid,bool station=false);
 bool EndFilesNetwork();
 bool FilesAccessPoint()const{return m_FileAP;}
 const char* NetworkStatus()const{return m_Status;}
 void Enter(uint16_t held_pads=0); // stop audio and save the project first
 void ReleasePads(){m_PadLatch.preset(0);m_ButtonLatch.reset();}
 bool active()const{return m_Active;}
 bool network_started()const{return m_Net!=nullptr;}
 bool remote_enabled()const{return m_Remote;}
 void DiagnosticBusy(bool busy){m_DiagnosticBusy=busy;}
 bool diagnostic_busy()const{return m_DiagnosticBusy;}
 unsigned TakeEfficiencyRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;if(m_BenchmarkRequests.take(22,pending))return 1;if(m_BenchmarkRequests.take(23,pending))return 3;if(m_BenchmarkRequests.take(24,pending))return 4;return 0;}
 bool TakeBenchmarkRequest(){return m_BenchmarkRequests.take(1,m_ControlClient || m_UsbReplySent<m_UsbReplyUsed);}
 unsigned TakePlaitsStressRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;if(m_BenchmarkRequests.take(2,pending))return 2;if(m_BenchmarkRequests.take(3,pending))return 3;if(m_BenchmarkRequests.take(4,pending))return 4;if(m_BenchmarkRequests.take(7,pending))return 7;return 0;}
 bool TakeLoopbackRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(5,pending);}
 bool TakeSampleWorkflowRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(6,pending);}
 unsigned TakeDspRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;if(m_BenchmarkRequests.take(20,pending))return 20;if(m_BenchmarkRequests.take(21,pending))return 21;return 0;}
 bool TakeFinalTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(19,pending);}
 bool TakeWave4TestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(18,pending);}
 bool TakePresetCheckRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(17,pending);}
 bool TakeInputTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(16,pending);}
 bool TakeAudioTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(15,pending);}
 bool TakeNativeTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(14,pending);}
 bool TakeMidiTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(13,pending);}
 bool TakeDeployCheckRequest(){return m_BenchmarkRequests.take(25,m_ControlClient || m_UsbReplySent<m_UsbReplyUsed);}
 bool TakeOptimizationRequest(){return m_BenchmarkRequests.take(26,m_ControlClient || m_UsbReplySent<m_UsbReplyUsed);}
 bool TakeUiTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(12,pending);}
 bool TakeSaveTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(8,pending);}
 bool TakeKitSwapRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(9,pending);}
 bool TakeChopTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(10,pending);}
 bool TakeKitRecordTestRequest(){bool pending=m_ControlClient || m_UsbReplySent<m_UsbReplyUsed;return m_BenchmarkRequests.take(11,pending);}
 bool remote_ready()const{return m_Remote&&m_Ready&&m_Identity&&!m_DiagnosticBusy;}
 bool RemoteEnterActive(){if(!remote_ready()||!m_Active)return false;m_Autoboot=false;return true;}
 bool Remote(groove::update::SerialCommand command);
 void RemoteStatus(CString& out)const;
 void TrialDiagnostics(CString& out)const;
 void RemoteLog(CString& out)const;
 void RemoteHardware(CString& out)const;
 // Both normal application and recovery call this from their main task.
 // Serial transfers exclusively own the CDC byte stream until completion.
 template<class Enter> void PollUsbControl(Enter enter,groove::bulk::Port* files=nullptr){
  using groove::update::SerialCommand;
  if(!m_GadgetReady || !m_Gadget)return;
  if(m_Gadget->UpdatePlugAndPlay()){if(files)files->disconnected();if(m_Channel==Serial)Disconnect();m_UsbPeer=nullptr;m_UsbControl={};m_UsbReplyUsed=m_UsbReplySent=0;}
  auto* serial=m_Gadget->GetSerialDevice();
  if(serial!=m_UsbPeer){if(files)files->disconnected();if(m_Channel==Serial)Disconnect();m_UsbPeer=serial;m_UsbControl={};m_UsbReplyUsed=m_UsbReplySent=0;}
  if(files)files->tick(CTimer::GetClockTicks64());
  if(!serial || m_Channel!=Idle)return;
  auto pump_files=[&](){StorageUiWait::Scope responsive;groove::bulk::pump(*serial,*files,[](){return CTimer::GetClockTicks64();});};
  if(files && files->framing()){pump_files();return;}

  if(m_UsbReplySent<m_UsbReplyUsed){int n=serial->Write(m_UsbReply+m_UsbReplySent,m_UsbReplyUsed-m_UsbReplySent);if(n<0){m_UsbReplyUsed=m_UsbReplySent=0;return;}m_UsbReplySent+=unsigned(n);return;}
  m_UsbReplyUsed=m_UsbReplySent=0;
  for(unsigned i=0;i<64;++i){unsigned char c=0;int n=serial->Read(&c,1);if(n<=0)return;
   auto input=m_UsbControl.feed(c,m_Active,files!=nullptr);if(input.kind==groove::update::UsbInputKind::FileStart){files->activity(CTimer::GetClockTicks64());files->feed(&c,1);pump_files();return;}if(input.kind==groove::update::UsbInputKind::BinaryStart){m_Autoboot=false;m_Channel=Serial;m_LastSerial=serial;m_Receiver.reset();m_LastActivity=CTimer::GetClockTicks();int result=m_Receiver.feed(&c,1);if(result)Reply(result);return;}
   if(input.kind==groove::update::UsbInputKind::None){continue;}SerialCommand command=input.command;
   CString result;if(command==SerialCommand::Status)RemoteStatus(result);else if(command==SerialCommand::Log && remote_enabled())RemoteLog(result);else if(command==SerialCommand::Hardware && remote_enabled())RemoteHardware(result);else{bool ok=false;if(remote_ready())ok=command==SerialCommand::Enter?(m_Active?RemoteEnterActive():enter()):Remote(command);result.Format("GBX1 RESULT command=%u ok=%u\n",unsigned(command),unsigned(ok));}
   const char* text=result;unsigned size=strlen(text);if(size>=sizeof m_UsbReply)size=sizeof m_UsbReply-1;memcpy(m_UsbReply,text,size);m_UsbReplyUsed=size;return;
  }
 }
 template<class Enter> void PollNetworkControl(Enter enter){
  using groove::update::SerialCommand;
  if(!m_ControlListener || !m_Remote)return;
  unsigned now=CTimer::GetClockTicks();
  if(!m_ControlClient){m_ControlClient=TakeControl();if(!m_ControlClient)return;m_NetControl={};m_NetReplyUsed=m_NetReplySent=0;m_NetControlActivity=now;}
  if(now-m_NetControlActivity>5000000){CloseControl();return;}
  if(m_NetReplySent<m_NetReplyUsed){int n=m_ControlClient->Send(m_NetReply+m_NetReplySent,m_NetReplyUsed-m_NetReplySent,MSG_DONTWAIT);if(n<0){CloseControl();return;}if(n>0){m_NetReplySent+=unsigned(n);m_NetControlActivity=now;}if(m_NetReplySent==m_NetReplyUsed)CloseControl();return;}
  unsigned char input[1600];int n=m_ControlClient->Receive(input,sizeof input,MSG_DONTWAIT);if(n<0){CloseControl();return;}if(!n)return;m_NetControlActivity=now;
  for(int i=0;i<n;++i){SerialCommand command=m_NetControl.feed(input[i]);if(command==SerialCommand::None)continue;
   CString result;if(command==SerialCommand::Status)RemoteStatus(result);else if(command==SerialCommand::Log && remote_enabled())RemoteLog(result);else if(command==SerialCommand::Hardware && remote_enabled())RemoteHardware(result);else{bool ok=false;if(remote_ready())ok=command==SerialCommand::Enter?(m_Active?RemoteEnterActive():enter()):Remote(command);result.Format("GBX1 RESULT command=%u ok=%u\n",unsigned(command),unsigned(ok));}
   const char* text=result;unsigned size=strlen(text);if(size>=sizeof m_NetReply)size=sizeof m_NetReply-1;memcpy(m_NetReply,text,size);m_NetReplyUsed=size;m_NetControlActivity=CTimer::GetClockTicks();return;
  }
 }
 void Pad(unsigned hardware,uint8_t tag);
 void Control(unsigned byte,uint8_t value,bool baseline);
 void Process();
 void PollTrialFallback();
 void Draw(groove::SoftwareCanvas& renderer);
 void Leds(uint8_t pads[16],uint8_t buttons[62],const groove::ui::Theme& theme=groove::ui::Theme::by_index(groove::ui::Theme::kDefault));
};
