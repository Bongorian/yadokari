"""Execute production HID poll/restart bodies against a halted asynchronous endpoint."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];circle=Path(os.environ.get('CIRCLEHOME',root/'.deps/circle-build'))
with tempfile.TemporaryDirectory() as directory:
 base=Path(directory);archive=subprocess.Popen(['git','-C',str(circle),'archive','HEAD'],stdout=subprocess.PIPE)
 subprocess.run(['tar','-x','-C',str(base)],stdin=archive.stdout,check=True);archive.stdout.close();assert archive.wait()==0
 subprocess.run(['git','init','-q',str(base)],check=True);subprocess.run(['git','-C',str(base),'add','lib/usb/usbdevice.cpp'],check=True)
 for script in re.findall(r'python3 "\$root/(scripts/patch-circle[^" ]+\.py)"',(root/'scripts/build-firmware.sh').read_text()):
  subprocess.run(['python3',str(root/script),str(base)],check=True,stdout=subprocess.DEVNULL)
  if script.endswith('hid-recovery.py'):break
 paths=['include/circle/usb/usbhiddevice.h','lib/usb/usbhiddevice.cpp','lib/usb/xhciendpoint.cpp'];first=[(base/p).read_bytes() for p in paths]
 subprocess.run(['python3',str(root/'scripts/patch-circle-hid-recovery.py'),str(base)],check=True);assert first==[(base/p).read_bytes() for p in paths]
 xhci=(base/paths[2]).read_text();assert 'uchCompletionCode == XHCI_TRB_COMPLETION_CODE_MISSED_SERVICE_ERROR' in xhci and '? USBErrorFrameOverrun : USBErrorUnknown' in xhci
 source=(base/paths[1]).read_text();a=source.index('boolean CUSBHIDDevice::RecoverInputEndpoint');bodies=source[a:]
 mk3=(root/'firmware/mk3_usb.cpp').read_text();a=mk3.index('bool CMk3Hid::RestartInput');b=mk3.index('\nboolean CMk3Hid::Configure',a);restart=mk3[a:b]
 code=r'''
#include <cassert>
#include <cstdint>
#include "mk3_input_queue.h"
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using boolean=bool;
constexpr bool TRUE=true,FALSE=false;constexpr unsigned RASPPI=4;
#define RASPPI 4
#define MSEC2HZ(x) (x)
constexpr unsigned USBErrorFrameOverrun=3,LogError=1,ErrorDecayMs=2000,MaxConsecutiveErrors=50;
constexpr unsigned REQUEST_OUT=0,REQUEST_TO_ENDPOINT=2,CLEAR_FEATURE=1;
const char* FromUSBHID="hid";
struct CTimer{static CTimer* Get(){static CTimer t;return &t;}unsigned GetTicks(){return 1;}};
struct CLogger{static CLogger* Get(){static CLogger l;return &l;}void Write(const char*,unsigned,const char*){}};
struct Endpoint{bool halted=false;unsigned resets=0;Endpoint* GetXHCIEndpoint(){return this;}unsigned GetNumber(){return 1;}bool ResetFromHalted(){++resets;halted=false;return true;}};
struct CUSBRequest{using Callback=void(*)(CUSBRequest*,void*,void*);Callback callback=nullptr;void* argument=nullptr;bool status=false;unsigned error=9;CUSBRequest* context=nullptr;bool GetStatus(){return status;}unsigned GetUSBError(){return error;}unsigned GetResultLength(){return 42;}void Reuse(void*,unsigned){status=false;}void SetCompletionRoutine(Callback fn,void*,void* owner){callback=fn;argument=owner;}};
struct Host{Endpoint endpoint;unsigned calls=0,clears=0;bool control_ok=true,immediate_error=false;bool SubmitAsyncRequest(CUSBRequest* r){++calls;if(immediate_error)r->callback(r,nullptr,r->argument);return true;}bool IsPlugAndPlay(){return true;}int ControlMessage(Endpoint*,unsigned,unsigned,unsigned,unsigned address,void*,unsigned){assert(address==0x81);++clears;return control_ok?0:-1;}};
struct CUSBHIDDevice{
 struct TPoll{CUSBHIDDevice* owner;CUSBRequest* request;unsigned pending=0;};CUSBRequest request;TPoll poll{this,&request};TPoll* m_pPoll=&poll;Host host;Endpoint* m_pReportEndpoint=&host.endpoint;u8* m_pReportBuffer=nullptr;unsigned m_nMaxReportSize=64,m_nErrorCount=0,m_nLastErrorTicksHZ=0;bool retry=true;
 Host* GetHost(){return &host;}Endpoint* GetEndpoint0(){return &host.endpoint;}
 static void CompletionStub(CUSBRequest*,void*,void*);
 virtual void ReportHandler(const u8*,unsigned){}virtual void ReportTransferError(unsigned){}virtual bool RetryTransferError(unsigned){return retry;}virtual void ReportPollingStopped(){}
 bool StartRequest();bool RecoverInputEndpoint();void CompletionRoutine(CUSBRequest*);
};
struct CMk3Hid:CUSBHIDDevice{
 groove::Mk3InputQueue m_Events;unsigned m_PollState=0,m_PollProgress=0,m_InputRecoveryFailures=0,m_InputFailureProgress=0;
 bool RestartInput(u32&);unsigned InputProgress()const{return m_PollProgress;}
 void ReportTransferError(unsigned)override{m_Events.mark_loss();}bool RetryTransferError(unsigned error)override{return error==USBErrorFrameOverrun;}
 void ReportPollingStopped()override{m_PollState=1;}
 void ReportHandler(const u8*,unsigned)override{++m_PollProgress;}
};
'''+bodies+'\n'+restart+r'''
int main(){u32 resync=0;
 // Old policy blindly requeues to a HALTED xHC ring, which never completes.
 CUSBHIDDevice old;assert(old.StartRequest());old.host.endpoint.halted=true;old.CompletionRoutine(&old.request);assert(old.poll.pending==1 && old.host.calls==2 && old.host.endpoint.halted);
 CMk3Hid h;assert(h.StartRequest());h.host.endpoint.halted=true;h.CompletionRoutine(&h.request);assert(h.m_PollState==1 && !h.poll.pending && h.host.calls==1);
 assert(h.RestartInput(resync));assert(resync!=0);assert(h.host.clears==1 && h.host.endpoint.resets==1 && h.poll.pending==1 && h.m_PollState==0);assert(!h.RestartInput(resync));
 h.request.status=true;h.CompletionRoutine(&h.request);assert(h.m_PollProgress==1 && h.poll.pending==1);h.m_InputRecoveryFailures=0; // task observed successful progress

 h.request.status=false;h.CompletionRoutine(&h.request);h.host.control_ok=false;
 for(unsigned i=0;i<3;++i){assert(!h.RestartInput(resync));}assert(h.m_InputRecoveryFailures==3 && h.m_PollState==1 && !h.poll.pending);
 unsigned resets=h.host.endpoint.resets;h.poll.pending=1;assert(!h.RestartInput(resync));assert(h.host.endpoint.resets==resets);h.poll.pending=0;
 CMk3Hid immediate;assert(immediate.StartRequest());immediate.CompletionRoutine(&immediate.request);immediate.host.immediate_error=true;for(unsigned i=0;i<3;++i){assert(!immediate.RestartInput(resync));assert(immediate.m_InputRecoveryFailures==i+1 && immediate.m_PollState==1 && !immediate.poll.pending);}
 CMk3Hid overrun;assert(overrun.StartRequest());overrun.request.error=USBErrorFrameOverrun;overrun.CompletionRoutine(&overrun.request);assert(overrun.poll.pending && overrun.m_PollState==0);
}
'''
 p=base/'hid.cpp';p.write_text(code);exe=base/'hid';subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('HID fault injection: old halted requeue reproduced; stopped ownership, clear/reset/restart, success, repeated failure, pending DMA and frame overrun passed; hardware trigger unverified')
# The real kernel escalation body must quiesce before device removal and stay
# bounded even if a replacement controller repeats the same fault.
kernel=(root/'firmware/kernel.cpp').read_text();start=kernel.index('  bool software_usb_changed=');end=kernel.index('  bool usb_changed=',start);escalation=kernel[start:end]
code=r'''
#include <cassert>
#include <vector>
#include "output_retry.h"
constexpr bool FALSE=false;constexpr unsigned LogWarning=1;const char* FromKernel="kernel";
std::vector<unsigned> order;
struct CTimer{static inline uint64_t ticks=0;static uint64_t GetClockTicks64(){return ticks;}};
struct CXHCIDevice{void ReScanDevices(){order.push_back(4);}} host;
struct Device{CXHCIDevice* GetHost(){return &host;}void RemoveDevice(){order.push_back(3);}} device;
struct CMk3Hid{bool failed=true;bool InputRecoveryFailed(){return failed;}Device* GetDevice(){return &device;}} hid;
struct Names{void* GetDevice(const char*,bool){return &hid;}} m_DeviceNameService;
struct Busy{bool on=false;bool active(){return on;}bool busy(){return on;}bool recording_busy(){return on;}} update,transfer,files,deployment_check,ui_probe,preset_check,kit_check,fluid_probe,display_calibration,dsp,efficiency,final;
Busy *updater=&update,*file_transfer=&transfer,*dsp_probe=&dsp,*efficiency_probe=&efficiency,*final_check=&final;
struct Dummy{};Dummy object;Dummy* audio=&object;Dummy* engine=&object;
namespace groove {enum class UiNotice {ControllerReconnect};}
struct Performance{void notify(groove::UiNotice){}} performance;
struct Display{void Bind(void*){order.push_back(2);}void Invalidate(){}} display;
struct Observed{void reset(){}} observed_input;
struct Output{void invalidate(){}} led_output;
struct Logger{template<class... T>void Write(T...){}} m_Logger;
unsigned hid_reenumerations=0,hid_recovery_attempts=0,input_resync=9,last_dropped=9,last_hid_progress=9;bool last_screen=true;
CMk3Hid* last_hid=&hid;groove::OutputRetry hid_reenumeration_retry;
bool native_running=false,fx_stress_running=false,audio_stress_preparing=false,can_quiesce=true;
bool QuiesceAudio(Dummy*&,Dummy&,Performance&,bool){order.push_back(1);return can_quiesce;}
void service(){
'''+escalation+r'''
 (void)software_usb_changed;
}
int main(){
 files.on=true;service();assert(order.empty());files.on=false;
 dsp.on=true;service();assert(order.empty());dsp.on=false;
 can_quiesce=false;service();assert(order==std::vector<unsigned>{1} && hid_reenumerations==0 && hid_recovery_attempts==1);order.clear();hid_recovery_attempts=0;CTimer::ticks+=6000000;can_quiesce=true;
 service();assert((order==std::vector<unsigned>{1,2,3,4}) && hid_reenumerations==1 && !last_hid && !last_screen && !input_resync && !last_dropped && !last_hid_progress);order.clear();CTimer::ticks+=6000000;
 service();assert(hid_reenumerations==2);order.clear();CTimer::ticks+=6000000;service();assert(order.empty());
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)/'kernel-recovery.cpp';p.write_text(code);exe=p.with_suffix('');subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Kernel recovery body: recording/probe guards, failed audio join, quiesce/detach/remove/rescan ordering and two-attempt cap passed')
