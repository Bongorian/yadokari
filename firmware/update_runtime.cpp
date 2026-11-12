#include "update_runtime.h"
#include "update_key.h"
#include "../src/update/view.h"
#include "../src/update/pi4_policy.h"
#include "../src/hardware_profile.h"
#include <circle/alloc.h>
#include <circle/memory.h>
#include <circle/devicenameservice.h>
#include <circle/machineinfo.h>
#include <circle/devicetreeblob.h>
#include <circle/bcmpropertytags.h>
#include <circle/net/in.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/util.h>
#ifdef GROOVE_UPDATE_WIFI
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#endif
using namespace groove::update;
unsigned GrooveFirmwareVersion(){return GROOVE_FW_VERSION;}
CUpdateDisk::CUpdateDisk():m_Buffer(static_cast<u8*>(palloc())){}
bool CUpdateDisk::sector(unsigned lba,uint8_t* bytes,bool write){auto* d=CDeviceNameService::Get()->GetDevice("emmc1",TRUE);if(!d||!m_Buffer||d->Seek(u64(lba)*512)!=u64(lba)*512)return false;if(write){memcpy(m_Buffer,bytes,512);return d->Write(m_Buffer,512)==512;}if(d->Read(m_Buffer,512)!=512)return false;memcpy(bytes,m_Buffer,512);return true;}
// The operator EEPROM acknowledgement (EEPROM.OK) is the evidence a
// rev-1.0-class board may use firmware tryboot (the early-Pi 4 caution,
// carried to Pi 5 rev 1.0 by the strict rule). Round 1 on the real Pi 5
// proved the ambient lookup can false-negative: EEPROM.OK sat on partition
// 1 root, yet f_stat("SD:/EEPROM.OK") failed inside supported() at trial
// time. So the probe must not depend on the mount state other flows happen
// to leave behind. It tries the boot volume as found; if nothing has the
// volume registered (this FatFs is built with FF_USE_LABEL == 0, so the
// stat result is the mount probe: FR_NOT_ENABLED means no work area is
// registered for the drive), it mounts the volume into its own static
// FATFS and looks again; it also accepts the DATA volume, which project
// storage keeps mounted in normal operation. It never unmounts or
// replaces a registration owned by someone else, and it reports which
// source satisfied the ack so a boot log can prove where it was found.
enum class EepromAckSource:uint8_t { None, Sd, SdRemount, Data };
struct EepromAck { bool present=false;EepromAckSource source=EepromAckSource::None; };
static const char* EepromAckName(EepromAckSource source){
 switch(source){
 case EepromAckSource::Sd:return "sd";
 case EepromAckSource::SdRemount:return "sd_remount";
 case EepromAckSource::Data:return "data";
 default:return "none";
 }
}
static bool EepromAckFile(const char* path){FILINFO info{};return f_stat(path,&info)==FR_OK&&!(info.fattrib&AM_DIR);}
static EepromAck ProbeEepromAck(){
 EepromAck result{};
 FILINFO info{};const FRESULT first=f_stat("SD:/EEPROM.OK",&info);
 if(first==FR_OK&&!(info.fattrib&AM_DIR)){result.present=true;result.source=EepromAckSource::Sd;return result;}
 if(first==FR_NOT_ENABLED){
  // Left mounted on purpose: deregistering could race a later user of
  // the volume, and a mounted boot volume is the normal state anyway.
  static FATFS mount{};
  if(f_mount(&mount,"SD:",1)==FR_OK&&EepromAckFile("SD:/EEPROM.OK")){result.present=true;result.source=EepromAckSource::SdRemount;return result;}
 }
 if(EepromAckFile("DATA:/EEPROM.OK")){result.present=true;result.source=EepromAckSource::Data;}
 return result;
}
// One gate, two Pi 5 rules: pi5_tryboot_allowed_strict for release builds
// (GROOVE_STRICT_UPDATE_GATE), the relaxed pi5_tryboot_allowed otherwise
// (this tree's default). The Pi 4 rule is the same in both modes; only its
// ack lookup became robust (ProbeEepromAck). Pi 5 tryboot uses the same
// autoboot.txt, mailbox-flag and watchdog-partition mechanism as Pi 4; the
// stock Pi 5 EEPROM bootloader needs no configuration change for
// tryboot_a_b. A caller that already probed (BOOTDIAG) passes its result
// in `probed` instead of probing twice.
static bool TrybootSupported(bool strict_pi5,const EepromAck* probed=nullptr){
 auto* info=CMachineInfo::Get();if(!info)return false;
 const uint32_t revision=info->GetRevisionRaw();
 if(info->GetMachineModel()==MachineModel5){
  if(!strict_pi5)return pi5_tryboot_allowed(revision,false);
  if(pi5_tryboot_allowed_strict(revision,false))return true;
  const EepromAck ack=probed?*probed:ProbeEepromAck();
  return pi5_tryboot_allowed_strict(revision,ack.present);
 }
 if(info->GetMachineModel()!=MachineModel4B)return false;
 if(pi4_tryboot_allowed(revision,false))return true;
 // Early Pi4 revisions require EEPROM write protection to be disabled. This
 // operator-provisioned acknowledgement is never created by the updater.
 const EepromAck ack=probed?*probed:ProbeEepromAck();
 return pi4_tryboot_allowed(revision,ack.present);
}
bool CUpdateReboot::supported()const{
#ifdef GROOVE_STRICT_UPDATE_GATE
 return TrybootSupported(true);
#else
 return TrybootSupported(false);
#endif
}
bool CUpdateReboot::restart(unsigned partition,bool trial){
 if(partition<1||partition>3||(trial&&!supported()))return false;
 CBcmPropertyTags tags;alignas(16) u32 flags[4],notify[4];reboot_flags_tag(flags,trial);reboot_notify_tag(notify);
 // Official firmware mailbox ABI, not an assumed PM_RSTS tryboot bit.
 if(!tags.GetTags(flags,sizeof flags)||!(flags[2]&VALUE_LENGTH_RESPONSE))return false;
 if(!tags.GetTags(notify,sizeof notify))return false;
 CBcmWatchdog watchdog;watchdog.Restart(partition);return false;
}
class CUpdateListener:public CTask {
 CNetSubSystem* m_Net;CSocket* m_Pending=nullptr;u16 m_Port;
public:
 explicit CUpdateListener(CNetSubSystem* net,u16 port=16700):m_Net(net),m_Port(port){SetName("update-accept");}
 CSocket* Take(){auto* result=m_Pending;m_Pending=nullptr;return result;}
 void Run()override{while(!m_Net->IsRunning())CScheduler::Get()->MsSleep(100);CSocket listener(m_Net,IPPROTO_TCP);if(listener.Bind(m_Port)<0||listener.Listen(1)<0)return;for(;;){while(m_Pending)CScheduler::Get()->MsSleep(10);CIPAddress peer;u16 port;m_Pending=listener.Accept(&peer,&port);if(!m_Pending)CScheduler::Get()->MsSleep(100);}}
};
// Read-only status survives a CDC failure while an invalid trial is reverting.
class CTrialStatusListener:public CTask {
 CNetSubSystem* m_Net;CUpdateRuntime* m_Runtime;
public:
 CTrialStatusListener(CNetSubSystem* net,CUpdateRuntime* runtime):m_Net(net),m_Runtime(runtime){SetName("trial-status");}
 void Run()override{
  while(!m_Net->IsRunning())CScheduler::Get()->MsSleep(100);
  CSocket listener(m_Net,IPPROTO_TCP);if(listener.Bind(16701)<0||listener.Listen(1)<0)return;
  for(;;){CIPAddress peer;u16 port;auto* client=listener.Accept(&peer,&port);if(!client){CScheduler::Get()->MsSleep(100);continue;}
   CString result,diagnostics;m_Runtime->RemoteStatus(result);m_Runtime->TrialDiagnostics(diagnostics);result.Append(diagnostics);const char* text=result;unsigned size=strlen(text),sent=0,start=CTimer::GetClockTicks();
   while(sent<size && CTimer::GetClockTicks()-start<2000000){int n=client->Send(text+sent,size-sent,MSG_DONTWAIT);if(n<0)break;if(n>0)sent+=unsigned(n);else CScheduler::Get()->MsSleep(1);}
   delete client;
  }
 }
};
CUpdateRuntime::CUpdateRuntime(CInterruptSystem* interrupt):m_Storage(m_Disk,"SD:","SLOTA:","SLOTB:"),m_Manager(m_Storage,m_Reboot,UpdatePublicKey),m_Receiver(m_Manager),m_Interrupt(interrupt),m_Status("LOCAL UPDATE MODE ONLY"){}

bool CUpdateRuntime::Initialize(bool recovery){
 m_Ready=m_Storage.mount()&&m_Manager.load();
 const bool storage_ready=m_Ready;
 const auto* dtb=CMachineInfo::Get()->GetDTB();if(dtb){auto* node=dtb->FindNode("/chosen/bootloader");if(node){auto* p=dtb->FindProperty(node,"partition");auto* t=dtb->FindProperty(node,"tryboot");if(p&&t&&dtb->GetPropertyValueLength(p)==4&&dtb->GetPropertyValueLength(t)==4){m_Partition=dtb->GetPropertyValueWord(p,0);m_Tryboot=dtb->GetPropertyValueWord(t,0)!=0;m_Identity=m_Partition>=1&&m_Partition<=3;}}}
 if(m_Ready&&m_Identity)m_Ready=m_Manager.on_boot(m_Partition,m_Tryboot,GROOVE_FW_VERSION);
 if(!recovery && !m_Ready && m_Identity && m_Manager.rejected_trial(m_Partition)){m_TrialFallback=true;m_FallbackStart=CTimer::GetClockTicks();}
 if(!recovery && m_Ready && m_Manager.state().phase==Phase::Trial){m_JournalTrial=true;m_FallbackStart=CTimer::GetClockTicks();}
 if(recovery){if(f_mount(&m_DataFs,"DATA:",1)!=FR_OK)m_Status="DATA PARTITION UNAVAILABLE";Enter();m_Autoboot=m_Ready&&m_Identity&&m_Partition==1&&m_Manager.state().phase==Phase::Stable;FILINFO status;if(f_stat("DATA:/RECOVERY.ON",&status)==FR_OK)m_Autoboot=false;}
 // Keep STATUS and logs available after a boot-identity failure; mutating
 // remote commands still require m_Ready and m_Identity via remote_ready().
 FILINFO remote_marker{};
 const bool cdc_marker=f_stat("DATA:/REMOTE.ON",&remote_marker)==FR_OK && !(remote_marker.fattrib&AM_DIR);
#if defined(GROOVE_CDC_DEFAULT_ON) && RASPPI >= 5
 // 0.9.1 Pi 5 release configuration (firmware/circle-pi5.mk): the
 // USB-C CDC gadget is on by default, with the behaviour a Pi 4
 // shows when DATA:/REMOTE.ON is present; no opt-in marker file
 // is required. The marker still works -- it is simply not
 // needed. The define is never set for Pi 4 or development builds.
 const bool cdc_default_on=true;
#else
 const bool cdc_default_on=false;
#endif
 // Gadget enumeration follows the channel decision alone, not update
 // storage readiness: PollUsbControl serves STATUS without
 // remote_ready() precisely so a card whose update layout is not
 // prepared -- the single-partition release card, for example -- can
 // still report the INVALID UPDATE SD LAYOUT status below. m_Remote,
 // which gates every mutating and diagnostic command, keeps requiring
 // prepared storage. tests/cdc_gate_test.py executes this truth table.
 const bool cdc_channel=cdc_marker||cdc_default_on;
 m_Remote=storage_ready && cdc_channel;
 if(cdc_channel && !StartGadget())m_Status="USB-C GADGET INITIALIZATION FAILED";
 if(m_Remote && m_TrialFallback){Enter();StartNetwork(false);}
 if(!m_Ready)m_Status="INVALID UPDATE SD LAYOUT / JOURNAL";
 if(!m_Identity)m_Status="BOOT IDENTITY UNKNOWN / NO AUTO BOOT";
 CLogger::Get()->Write("update",LogNotice,"Compiled version %u; partition %u trial %u; layout/journal %u",unsigned(GROOVE_FW_VERSION),m_Partition,unsigned(m_Tryboot),unsigned(m_Ready));
 return m_Ready;
}
void CUpdateRuntime::Enter(uint16_t held_pads){m_PadLatch.preset(held_pads);m_Active=true;m_Manager.arm(m_Ready&&m_Identity);m_Start=CTimer::GetClockTicks();
}
void CUpdateRuntime::RemoteStatus(CString& out)const{
 CString ip;if(m_Net&&m_Net->IsRunning())m_Net->GetConfig()->GetIPAddress()->Format(&ip);else ip.Format("none");
 const auto& state=m_Manager.state();out.Format("GBX1 STATUS remote=%u active=%u ready=%u armed=%u phase=%u version=%u highest=%u partition=%u trial=%u journal_trial=%u error=%u ip=%s diagnostic_busy=%u\n",unsigned(m_Remote),unsigned(m_Active),unsigned(m_Ready),unsigned(m_Manager.armed()),unsigned(state.phase),unsigned(GROOVE_FW_VERSION),unsigned(state.highest),m_Partition,unsigned(m_Tryboot),unsigned(m_JournalTrial),unsigned(m_Manager.error()),static_cast<const char*>(ip),unsigned(m_DiagnosticBusy));
}
void CUpdateRuntime::TrialDiagnostics(CString& out)const{
 CBcmPropertyTags tags;alignas(16) u32 flags[4];reboot_flags_query_tag(flags);
 bool valid=tags.GetTags(flags,sizeof flags) && (flags[2]&VALUE_LENGTH_RESPONSE) && (flags[2]&~VALUE_LENGTH_RESPONSE)>=4;
 u32 boot_version=0;bool has_version=false;const auto* dtb=CMachineInfo::Get()->GetDTB();
 if(dtb){auto* node=dtb->FindNode("/chosen/bootloader");if(node){auto* property=dtb->FindProperty(node,"version");if(property && dtb->GetPropertyValueLength(property)==4){boot_version=dtb->GetPropertyValueWord(property,0);has_version=true;}}}
 out.Format("GBX1 BOOTDIAG revision=%u rsts=%u flag_valid=%u flags=%u boot_version_valid=%u boot_version=%u\n",CMachineInfo::Get()->GetRevisionRaw(),read32(ARM_PM_RSTS),unsigned(valid),valid?flags[3]:0,unsigned(has_version),boot_version);
}
void CUpdateRuntime::RemoteHardware(CString& out)const{
 // Explicit diagnostic request only: mailbox calls do not run in the audio callback.
 CBcmPropertyTags tags;TPropertyTagClockRate clock{};clock.nClockId=CLOCK_ID_ARM;
 bool clock_ok=tags.GetTag(PROPTAG_GET_CLOCK_RATE_MEASURED,&clock,sizeof clock,4);
 TPropertyTagTemperature temp{};temp.nTemperatureId=TEMPERATURE_ID;
 bool temp_ok=tags.GetTag(PROPTAG_GET_TEMPERATURE,&temp,sizeof temp,4);
 TPropertyTagSimple throttle{};bool throttle_ok=tags.GetTag(PROPTAG_GET_THROTTLED,&throttle,sizeof throttle,4);
 auto* memory=CMemorySystem::Get();
 out.Format("GBX1 HARDWARE arm_valid=%u arm_hz=%u temp_valid=%u temp_mc=%u throttle_valid=%u throttle=%u mapped_ram_mb=%u heap_low_headroom_mb=%u heap_high_headroom_mb=%u cpu_ram_mb=%u heap_cpu_headroom_mb=%u uptime_s=%u\n",unsigned(clock_ok),clock_ok?clock.nRate:0,unsigned(temp_ok),temp_ok?temp.nValue:0,unsigned(throttle_ok),throttle_ok?throttle.nValue:0,unsigned(memory->GetMemSize()/(1024*1024)),unsigned(memory->GetHeapFreeSpace(HEAP_LOW)/(1024*1024)),unsigned(memory->GetHeapFreeSpace(HEAP_HIGH)/(1024*1024)),unsigned(memory->GetCpuMemSize()/(1024*1024)),unsigned(memory->GetHeapFreeSpace(HEAP_CPU)/(1024*1024)),unsigned(CTimer::GetClockTicks64()/1000000));
}
void CUpdateRuntime::RemoteLog(CString& out)const{
 unsigned char bytes[kLogChunkBytes];int n=CLogger::Get()->Read(bytes,sizeof bytes,TRUE);if(n<0)n=0;
 static const char hex[]="0123456789abcdef";char encoded[sizeof bytes*2+1];for(int i=0;i<n;++i){encoded[i*2]=hex[bytes[i]>>4];encoded[i*2+1]=hex[bytes[i]&15];}encoded[n*2]=0;
 out.Format("GBX1 LOG %s\n",n?encoded:"-");
}
bool CUpdateRuntime::Remote(SerialCommand command){
 if(m_DiagnosticBusy)return false;
 if(command==SerialCommand::Ethernet && m_Remote && m_Ready && m_Identity && !m_Active){if(m_NetworkWifi)return false;StartNetwork(false,true);return m_ControlListener!=nullptr;}
 if((command==SerialCommand::Efficiency || command==SerialCommand::EfficiencyTail || command==SerialCommand::EfficiencySampler)){if(!m_Remote || !m_Ready || !m_Identity || m_Active || m_NetworkWifi)return false;StartNetwork(false,true);return m_ControlListener && m_BenchmarkRequests.request(command==SerialCommand::Efficiency?22:command==SerialCommand::EfficiencyTail?23:24);}
 if(command==SerialCommand::DeployCheck || command==SerialCommand::OptimizationTest){if(!m_Remote || !m_Ready || !m_Identity || m_Active)return false;return m_BenchmarkRequests.request(command==SerialCommand::DeployCheck?25:26);}
 if(command==SerialCommand::Benchmark || command==SerialCommand::PlaitsStress || command==SerialCommand::PlaitsFast || command==SerialCommand::DrumDemo || command==SerialCommand::LoopTest || command==SerialCommand::SampleTest || command==SerialCommand::FxStress || command==SerialCommand::SaveTest || command==SerialCommand::KitSwap || command==SerialCommand::ChopTest || command==SerialCommand::KitRecordTest || command==SerialCommand::UiTest || command==SerialCommand::MidiTest || command==SerialCommand::NativeTest || command==SerialCommand::AudioTest || command==SerialCommand::InputTest || command==SerialCommand::PresetCheck || command==SerialCommand::Wave4Test || command==SerialCommand::FinalTest || command==SerialCommand::DspTest || command==SerialCommand::DspStress){if(!m_Remote || !m_Ready || !m_Identity || m_Active)return false;return m_BenchmarkRequests.request(command==SerialCommand::Benchmark?1:command==SerialCommand::PlaitsStress?2:command==SerialCommand::PlaitsFast?3:command==SerialCommand::DrumDemo?4:command==SerialCommand::LoopTest?5:command==SerialCommand::SampleTest?6:command==SerialCommand::FxStress?7:command==SerialCommand::SaveTest?8:command==SerialCommand::KitSwap?9:command==SerialCommand::ChopTest?10:command==SerialCommand::KitRecordTest?11:command==SerialCommand::UiTest?12:command==SerialCommand::MidiTest?13:command==SerialCommand::NativeTest?14:command==SerialCommand::AudioTest?15:command==SerialCommand::InputTest?16:command==SerialCommand::PresetCheck?17:command==SerialCommand::Wave4Test?18:command==SerialCommand::FinalTest?19:command==SerialCommand::DspTest?20:21);}
 if(!m_Remote || !m_Active || !m_Ready || !m_Identity)return false;
 m_Autoboot=false;
 if(command==SerialCommand::Ethernet || command==SerialCommand::Wifi){if(m_Channel!=Idle)return false;bool wifi=command==SerialCommand::Wifi;if(!wifi && m_Net && !m_NetworkWifi && m_ControlListener && !m_Listener)m_Listener=new CUpdateListener(m_Net);if(m_Listener){if(wifi && !m_NetworkWifi)m_WlanPreflight.Start();return m_NetworkWifi==wifi;}StartNetwork(wifi);return m_Listener!=nullptr && m_NetworkWifi==wifi;}
 if(command==SerialCommand::Usb){if(m_Channel!=Idle)return false;return StartGadget();}
 if(m_Channel!=Idle || m_OfflineOpen)return false;
 if(command==SerialCommand::Trial)return m_Manager.trial();
 if(command==SerialCommand::Confirm){bool healthy=m_Ready&&m_Identity&&m_JournalTrial&&m_Partition==unsigned(m_Manager.state().pending)+2&&GROOVE_FW_VERSION==m_Manager.state().slots[m_Manager.state().pending].version;bool ok=m_Manager.confirm(m_Partition,m_JournalTrial,healthy);if(ok)m_JournalTrial=false;return ok;}
 if(command==SerialCommand::Boot)return m_Manager.boot_active();
 if(command==SerialCommand::Cancel){if(!m_Manager.cancel())return false;return m_Reboot.restart(1,false);}
 return false;
}
// pid.codes private-test PID: not unique and unsuitable for distribution.
// https://pid.codes/1209/0001/
bool CUpdateRuntime::StartGadget(){
#ifdef GROOVE_PI5_NO_GADGET
 // PI5 BUILD EXPERIMENT ONLY (not for release): Circle's USB gadget library
 // builds empty for RASPPI=5, so no CDC gadget can be created. Report the
 // same failure status this runtime already uses for a failed gadget init;
 // every caller already handles a false return.
 m_Status="USB-C GADGET INITIALIZATION FAILED";return false;
#else
 if(!m_Gadget){m_Gadget=new CUSBCDCGadget(m_Interrupt,0x1209,0x0001);m_GadgetReady=m_Gadget->Initialize();if(!m_GadgetReady)m_Status="USB-C GADGET INITIALIZATION FAILED";}return m_GadgetReady;
#endif
}
void CUpdateRuntime::StartNetwork(bool wifi,bool diagnostics){
 if((!m_Ready&&!m_TrialFallback)||m_Net||m_Manager.receiving())return;
 #ifndef GROOVE_UPDATE_WIFI
 if(wifi){m_Status="WI-FI NOT INCLUDED IN THIS BUILD";return;}
#else
 if(wifi){auto* wlan=new CBcm4343Device("DATA:/firmware/");if(!wlan->Initialize()){m_Status="WI-FI FIRMWARE / DRIVER FAILED";return;}}
#endif
 m_Net=new CNetSubSystem(nullptr,nullptr,nullptr,nullptr,"groovebox-update",wifi?NetDeviceTypeWLAN:NetDeviceTypeEthernet);
 if(!m_Net->Initialize(FALSE)){m_Status="NETWORK INITIALIZATION FAILED";return;}
 #ifdef GROOVE_UPDATE_WIFI
 if(wifi){auto* wpa=new CWPASupplicant("DATA:/wpa_supplicant.conf");if(!wpa->Initialize()){m_Status="WPA CONFIGURATION FAILED";return;}}
#endif
 if(m_TrialFallback){new CTrialStatusListener(m_Net,this);m_Status="TRIAL FAILED / STATUS TCP 16701";}
 else{if(!diagnostics)m_Listener=new CUpdateListener(m_Net);m_ControlListener=new CUpdateListener(m_Net,16701);}
 m_NetworkWifi=wifi;if(!m_TrialFallback)m_Status=diagnostics?"WAITING FOR DHCP / DIAGNOSTICS TCP 16701":"WAITING FOR DHCP / TCP 16700";
}
// A single Circle network stack is selected on first use after boot. Normal
// playback never starts it merely because the private REMOTE marker exists.
CNetSubSystem* CUpdateRuntime::BeginFilesNetwork(const char* ssid,bool station){
#ifdef GROOVE_UPDATE_WIFI
 if(m_Active || m_Manager.receiving()){m_Status="UPDATE BUSY";return nullptr;}
 if(m_Net){if(station==m_FileAP){m_Status="RESTART TO CHANGE WI-FI MODE";return nullptr;}if(!m_NetworkWifi || !m_FileWlan){m_Status="RESTART TO SELECT WI-FI";return nullptr;}if(m_FileAP){if(!m_FileWlan->CreateOpenNet(ssid,6,false)){m_Status="AP START FAILED";return nullptr;}}else if(!m_FileWpa){m_FileWpa=new CWPASupplicant("DATA:/wpa_supplicant.conf");if(!m_FileWpa->Initialize()){m_Status="WI-FI CONFIGURATION FAILED";return nullptr;}}return m_Net;}
 if(m_FileWlan){m_Status="RESTART AFTER WI-FI ERROR";return nullptr;}
 m_FileWlan=new CBcm4343Device("DATA:/firmware/");if(!m_FileWlan->Initialize()){m_Status="WI-FI DRIVER FAILED";return nullptr;}m_FileWlanInitialized=true;
 m_FileAP=!station && m_FileWlan->CreateOpenNet(ssid,6,false);
 if(!m_FileAP){FILINFO cfg;if(f_stat("DATA:/wpa_supplicant.conf",&cfg)!=FR_OK){m_Status="AP FAILED / WI-FI CONFIG MISSING";return nullptr;}}
 static const u8 ip[]={192,168,76,1},mask[]={255,255,255,0};
 m_Net=new CNetSubSystem(m_FileAP?ip:nullptr,m_FileAP?mask:nullptr,nullptr,nullptr,"groovebox",NetDeviceTypeWLAN);
 if(!m_Net->Initialize(FALSE)){m_Status="WI-FI NETWORK FAILED";return nullptr;}
 if(!m_FileAP){m_FileWpa=new CWPASupplicant("DATA:/wpa_supplicant.conf");if(!m_FileWpa->Initialize()){m_Status="WI-FI CONFIGURATION FAILED";return nullptr;}}
 m_NetworkWifi=true;return m_Net;
#else
 (void)ssid;(void)station;m_Status="WI-FI NOT INCLUDED";return nullptr;
#endif
}
bool CUpdateRuntime::EndFilesNetwork(){
 bool ok=true;
#ifdef GROOVE_UPDATE_WIFI
 if(m_FileWpa){delete m_FileWpa;m_FileWpa=nullptr;}
 if(m_FileWlanInitialized)ok=m_FileAP?m_FileWlan->DestroyOpenNet():m_FileWlan->Control("down");
#endif
 CLogger::Get()->Write("files",ok?LogNotice:LogWarning,"Wi-Fi transfer radio stopped %u",unsigned(ok));return ok;
}
CSocket* CUpdateRuntime::TakeControl(){return m_ControlListener?m_ControlListener->Take():nullptr;}
void CUpdateRuntime::CloseControl(){if(m_ControlClient){delete m_ControlClient;m_ControlClient=nullptr;}m_NetControl={};m_NetReplyUsed=m_NetReplySent=0;}
void CUpdateRuntime::Reply(int result){const char* text=result==1?"READY\n":result==2?"OK\n":result==3?"VERIFIED\n":"ERROR rejected\n";strcpy(m_Reply,text);m_ReplyUsed=strlen(text);m_ReplySent=0;if(result<0)m_Status=error_name(m_Manager.error());}
void CUpdateRuntime::Disconnect(){m_Receiver.reset();if(m_Client){delete m_Client;m_Client=nullptr;}m_Channel=Idle;m_ReplyUsed=m_ReplySent=0;m_LastSerial=nullptr;}
void CUpdateRuntime::Offline(){
 if(!m_Ready||m_Channel!=Idle||m_OfflineOpen)return;
 if(f_open(&m_Offline,"DATA:/UPDATE.GUP",FA_READ)!=FR_OK){m_Status="DATA:/UPDATE.GUP NOT FOUND";return;}
 uint8_t header[HeaderSize];UINT n=0;bool ok=f_read(&m_Offline,header,HeaderSize,&n)==FR_OK&&n==HeaderSize&&f_size(&m_Offline)==HeaderSize+get32(header+16);
 m_Receiver.reset();if(!ok||m_Receiver.feed(header,HeaderSize)!=1){f_close(&m_Offline);m_Status=error_name(m_Manager.error());return;}m_OfflineOpen=true;m_Channel=OfflineChannel;m_Status="OFFLINE RECEIVE / VERIFY";
}
void CUpdateRuntime::Control(unsigned byte,uint8_t value,bool baseline){
 uint8_t rising=m_ButtonLatch.update(byte,value,baseline);
 static const unsigned action[]={0,1,2,3,5,6,7,15};
 for(unsigned i=0;i<8;++i)if(rising&(1u<<i))Action(action[i]);
}
void CUpdateRuntime::Pad(unsigned hardware,uint8_t tag){
 if(!m_PadLatch.press(hardware,tag))return;
 Action((3-hardware/4)*4+hardware%4);
}
void CUpdateRuntime::Action(unsigned pad){
 m_Autoboot=false;
 if(pad==0)StartNetwork(false);else if(pad==1)StartNetwork(true);else if(pad==2)Offline();
 else if(pad==3&&!m_Gadget){m_Status=StartGadget()?"USB-C CDC / ttyACM ON HOST":"USB-C GADGET INITIALIZATION FAILED";}
 else if(pad==5&&m_Channel==Idle)m_Manager.trial();
 else if(pad==6&&m_Channel==Idle){bool healthy=m_Ready&&m_Identity&&m_JournalTrial&&GROOVE_FW_VERSION==m_Manager.state().slots[m_Manager.state().pending].version;if(m_Manager.confirm(m_Partition,m_JournalTrial,healthy))m_JournalTrial=false;}
 else if(pad==7&&m_Channel==Idle)m_Manager.boot_active();
 else if(pad==8&&m_Channel==Idle){if(m_Manager.rollback_confirmed())m_Reboot.restart(1,false);}
 else if(pad==15){if(m_OfflineOpen){f_close(&m_Offline);m_OfflineOpen=false;}Disconnect();m_Manager.cancel();m_Reboot.restart(1,false);}
}
void CUpdateRuntime::PollTrialFallback(){
 if(m_TrialFallback && CTimer::GetClockTicks()-m_FallbackStart>=30000000){
  m_TrialFallback=false;
  CLogger::Get()->Write("update",LogWarning,"Rejected trial: returning to recovery");
  m_Reboot.restart(1,false);
 }
 if(journal_trial_expired(m_JournalTrial,m_Manager.state().phase==Phase::Trial,CTimer::GetClockTicks(),m_FallbackStart)){
  m_JournalTrial=false;
  CLogger::Get()->Write("update",LogWarning,"Unconfirmed trial timed out: returning to recovery");
  m_Reboot.restart(1,false);
 }
}
void CUpdateRuntime::Process(){
 m_WlanPreflight.Poll();
 if(!m_Active)return;
 unsigned now=CTimer::GetClockTicks();
 if((m_Channel==Serial||m_Channel==TCP)&&now-m_LastActivity>30000000){Disconnect();m_Status="RECEIVE TIMEOUT / RETRY PACKAGE";return;}
 if(m_Autoboot&&now-m_Start>=5000000){m_Autoboot=false;m_Manager.boot_active();}
 if(m_Net&&m_Net->IsRunning()){CString ip;m_Net->GetConfig()->GetIPAddress()->Format(&ip);m_Status.Format(m_TrialFallback?"TRIAL STATUS TCP %s:16701":"TCP %s:16700 / SIGNED PACKAGE ONLY",static_cast<const char*>(ip));}
 if(m_Channel==OfflineChannel){uint8_t buffer[4096];UINT n=0;unsigned wanted=m_Receiver.wanted();int result=-1;if(f_read(&m_Offline,buffer,wanted,&n)==FR_OK&&n==wanted)result=m_Receiver.feed(buffer,n);if(result<0||result==3){f_close(&m_Offline);m_OfflineOpen=false;m_Channel=Idle;if(result<0)m_Receiver.reset();m_Status=result==3?"VERIFIED / PAD 6 TO TRYBOOT":"OFFLINE UPDATE FAILED";}return;}
 auto* serial=m_Gadget?m_Gadget->GetSerialDevice():nullptr;
 if(m_Channel==Serial&&serial!=m_LastSerial){Disconnect();m_Status="USB-C DISCONNECTED / RETRY PACKAGE";return;}
 if(m_Channel==Idle&&m_Listener){m_Client=m_Listener->Take();if(m_Client){m_Channel=TCP;m_Receiver.reset();m_LastActivity=now;}}
 if(m_ReplySent<m_ReplyUsed){int n=m_Channel==TCP?m_Client->Send(m_Reply+m_ReplySent,m_ReplyUsed-m_ReplySent,MSG_DONTWAIT):serial?serial->Write(m_Reply+m_ReplySent,m_ReplyUsed-m_ReplySent):-1;if(n<0){Disconnect();return;}m_ReplySent+=unsigned(n);if(n>0)m_LastActivity=now;if(m_ReplySent==m_ReplyUsed&&m_Receiver.done()){Disconnect();m_Status="TRANSFER FINISHED / INSPECT STATUS";}return;}
 uint8_t buffer[4096];int n=m_Channel==TCP?m_Client->Receive(buffer,sizeof buffer,MSG_DONTWAIT):m_Channel==Serial&&serial?serial->Read(buffer,m_Receiver.wanted()):0;
 if(n<0){Disconnect();return;}if(n>0){m_LastActivity=now;int result=m_Receiver.feed(buffer,unsigned(n));if(result)Reply(result);}
 if(m_Channel!=Idle&&now-m_LastActivity>30000000){Disconnect();m_Status="RECEIVE TIMEOUT / RETRY PACKAGE";}
}
void CUpdateRuntime::Draw(groove::SoftwareCanvas& renderer){draw_view(renderer,m_Manager,static_cast<const char*>(m_Status),m_Partition,m_JournalTrial,GROOVE_FW_VERSION);}
void CUpdateRuntime::Leds(uint8_t pads[16],uint8_t buttons[62],const groove::ui::Theme& theme){bool confirm=m_Identity&&m_JournalTrial&&m_Partition==unsigned(m_Manager.state().pending)+2&&GROOVE_FW_VERSION==m_Manager.state().slots[m_Manager.state().pending].version;controller_leds(m_Manager,m_Channel!=Idle,confirm,pads,buttons,theme);}
