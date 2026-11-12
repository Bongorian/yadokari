#include "deployment_check.h"
#include "diagnostic_panic_tap.h"
#include "boot_trace_config.h"
#include "../src/preset_functional_check.h"
#include "../src/factory_library_audit.h"
#include "final_device_check.h"
#include "kit_record_check.h"
#include "../src/midi_loopback_probe.h"
#include "../src/midi_output_pump.h"
#include "../src/midi_input_router.h"
#include "../src/alloc_policy.h"
#include "../src/hardware_profile.h"
#include <circle/machineinfo.h>
#include "cpu_memory.h"
// Circle initialization derived from Circle sample 05 (GPL-3.0-or-later).
#include "kernel.h"
#include "kernel_size_marker.h"
#include "mk3_usb.h"
#include "mk3_audio.h"
#include "mk3_midi.h"
#include "update_runtime.h"
#include "file_transfer.h"
#include "serial_update_control.h"
#include "circle_display.h"
#include "display_calibration.h"
#include "storage_ui_wait.h"
#include "fluid_probe.h"
#include "display_worker.h"
#include "dsp_probe.h"
#include "efficiency_probe.h"
#include "ui_probe.h"
#include "../src/update/preparation.h"
#include "../src/mk3_output.h"
#include "../src/performance.h"
#include "../src/ui_renderer.h"
#include "../src/demo_project.h"
#include "../src/controller_output.h"
#include "../src/output_retry.h"
#include "../src/display_pacing.h"
#include "../src/benchmark_project.h"
#include "../src/native_stress_project.h"
#include "../src/wave4_workload.h"
#include "../src/audio_stress_assets.h"
extern "C" unsigned GrooveXHCIIsoEvents(unsigned kind,unsigned endpoint);
#include "../src/audio_drain_monitor.h"
#include "../src/session_files.h"
#include <circle/usb/usbaudiostreaming.h>
#include <circle/koptions.h>
#include <circle/usb/usbdevice.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/alloc.h>
#include <circle/cputhrottle.h>
static const char FromKernel[]="groove";
static uint32_t AudioProfileClock(){return CTimer::GetClockTicks();}
static bool FreshProbeSong(groove::IFileAccess& fs,char (&path)[96]){
 for(unsigned i=1;i<1000;++i){groove::copy_text(path,sizeof path,"GROOVE/CHECK-000.mgxs");path[13]=char('0'+i/100);path[14]=char('0'+i/10%10);path[15]=char('0'+i%10);
  bool used=fs.exists(path);const char* suffixes[]={".tmp",".new",".bak"};for(const char* suffix:suffixes){char other[112];groove::copy_text(other,sizeof other,path);unsigned n=0;while(other[n])++n;groove::copy_text(other+n,sizeof other-n,suffix);used|=fs.exists(other);}if(!used)return true;
 }path[0]=0;return false;
}
static bool RemoveProbeSong(groove::IFileAccess& fs,const char* path){if(!path[0])return true;bool ok=fs.remove(path);const char* suffixes[]={".tmp",".new",".bak"};for(const char* suffix:suffixes){char other[112];groove::copy_text(other,sizeof other,path);unsigned n=0;while(other[n])++n;groove::copy_text(other+n,sizeof other-n,suffix);ok=fs.remove(other) && ok;}return ok;}

struct KitProbePhases{CMk3Audio** audio;unsigned last_us=0,last_missing=0;};

static void KitProbePhase(void* context,unsigned phase){auto& p=*static_cast<KitProbePhases*>(context);unsigned now=CTimer::GetClockTicks(),missing=*p.audio?(*p.audio)->worker_missing():0;CLogger::Get()->Write(FromKernel,LogNotice,"Kit phase %u elapsed-us %u missing %u",phase,uint32_t(now-p.last_us),missing-p.last_missing);p.last_us=now;p.last_missing=missing;}

CKernel::CKernel():m_Screen(m_Options.GetWidth(),m_Options.GetHeight()),m_Serial(&m_Interrupt),m_Timer(&m_Interrupt),m_Logger(m_Options.GetLogLevel(),&m_Timer),m_USBHCI(&m_Interrupt,&m_Timer,TRUE),m_EMMC(&m_Interrupt,&m_Timer,&m_ActLED) {}
CKernel::~CKernel() {}
static CMk3Display * SelectMk3Display(CMk3Hid*,CMk3Display*);
void CKernel::BootTrace(const char* stage){
#if GROOVE_BOOT_TRACE
 if(m_BootGadget){m_BootGadget->UpdatePlugAndPlay();auto* serial=m_BootGadget->GetSerialDevice();if(serial && !m_BootConsole){m_Logger.Initialize(serial);m_BootConsole=true;}}
 m_Logger.Write("boot-stage",LogNotice,"%s",stage);
#else
 (void)stage;
#endif
}

boolean CKernel::Initialize() {
 // Interrupts must be initialized before the UART registers its handler.
 if(!m_Interrupt.Initialize()) return FALSE;
 if(!m_Serial.Initialize(115200)) return FALSE;
 static const char started[]="groove boot: serial ready\n";
 m_Serial.Write(started,sizeof(started)-1);
 if(!m_Logger.Initialize(&m_Serial)) {static const char message[]="groove boot: logger initialization failed\n";m_Serial.Write(message,sizeof(message)-1);return FALSE;}
 if(!m_Timer.Initialize()) {static const char message[]="groove boot: timer initialization failed\n";m_Serial.Write(message,sizeof(message)-1);return FALSE;}
#if GROOVE_BOOT_TRACE
 m_BootGadget=new CUSBCDCGadget(&m_Interrupt,0x1209,0x0001);
 if(m_BootGadget && !m_BootGadget->Initialize()){delete m_BootGadget;m_BootGadget=nullptr;}
#endif
 m_Logger.Write(FromKernel,LogNotice,"Application firmware %u: initializing USB host",GrooveFirmwareVersion());
 m_Logger.Write(FromKernel,LogNotice,"Boot raw PM_RSTS %08x (before application watchdog selection)",read32(ARM_PM_RSTS));
 m_Watchdog.Start(15);
 if(!m_USBHCI.Initialize()){m_Logger.Write(FromKernel,LogError,"USB host initialization failed");return FALSE;}
 m_Logger.Write(FromKernel,LogNotice,"USB host initialized; starting instrument");
 auto* hid=static_cast<CMk3Hid*>(m_DeviceNameService.GetDevice("mk3hid",FALSE));
 auto* screen=SelectMk3Display(hid,static_cast<CMk3Display*>(m_DeviceNameService.GetDevice("mk3display",FALSE)));
 BootTrace("USB host ready / allocating display");
 m_Display=new CircleDisplay((u8*)palloc());
 BootTrace("Display allocated / submitting splash");
 auto circle_us=[&](){unsigned seconds=0,micros=0;m_Timer.GetUptime(&seconds,&micros);return seconds*1000000u+micros;};
 unsigned submitted=CTimer::GetClockTicks(),submitted_circle=circle_us();bool sent=m_Display && m_Display->BootSplash(screen),ack=false;
 BootTrace("Splash submitted / awaiting ACK");
 if(sent){while(!(ack=m_Display->BootSplashComplete()) && uint32_t(CTimer::GetClockTicks()-submitted)<1000000u)m_Timer.MsDelay(1);}
 m_Logger.Write(FromKernel,ack?LogNotice:LogWarning,"Boot splash first-frame submitted-us %u acknowledged-us %u both %u",submitted_circle,circle_us(),unsigned(ack));
 BootTrace("Splash completed / initialize returns");
 return TRUE;
}
// Find a Circle audio-control index belonging to the actual MK3, not another USB interface.
static int FindMk3Audio(const CUSBDevice* target,unsigned endpoint,unsigned& output_index) {
 if(!target)return -1;
 for(unsigned device=0;device<16;++device){unsigned ordinal=0;for(unsigned iface=0;iface<16;++iface){
  CString name;name.Format("uaudio%u-%u",device+1,iface+1);
  auto* stream=static_cast<CUSBAudioStreamingDevice*>(CDeviceNameService::Get()->GetDevice(name,FALSE));
  if(!stream)continue;
  auto info=stream->GetDeviceInfo();if(!info.IsOutput)continue;
  const auto* desc=stream->GetDevice()->GetDeviceDescriptor();
  if(stream->GetDevice()==target && desc && desc->idVendor==0x17cc && desc->idProduct==0x1600 && info.NumChannels>=2 && info.NumChannels<=4 && (!endpoint || stream->GetDataEndpointAddress()==endpoint)){
   for(unsigned r=0;r<info.SampleRateRanges;++r){const auto& range=info.SampleRateRange[r];if(48000>=range.Min && 48000<=range.Max && (!range.Resolution || (48000-range.Min)%range.Resolution==0)){output_index=ordinal;return int(device);}}
  }
  ++ordinal;
 }}return -1;
}
static int FindMk3Input(const CUSBDevice* target,unsigned device,unsigned endpoint,groove::HardwareDiagnostics& diagnostics){if(!target)return -1;int chosen=-1;unsigned input_index=0;for(unsigned iface=0;iface<16;++iface){CString name;name.Format("uaudio%u-%u",device+1,iface+1);auto* stream=static_cast<CUSBAudioStreamingDevice*>(CDeviceNameService::Get()->GetDevice(name,FALSE));if(!stream)continue;const auto* desc=stream->GetDevice()->GetDeviceDescriptor();auto info=stream->GetDeviceInfo();if(stream->GetDevice()!=target || !desc || desc->idVendor!=0x17cc || desc->idProduct!=0x1600){if(!info.IsOutput)++input_index;continue;}if(info.IsOutput && !diagnostics.tx_channels)diagnostics.tx_channels=info.NumChannels;if(!info.IsOutput && !diagnostics.rx_channels)diagnostics.rx_channels=info.NumChannels;CLogger::Get()->Write("audio",LogNotice,"Interface %u %s channels %u rate ranges %u",iface,info.IsOutput?"TX":"RX",info.NumChannels,info.SampleRateRanges);if(!info.IsOutput){if(chosen<0 && info.NumChannels>=1 && info.NumChannels<=2 && (!endpoint || stream->GetDataEndpointAddress()==endpoint)){for(unsigned r=0;r<info.SampleRateRanges;++r){const auto& range=info.SampleRateRange[r];if(48000>=range.Min && 48000<=range.Max && (!range.Resolution || (48000-range.Min)%range.Resolution==0)){chosen=int(input_index);break;}}}++input_index;}}return chosen;}
// A canceled stream remains owned until Circle has drained all callbacks.
static bool ReconcileAudioBinding(CMk3Audio* audio,const CUSBDevice* owner,const CUSBDevice* selected){
 if(!audio || !owner || owner==selected || !audio->operational())return false;
 audio->Cancel();
 return true;
}
// Never draw a controller's state on a display from another USB device.
static CMk3Display* SelectMk3Display(CMk3Hid* hid,CMk3Display* display){
 return hid && display && hid->GetDevice()==display->GetDevice()?display:nullptr;
}
// Shared task-owned handoff. Only render from core 0 after cancellation has
// completed and deleting CMk3Audio has joined the sole DSP worker.
static bool QuiesceAudio(CMk3Audio*& audio,groove::Engine& engine,groove::Performance& performance,bool reset_controls){
 if(reset_controls)performance.release_all();else performance.release_input_pads();
 if(audio){audio->Cancel();unsigned start=CTimer::GetClockTicks();while(audio->IsActive() && CTimer::GetClockTicks()-start<2000000)CTimer::Get()->MsDelay(1);if(audio->IsActive())return false;delete audio;audio=nullptr;}
 float scratch[2];engine.render(scratch,0);performance.poll();if(!performance.stop())return false;engine.render(scratch,0);performance.poll();return true;
}
class CPrepareUpdate:public groove::update::Preparation {
 CMk3Audio*& audio_;groove::Engine& engine_;groove::Performance& performance_;CProjectStorage& storage_;groove::ProjectState& project_;CUpdateRuntime& updater_;uint16_t held_pads_;
public:
 CPrepareUpdate(CMk3Audio*& audio,groove::Engine& engine,groove::Performance& performance,CProjectStorage& storage,groove::ProjectState& project,CUpdateRuntime& updater):audio_(audio),engine_(engine),performance_(performance),storage_(storage),project_(project),updater_(updater),held_pads_(performance.held_pad_mask()){}
 bool stop_and_quiesce()override{return !updater_.diagnostic_busy() && QuiesceAudio(audio_,engine_,performance_,true);}

 bool save_project()override{if(!performance_.require_project_save_safe())return false;performance_.sync_ui_settings();bool saved=storage_.SaveUpdateSnapshot(project_,engine_);if(!saved)performance_.require_project_save_safe();return saved;}
 void enter_update_mode()override{updater_.Enter(held_pads_);}
};

__attribute__((noinline,optimize("Os"))) TShutdownMode CKernel::Run() {
 if(m_Display)m_Display->BootSplashVersion(GrooveFirmwareVersion());
 BootTrace("Run entered / CPU clock");
 // Request the board's firmware-reported maximum, without overclocking. Circle
 // monitors temperature and returns to its safe low clock above its configured limit.
 CCPUThrottle cpu_throttle(CPUSpeedMaximum);bool cpu_monitor_failed=false;
 m_Logger.Write(FromKernel,LogNotice,"CPU clock control dynamic %u min %u max %u",unsigned(cpu_throttle.IsDynamic()),cpu_throttle.GetMinClockRate(),cpu_throttle.GetMaxClockRate());
 // HardwareProfile: decode board + RAM once, before the engine exists;
 // every budget decision afterwards reads the active profile.
 groove::hardware::set_active_profile(groove::hardware::decode(CMachineInfo::Get()->GetRevisionRaw()));
 const auto& hw_profile=groove::hardware::active_profile;
 m_Logger.Write(FromKernel,LogNotice,"Hardware profile board %u ram %u MB cpu heap %u MB budget scale %u permille baseline %u",unsigned(hw_profile.board),hw_profile.ram_mb,hw_profile.cpu_heap_bytes/(1024*1024),hw_profile.budget_scale_permille,unsigned(hw_profile.baseline));
 m_Storage.SetWatchdog(&m_Watchdog);
 auto* engine=groove::alloc_cpu_new<groove::Engine>(48000);
 auto* project=groove::alloc_cpu_new<groove::ProjectState>();
 // Boot-critical allocations (SPEC section 8, item M4): log and halt
 // instead of dereferencing a failed allocation.
 if(!engine || !project){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (engine/project)");return ShutdownHalt;}
 BootTrace("Engine and project allocated / default state");
 groove::make_demo(*project,false);
 track_at(*project,0,15).instrument=groove::Instrument::DaisySynth;
 BootTrace("Default state ready / SD init");
 groove::RecoverySource initial_recovery;if(m_EMMC.Initialize() && m_Storage.Mount()) { BootTrace("SD mounted / factory prepare"); if(!m_Storage.PrepareContent())m_Logger.Write(FromKernel,LogWarning,"DATA factory preparation incomplete / retry next boot"); BootTrace("Factory returned / project load"); if(groove::FileReplace::pending(m_Storage)){m_Logger.Write(FromKernel,LogError,"DATA transaction requires recovery; refusing project writes");return ShutdownHalt;} if(m_Storage.Load(*project,&initial_recovery)) m_Logger.Write(FromKernel,LogNotice,"Saved project loaded"); else m_Logger.Write(FromKernel,LogNotice,"No valid project; using defaults"); } else m_Logger.Write(FromKernel,LogWarning,"SD unavailable; save disabled");
 BootTrace("Project load returned / sample preparation");
 groove::WavView sample_bindings[groove::kSampleAssets]{};
 if(!m_Storage.prepare(*project,sample_bindings) || !engine->stage_project(*project,sample_bindings)) {
  m_Logger.Write(FromKernel,LogWarning,"Saved project has missing sample assets or unsupported runtime settings; using defaults without modifying the file");
  initial_recovery={};for(auto& view:sample_bindings)view={};delete project; project=groove::alloc_cpu_new<groove::ProjectState>();if(!project){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (default project)");return ShutdownHalt;}
  groove::make_demo(*project,false);
  if(!engine->stage_project(*project)) m_Logger.Write(FromKernel,LogError,"Default project snapshot failed");
 }
 BootTrace("Project staged / initial render");
 float initial_audio[2];engine->render(initial_audio,0);
 BootTrace("Initial render done / controls and session");
 auto* midi=new CMk3Midi(); CMk3Audio* audio=nullptr; const CUSBDevice* audio_owner=nullptr; auto* performance_object=groove::alloc_cpu_new<groove::Performance>(*engine,*project); auto* renderer=groove::alloc_cpu_new<groove::UiRenderer>(); if(!midi || !performance_object || !renderer || !m_Display){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (midi/performance/renderer/display)");return ShutdownHalt;} auto& performance=*performance_object; u8 pads[16]={},buttons[62]={}; groove::ControllerOutputPump led_output; auto& display=*m_Display;
 display.BootSplashVersion(GrooveFirmwareVersion()); // Retry only if the static ACK arrived late.
 performance.animate(CTimer::GetClockTicks64()/1000);if(initial_recovery.kind!=groove::RecoveryKind::None && initial_recovery.kind!=groove::RecoveryKind::Main)performance.notify_recovery(initial_recovery);
 auto* files_object=groove::alloc_cpu_new<groove::SessionFiles>(m_Storage,m_Storage,*engine,*project,performance);if(!files_object){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (session files)");return ShutdownHalt;}auto& files=*files_object;files.project_locator("GROOVE/CURRENT.TXT");performance.project_path(m_Storage.CurrentProject());files.set_bindings(sample_bindings);if(!files.sync_preserved_from(m_Storage.preserved())){m_Logger.Write(FromKernel,LogError,"Boot preservation handoff failed");return ShutdownHalt;}
 auto* updater=new CUpdateRuntime(&m_Interrupt);if(!updater){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (update runtime)");return ShutdownHalt;}BootTrace("Handing gadget and logging to updater");
#if GROOVE_BOOT_TRACE
 if(m_BootGadget)updater->AdoptBootGadget(m_BootGadget);
 if(m_BootConsole){m_Logger.Initialize(&m_Serial);m_BootConsole=false;}
#endif
 updater->Initialize(false);
 auto* file_transfer=new CFileTransfer(m_Storage,*project,performance,*updater);if(!file_transfer){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (file transfer)");return ShutdownHalt;}
 {uint32_t checkpoint=0;unsigned got=0;if(m_Storage.exists("GROOVE/WaveOneCheck.chk") && m_Storage.read("GROOVE/WaveOneCheck.chk",0,&checkpoint,sizeof checkpoint,got) && got==sizeof checkpoint && (checkpoint&0xffff0000u)==0x57430000u)m_Logger.Write(FromKernel,LogWarning,"Interrupted preset check checkpoint engine %u stage %u detail %u",unsigned(checkpoint&255u)/16,unsigned(checkpoint&15u),unsigned((checkpoint>>8)&255u));}
 groove::PresetFunctionalCheck preset_check(files,m_Storage,*engine,*project,performance);unsigned preset_check_progress=0,preset_heap_min=~0u;
 auto* final_check=groove::alloc_new<groove::FinalDeviceCheck>(files,m_Storage,*engine,*project,performance);
 groove::DeploymentCheck deployment_check(*engine,*project,performance,files,m_Storage);
 groove::update::SerialControl serial_control;
 bool midi_resync_pending=false,midi_transport_resync_pending=false;const CMk3Hid* last_hid=nullptr;bool last_midi_connected=false; bool last_screen=false; bool midi_cables_seen[16]={};u32 input_resync=0;
 groove::AudioStressAssets* audio_stress=nullptr;bool native_mixed=false,audio_stress_preparing=false;unsigned native_stream_misses=0;
 bool wave4=false,wave4_restore=false,wave4_save=false;groove::ProjectState* wave4_original=nullptr;groove::WavView wave4_bindings[64]{};uint32_t wave4_restored_generation=0;char native_record_paths[20][96]{};unsigned native_records=0;
 CDspProbe* dsp_probe=nullptr;bool optimization_dsp_pending=false,optimization_fluid_pending=false,optimization_suite=false;
 bool native_running=false,native_profile=false,native_stop_requested=false;unsigned native_case=0,native_underrun=0,native_overrun=0,native_missing=0;uint64_t native_start=0,native_tone=0,native_profile_start=0;CMk3Audio* native_audio=nullptr;
 bool fx_stress_running=false,fx_profile_started=false;uint64_t fx_stress_start=0,fx_stress_next_morph=0;CMk3Audio* fx_stress_audio=nullptr;unsigned fx_start_underrun=0,fx_start_overrun=0,fx_start_missing=0,fx_stress_last_throw=255;
 const bool trace_requested=CKernelOptions::Get()->GetAppOptionDecimal("mk3trace",0)!=0;bool startup_health_logged=false;unsigned reported_faults[11]{};
 unsigned hid_reenumerations=0,hid_recovery_attempts=0;groove::OutputRetry hid_reenumeration_retry;
 unsigned last_audio_attempt=0,last_led=0, last_dropped=0, last_hid_progress=0,last_command_drops=0, last_midi_drops=0, last_stats=0,midi_output_failures=0;groove::AudioDrainMonitor audio_drain;groove::DisplayPacing display_pacing;groove::UiFrameClock frame_clock;uint64_t frame_window=0;groove::MidiOutputPump midi_output;groove::OutputRetry hid_retry,midi_retry,led_retry[2],display_retry[2];auto* midi_input=groove::alloc_new<groove::MidiInputRouter>();if(!midi_input){m_Logger.Write(FromKernel,LogError,"Boot allocation failed (midi input)");return ShutdownHalt;}
 groove::MidiLoopbackProbe midi_probe;bool midi_probe_reported=true;unsigned midi_probe_missing=0,midi_probe_out=0,midi_probe_in=0;
 struct SaveStages {uint64_t start=0,ui_start=0;unsigned max[7]{},total[7]{},ui[7]{};} save_stages;
 files.set_project_save_observer([](void* context,unsigned phase,bool begin){auto& s=*static_cast<SaveStages*>(context);if(phase>=7)return;if(begin){s.start=CTimer::GetClockTicks64();s.ui_start=StorageUiWait::pump_us;}else{unsigned us=unsigned(CTimer::GetClockTicks64()-s.start);if(us>s.max[phase])s.max[phase]=us;s.total[phase]+=us;s.ui[phase]+=unsigned(StorageUiWait::pump_us-s.ui_start);}},&save_stages);
 CUiProbe ui_probe;CFluidProbe fluid_probe;CDisplayCalibration display_calibration;bool calibrated_ui_pending=false;
 unsigned last_loop_us=CTimer::GetClockTicks(),max_loop_us=0,max_draw_us=0,max_draw_minus_audio_us=0,draw_timed_count=0,draw_count=0;char save_probe_path[96]{};uint64_t ui_service_last=0;unsigned ui_service_gap=0,ui_service_over_frame=0;uint64_t save_probe_started_ms=0,chop_probe_end_ms=0;unsigned save_service_max_us=0,save_snapshot_us=0,kit_probe_started_us=0,kit_probe_missing=0,kit_probe_out=0,kit_probe_in=0,kit_probe_track=0,kit_service_max_us=0,chop_probe_missing=0,chop_probe_out=0,chop_probe_in=0;bool kit_probe_playing=false,chop_probe_pending=false;CMk3Audio* chop_probe_audio=nullptr;groove::KitRecordCheck kit_check(files,m_Storage,m_Storage,*engine,*project,performance);KitProbePhases kit_phases{&audio};files.set_kit_probe_hook(KitProbePhase,&kit_phases);
#ifdef GROOVE_MULTICORE_DISPLAY
 performance.set_visual_analysis_worker(nullptr);
 auto* display_worker=groove::alloc_cpu_new<CDisplayWorker>();
 bool cores_started=display_worker && display_worker->Initialize();
 bool audio_worker_ready=cores_started && display_worker->ProbeAudio();
 if(audio_worker_ready)m_Logger.Write(FromKernel,LogNotice,"Audio render worker core 3 ready");
 else m_Logger.Write(FromKernel,LogWarning,"Audio render worker unavailable; using USB callback");
 if(cores_started && display_worker->Probe() && display.UseWorker(&display_worker->Slot()))m_Logger.Write(FromKernel,LogNotice,"Display worker core 1 enabled");
 else m_Logger.Write(FromKernel,LogWarning,"Display worker initialization failed");
 if(cores_started && display_worker->ProbeAnalysis()){if(audio_worker_ready)engine->set_va_helper(&display_worker->VaSlot());performance.set_visual_analysis_worker(&display_worker->VisualSlot());files.set_resident_analysis_worker(&display_worker->AnalysisSlot());files.set_project_save_worker(&display_worker->ProjectSaveSlot());files.set_project_load_worker(&display_worker->ProjectLoadSlot());m_Storage.SetSoundFontWorker(&display_worker->SoundFontSlot());m_Logger.Write(FromKernel,LogNotice,"Sample analysis and project encode worker core 2 enabled");m_Logger.Write(FromKernel,display_worker->ProjectLoadSlot().pooled()?LogNotice:LogWarning,"Project decode pooled %u",unsigned(display_worker->ProjectLoadSlot().pooled()));}
 else m_Logger.Write(FromKernel,LogWarning,"Sample analysis worker unavailable; using main core");
#endif
 auto* efficiency_panic_tap=groove::alloc_new<CDiagnosticPanicTap>(m_Serial,m_BootGadget);if(efficiency_panic_tap)m_Logger.SetNewTarget(efficiency_panic_tap);
 CEfficiencyProbe* efficiency_probe=nullptr;
#ifdef GROOVE_MULTICORE_DISPLAY
 groove::CoreWorkMeter fallback_core_work;
 auto* efficiency_work=display_worker?&display_worker->Work():&fallback_core_work;
 if(display_worker)efficiency_probe=groove::alloc_new<CEfficiencyProbe>(*efficiency_work);
#endif
 groove::Mk3InputObservation observed_input;
 uint64_t input_probe_until=0;unsigned input_probe_left=0;uint8_t input_probe_raw[32]{};bool input_probe_seen=false;
 m_Logger.Write(FromKernel,LogNotice,"MK3 prototype: pads play, STEP edits selected track, group selects track; hardware unverified");
 for(;;) {
#ifdef GROOVE_MULTICORE_DISPLAY
  groove::CoreWorkScope efficiency_core0(*efficiency_work,0);
  if(unsigned efficiency_first=updater->TakeEfficiencyRequest()){bool ok=efficiency_probe && audio_worker_ready && !updater->active() && !file_transfer->active() && !(dsp_probe && dsp_probe->busy()) && !preset_check.busy() && !kit_check.busy() && !wave4_original && !native_running && !fx_stress_running && !(final_check && final_check->busy()) && efficiency_probe->start(*engine,*project,performance,files,audio,efficiency_first-1);m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"EFF requested ok %u",unsigned(ok));}
  if(efficiency_probe)efficiency_probe->service(*engine,*project,performance,files,m_Storage,audio,*updater);
  if(efficiency_panic_tap)efficiency_panic_tap->arm(efficiency_probe && efficiency_probe->busy());
#endif
  updater->DiagnosticBusy(midi_probe.active() || wave4_original || files.project_save_busy() || files.kit_load_busy() || chop_probe_pending || (efficiency_probe && efficiency_probe->busy()) || deployment_check.busy() || ui_probe.busy() || display_calibration.busy() || calibrated_ui_pending || optimization_dsp_pending || (dsp_probe && dsp_probe->busy()) || fluid_probe.busy() || preset_check.busy() || kit_check.busy() || (final_check && final_check->busy()) || native_running || audio_stress_preparing || fx_stress_running);
  const bool periodic_diagnostics=trace_requested || deployment_check.busy() || (efficiency_probe && efficiency_probe->busy()) || ui_probe.busy() || display_calibration.busy() || (dsp_probe && dsp_probe->busy()) || preset_check.busy() || kit_check.busy() || fluid_probe.busy() || native_running || audio_stress_preparing || fx_stress_running || (final_check && final_check->busy()) || performance.mode()==groove::UiMode::Diagnostics;
  auto phase=ui_probe.busy()?groove::AudioPhaseTiming::UiProbe:(dsp_probe && dsp_probe->busy())?groove::AudioPhaseTiming::DspProbe:files.project_save_busy()?groove::AudioPhaseTiming::Save:(files.kit_load_busy() || preset_check.busy() || fluid_probe.busy())?groove::AudioPhaseTiming::File:performance.playing()?groove::AudioPhaseTiming::Play:groove::AudioPhaseTiming::Idle;display.TracePhase(unsigned(phase));if(audio)audio->SetTimingPhase(phase);
  unsigned loop_us=CTimer::GetClockTicks(),gap=uint32_t(loop_us-last_loop_us);last_loop_us=loop_us;if(gap>max_loop_us)max_loop_us=gap;
  m_Watchdog.Start(15);
  if(!cpu_monitor_failed && !cpu_throttle.Update()){
   cpu_monitor_failed=true;cpu_throttle.SetSpeed(CPUSpeedLow);
   m_Logger.Write(FromKernel,LogWarning,"CPU temperature control failed; requested low clock");
  }
  // Endpoint repair can fail when the controller itself is wedged. Drain all
  // audio ownership before deleting USB functions; never re-enumerate from IRQ
  // or while an update/transfer/recording owns the device. Two attempts per boot.
  bool software_usb_changed=false;
  auto* faulted_hid=static_cast<CMk3Hid*>(m_DeviceNameService.GetDevice("mk3hid",FALSE));
  if(faulted_hid && faulted_hid->InputRecoveryFailed() && hid_recovery_attempts<2 &&
     hid_reenumeration_retry.ready(CTimer::GetClockTicks64()/1000) &&
     !updater->active() && !file_transfer->active() && !files.recording_busy() &&
     !native_running && !fx_stress_running && !audio_stress_preparing &&
     !deployment_check.busy() && !ui_probe.busy() && !preset_check.busy() && !kit_check.busy() && !fluid_probe.busy() &&
     !display_calibration.busy() && !(dsp_probe && dsp_probe->busy()) &&
     !(efficiency_probe && efficiency_probe->busy()) && !(final_check && final_check->busy())){
   hid_reenumeration_retry.failed(CTimer::GetClockTicks64()/1000);
   ++hid_recovery_attempts;
   if(QuiesceAudio(audio,*engine,performance,true)){
    display.Bind(nullptr);display.Invalidate();auto* device=faulted_hid->GetDevice();
    auto* host=static_cast<CXHCIDevice*>(device->GetHost());
    ++hid_reenumerations;device->RemoveDevice();host->ReScanDevices();software_usb_changed=true;
    observed_input.reset();last_hid=nullptr;last_screen=false;input_resync=last_dropped=last_hid_progress=0;led_output.invalidate();
    performance.notify(groove::UiNotice::ControllerReconnect);
    m_Logger.Write(FromKernel,LogWarning,"HID fault: controller re-enumeration %u/2; playback stopped",hid_reenumerations);
   }else {m_Logger.Write(FromKernel,LogWarning,"HID recovery %u/2 failed: audio drain incomplete; USB resources retained",hid_recovery_attempts);}
  }
  bool usb_changed=m_USBHCI.UpdatePlugAndPlay() || software_usb_changed;
  auto* hid=static_cast<CMk3Hid*>(m_DeviceNameService.GetDevice("mk3hid",FALSE));
  auto* screen=SelectMk3Display(hid,static_cast<CMk3Display*>(m_DeviceNameService.GetDevice("mk3display",FALSE)));
  if(!audio)audio_owner=nullptr;
  if(ReconcileAudioBinding(audio,audio_owner,hid?hid->GetDevice():nullptr))m_Logger.Write(FromKernel,LogWarning,"MK3 controller changed; draining audio before rebinding");
  if(hid!=last_hid || bool(screen)!=last_screen) { if(hid!=last_hid){if(last_hid){if(!updater->active() && !file_transfer->active())performance.release_all();updater->ReleasePads();midi_input->release_all(*engine);midi_resync_pending=midi_transport_resync_pending=true;}last_dropped=last_hid_progress=0;input_resync=0;led_output.invalidate();} performance.mark_dirty(); last_hid=hid; last_screen=screen!=nullptr; }
  if(usb_changed) { observed_input.reset();midi_input->release_all(*engine);midi_resync_pending=true;midi->Scan(hid);hid_retry.reset();midi_retry.reset(); display.Bind(nullptr);display.Invalidate(); led_output.invalidate();for(auto& retry:led_retry)retry.reset();for(auto& retry:display_retry)retry.reset(); }
  bool midi_connected=midi->Matches(hid);if(midi_connected && !last_midi_connected && !midi_output.recovering())midi_transport_resync_pending=true;last_midi_connected=midi_connected;
  if(midi_resync_pending && engine->submit({groove::CommandType::ResendMidiState,0,0,0}))midi_resync_pending=false;
  if(midi_transport_resync_pending && engine->submit({groove::CommandType::ResendMidiTransport,0,0,0}))midi_transport_resync_pending=false;
  auto service_input=[&](){
   uint64_t at=CTimer::GetClockTicks64();if(ui_service_last){unsigned gap=unsigned(at-ui_service_last);if(gap>ui_service_gap)ui_service_gap=gap;if(gap>33334)++ui_service_over_frame;}ui_service_last=at;
  if(hid) {
   unsigned progress=hid->InputProgress();if(progress!=last_hid_progress){last_hid_progress=progress;hid_retry.reset();hid->InputRecovered();}
   auto recover_input=[&](){unsigned dropped=hid->dropped();if(dropped==last_dropped)return;last_dropped=dropped;input_resync=hid->RequestInputResync();if(!updater->active() && !file_transfer->active())performance.release_all();updater->ReleasePads();m_Logger.Write(FromKernel,LogWarning,"HID input lost or malformed total %u; releasing notes and resynchronizing",dropped);};
   recover_input();
   Mk3Pad event;
   while(hid->Pop(event)) {
    recover_input();
    if(input_resync){if(event.type==Mk3EventType::Resync && event.generation==input_resync)input_resync=0;continue;}
    if(event.type==Mk3EventType::Resync)continue;
    if(observed_input.first(event))m_Logger.Write("mk3",LogNotice,"Input event type %u index %u tag %u raw %u (first in category; mapping unverified)",unsigned(event.type),unsigned(event.index),unsigned(event.tag),unsigned(event.pressure));
    if(updater->active()){if(event.type==Mk3EventType::Pad)updater->Pad(event.index,event.tag);else if(event.type==Mk3EventType::ControlByte || event.type==Mk3EventType::ControlBaseline)updater->Control(event.index,event.tag,event.type==Mk3EventType::ControlBaseline);continue;}
    if(event.type==Mk3EventType::ControlBaseline) performance.seed_control(event.index,event.tag);
    else if(event.type==Mk3EventType::ControlByte) {
     // Project-load notifications can briefly report stopped after play() was queued.
     // Abort diagnostics on the physical STOP input, not that asynchronous UI state.
     const auto* stop=groove::mk3::button("stop");
     if((native_running || audio_stress_preparing || (dsp_probe && dsp_probe->busy())) && stop && event.index==stop->report_byte && (event.tag&stop->mask))native_stop_requested=true;
     bool accepted=performance.control(event.index,event.tag);
     if(input_probe_until && input_probe_left)m_Logger.Write(FromKernel,LogNotice,"Input control byte %u value %u accepted %u mode %u track %u step %u page %u",unsigned(event.index),unsigned(event.tag),unsigned(accepted),unsigned(performance.mode()),performance.selected_track(),performance.selected_step(),performance.page());
    }
    else if(event.type==Mk3EventType::Knob) performance.knob(event.index,s16(event.pressure));
    else if(event.type==Mk3EventType::Strip)performance.strip_input(event.pressure,1024);
    else if(event.type==Mk3EventType::MainEncoder){bool accepted=performance.encoder(s16(event.pressure));if(input_probe_until && input_probe_left)m_Logger.Write(FromKernel,LogNotice,"Input encoder delta %d accepted %u mode %u track %u step %u page %u",int(s16(event.pressure)),unsigned(accepted),unsigned(performance.mode()),performance.selected_track(),performance.selected_step(),performance.page());}
    else if(event.type==Mk3EventType::Pad) performance.pad(event.index,event.tag,event.pressure);
   }
   if(hid->InputStopped() && hid_retry.ready(CTimer::GetClockTicks64()/1000)){bool restarted=hid->RestartInput(input_resync);hid_retry.failed(CTimer::GetClockTicks64()/1000);if(restarted)m_Logger.Write(FromKernel,LogNotice,"HID polling restarted; awaiting input resynchronization");else m_Logger.Write(FromKernel,LogWarning,"HID polling restart failed");}
  }
  bool waiting_snapshot=hid && input_resync!=0;if(waiting_snapshot!=performance.diagnostics().input_resync){performance.diagnostics().input_resync=waiting_snapshot;performance.mark_dirty();}
  };
  auto service_ui=[&](bool dispatch=true){
   unsigned now=m_Timer.GetTicks();
   display.Poll();
   if(dispatch){{unsigned begin=display.TaskBegin();performance.animate(CTimer::GetClockTicks64()/1000);performance.poll();display.TaskEnd(2,begin);}service_input();}
  if(!display_calibration.busy() && frame_clock.due(CTimer::GetClockTicks64(),!screen || !draw_count || display.FrameComplete(),performance.navigation_key() | (performance.selected_track()<<24),8000)) {
   u64 draw_audio_before=0,draw_audio_after=0;bool draw_audio_timed=audio && audio->TimingTotal(draw_audio_before);
   unsigned draw_started=CTimer::GetClockTicks();display.BeginDraw(draw_started,frame_clock.due_at());
   performance.animate(CTimer::GetClockTicks64()/1000);renderer->theme=groove::ui::Theme::by_index(performance.visual_settings().theme);
   display.SetPixelOrder(groove::ui::PixelOrder(performance.visual_settings().pixel_order));
   if(updater->active()){renderer->invalidate();updater->Draw(*renderer);updater->Leds(pads,buttons,renderer->theme);led_output.update(pads,buttons);}
   else { const auto selected_track=track_at(*project,project->active_pattern,performance.selected_track());unsigned sample_slot=selected_track.instrument==groove::Instrument::DrumKit?selected_track.kit.pads[performance.selected_kit_pad()].sample[0]:selected_track.sample_slot;groove::WavView selected_sample=sample_slot<groove::kSampleAssets?m_Storage.lookup(project->assets[sample_slot].path):groove::WavView{};renderer->draw(*project,performance,0,0,selected_sample);display.SetPixelOrder(groove::ui::PixelOrder(performance.visual_settings().pixel_order));performance.set_dirty(false); }
   unsigned draw_us=uint32_t(CTimer::GetClockTicks()-draw_started);display.RecordDraw(draw_us);if(draw_us>max_draw_us)max_draw_us=draw_us;
   if(draw_audio_timed && audio->TimingTotal(draw_audio_after) && draw_audio_after>=draw_audio_before){u64 elapsed_audio=draw_audio_after-draw_audio_before;unsigned remaining=elapsed_audio<draw_us?draw_us-unsigned(elapsed_audio):0;if(remaining>max_draw_minus_audio_us)max_draw_minus_audio_us=remaining;++draw_timed_count;}
   ++draw_count;display.NewFrame(frame_clock.release_at());


  }
  {uint64_t us=CTimer::GetClockTicks64();bool moving=performance.playing() || performance.live_recording();if(!periodic_diagnostics || !moving)frame_window=0;else if(!frame_window){frame_window=us;display.ResetFrameStats(us);}else if(!native_running && !ui_probe.busy() && us-frame_window>=10000000){auto f=display.FrameStats();m_Logger.Write(FromKernel,LogNotice,"Display frames window-us %u left %u right %u gap-us %u/%u over100ms %u/%u discarded %u/%u record %u",unsigned(us-frame_window),f.complete[0],f.complete[1],f.max_gap_us[0],f.max_gap_us[1],f.over100ms[0],f.over100ms[1],f.discarded[0],f.discarded[1],unsigned(performance.live_recording()));frame_window=us;display.ResetFrameStats(us);}}
  bool output_failed=false;
  GrooveReapUsbOutput();if(hid)hid->ServiceOutput();
  if(now-last_led>=MSEC2HZ(16)){last_led=now;if(!updater->active()){groove::ui::LedFrame lights;performance.led_frame(lights);led_output.update(lights.pads,lights.buttons,lights.strip);}if(hid){uint64_t ms=CTimer::GetClockTicks64()/1000;bool pad=false;auto result=led_output.flush_one(*hid,led_retry[0].ready(ms),led_retry[1].ready(ms),pad);auto& retry=led_retry[pad?0:1];if(result==groove::ControllerOutputPump::Attempt::Failed){output_failed=true;retry.failed(CTimer::GetClockTicks64()/1000);groove::DiagnosticEvent err;err.kind=groove::DiagnosticKind::TransferError;err.timestamp=CTimer::GetClockTicks64();performance.diagnostics().accept(err);}else if(result==groove::ControllerOutputPump::Attempt::Sent)retry.reset();}}
  unsigned display_side=0;if(!display_calibration.owns_output() && display_pacing.next(CTimer::GetClockTicks(),display.ReadyToPresent()?0:200,screen && !output_failed,display_side) && display_retry[display_side].ready(CTimer::GetClockTicks64()/1000)){
   if(display.present(display_side,renderer->pixels[display_side],480,272))display_retry[display_side].reset();
   else{display_retry[display_side].failed(CTimer::GetClockTicks64()/1000);groove::DiagnosticEvent err;err.kind=groove::DiagnosticKind::TransferError;err.data[0]=4;err.timestamp=CTimer::GetClockTicks64();performance.diagnostics().accept(err);}
  }
  };
  // This callback runs only while recording FatFs waits on the card. It cannot
  // service file actions or update requests; those remain with the outer loop.
  StorageUiWait::bind([](void* p){(*static_cast<decltype(service_ui)*>(p))();},&service_ui);
#ifdef GROOVE_MULTICORE_DISPLAY
  if(cores_started)display_worker->set_snapshot_pump([](void* p){(*static_cast<decltype(service_ui)*>(p))(false);},&service_ui);
#endif
  {unsigned begin=display.TaskBegin();performance.animate(CTimer::GetClockTicks64()/1000);performance.poll();display.TaskEnd(2,begin);}if(!updater->active() && !file_transfer->active()){if(files.project_save_busy() || files.kit_load_busy()){bool saving=files.project_save_busy(),loading=files.kit_load_busy();unsigned started=CTimer::GetClockTicks();StorageUiWait::Scope responsive;unsigned begin=display.TaskBegin();files.service();display.TaskEnd(1,begin);unsigned elapsed=uint32_t(CTimer::GetClockTicks()-started);if(saving && elapsed>save_service_max_us)save_service_max_us=elapsed;if(loading && elapsed>kit_service_max_us)kit_service_max_us=elapsed;}else{unsigned begin=display.TaskBegin();files.service();display.TaskEnd(1,begin);}}display.Bind(screen);
  groove::RecordingResult recording;
  if(files.take_recording_result(recording)){if(final_check)final_check->recorded(recording);if(wave4_original && native_records<20){const char* path=recording.saved && recording.slot<groove::kSampleAssets?project->assets[recording.slot].path:recording.partial;if(path[0])groove::copy_text(native_record_paths[native_records++],96,path);}m_Logger.Write(FromKernel,recording.saved && recording.verified && recording.assigned && !recording.dropped?LogNotice:LogWarning,"Sampler record saved %u verified %u assigned %u frames %u slot %u peak-uv %u/%u input-drops %u failure %u queue-highwater %u",unsigned(recording.saved),unsigned(recording.verified),unsigned(recording.assigned),recording.frames,recording.slot,recording.peak_l_u,recording.peak_r_u,recording.dropped,recording.failure,recording.queue_highwater);}
  PollSerialUpdate(m_Serial,serial_control,*updater,[&](){if(deployment_check.busy() || display_calibration.busy() || ui_probe.busy() || file_transfer->active() || (dsp_probe && dsp_probe->busy()) || files.recording_busy() || kit_check.busy() || preset_check.busy() || wave4_original || (final_check && final_check->busy()))return false;CPrepareUpdate preparation(audio,*engine,performance,m_Storage,*project,*updater);return preparation.run();});
  auto usb_files_open=[&](){bool idle=!updater->diagnostic_busy() && !ui_probe.busy() && !deployment_check.busy() && !(efficiency_probe && efficiency_probe->busy()) && !display_calibration.busy() && !fluid_probe.busy() && !updater->active() && !file_transfer->active() && !files.transfer_busy() && !(dsp_probe && dsp_probe->busy()) && !preset_check.busy() && !kit_check.busy() && !wave4_original && !native_running && !fx_stress_running && !(final_check && final_check->busy());return idle && QuiesceAudio(audio,*engine,performance,false) && file_transfer->Open(false,false,true);};
  file_transfer->UsbPrepare([](void* owner){return (*static_cast<decltype(usb_files_open)*>(owner))();},&usb_files_open);
  updater->PollUsbControl([&](){if(deployment_check.busy() || (efficiency_probe && efficiency_probe->busy()) || display_calibration.busy() || file_transfer->active() || files.recording_busy() || kit_check.busy() || preset_check.busy() || wave4_original || (final_check && final_check->busy()))return false;CPrepareUpdate preparation(audio,*engine,performance,m_Storage,*project,*updater);return preparation.run();},file_transfer->UsbPort());
  updater->PollNetworkControl([&](){if(deployment_check.busy() || (efficiency_probe && efficiency_probe->busy()) || display_calibration.busy() || file_transfer->active() || files.recording_busy() || kit_check.busy() || preset_check.busy() || wave4_original || (final_check && final_check->busy()))return false;CPrepareUpdate preparation(audio,*engine,performance,m_Storage,*project,*updater);return preparation.run();});
  if(updater->TakeDeployCheckRequest()){
   bool ok=audio && audio->operational() && !updater->active() && !file_transfer->active() && !ui_probe.busy() && !display_calibration.busy() && !(dsp_probe && dsp_probe->busy()) && !fluid_probe.busy() && !preset_check.busy() && !kit_check.busy() && !native_running && !fx_stress_running && !(final_check && final_check->busy());
   ok=ok && deployment_check.start(CTimer::GetClockTicks64()/1000,[](void*,const char* name,bool passed){CLogger::Get()->Write("deploy",passed?LogNotice:LogWarning,"Deploy check %s ok %u",name,unsigned(passed));},nullptr);
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Deploy functional requested started %u",unsigned(ok));
  }
  deployment_check.service(CTimer::GetClockTicks64()/1000);
  {unsigned passed=0;bool ok=false;if(deployment_check.result(passed,ok))m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Deploy functional completed passed %u ok %u restored 1",passed,unsigned(ok));}
  if(updater->TakeFinalTestRequest()){bool ok=final_check && !updater->active() && !file_transfer->active() && !native_running && !fx_stress_running && !preset_check.busy() && final_check->start(CTimer::GetClockTicks64()/1000);m_Logger.Write(FromKernel,LogNotice,"Final device requested ok %u",unsigned(ok));}
  if(final_check)final_check->service(CTimer::GetClockTicks64()/1000);
  if(updater->TakePresetCheckRequest()){bool ok=!(dsp_probe && dsp_probe->busy()) && !updater->active() && !file_transfer->active() && !native_running && !fx_stress_running;
   if(ok){const auto library=groove::audit_factory_library(files,m_Storage,performance);ok=library.complete && library.presets+library.kits==groove::kFactoryLibraryCount && library.representative && !library.old_name && !library.old_formats;m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Factory library Browser presets %u kits %u total %u expected %u old-formats %u old-name %u representative %u ok %u",library.presets,library.kits,library.presets+library.kits,groove::kFactoryLibraryCount,library.old_formats,unsigned(library.old_name),unsigned(library.representative),unsigned(ok));}
   ok=ok && preset_check.start(groove::kFactoryRepresentative);m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Preset functional check started %u heap-low %u",unsigned(ok),unsigned(CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW)));}
  if(preset_check.busy()){unsigned free=unsigned(CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW));if(free<preset_heap_min)preset_heap_min=free;}
  if(preset_check.progress()!=preset_check_progress){preset_check_progress=preset_check.progress();m_Logger.Write(FromKernel,LogNotice,"Preset check engine %u stage %u heap-low %u min %u pattern-bytes %u",preset_check_progress/16,preset_check_progress%16,unsigned(CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW)),preset_heap_min,unsigned(sizeof(groove::PatternState)));}
  {unsigned begin=display.TaskBegin();preset_check.service();display.TaskEnd(3,begin);}{unsigned passed=0;bool ok=false;if(preset_check.result(passed,ok)){m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Factory representative TX81z Lately Bass selected mounted %u",unsigned(preset_check.representative_mounted()));display.EndTrace();if(audio)audio->SealAttribution();m_Logger.Write(FromKernel,LogNotice,"Diagnostic idle audio %u lcd %u",unsigned(!audio || audio->DiagnosticIdle()),unsigned(!display.TraceCapturing()));m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Preset functional check saved mounted %u/%u restored cleaned ok %u heap-low %u",passed,groove::PresetFunctionalCheck::kCases,unsigned(ok),unsigned(CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW)));}}
  if(updater->TakeInputTestRequest()){input_probe_until=CTimer::GetClockTicks64()/1000+120000;input_probe_left=128;input_probe_seen=false;m_Logger.Write(FromKernel,LogNotice,"Input probe armed: 120 seconds / 128 changed reports; no transport changes");}
  if(input_probe_until && CTimer::GetClockTicks64()/1000>=input_probe_until){input_probe_until=0;m_Logger.Write(FromKernel,LogNotice,"Input probe finished");}
  if(updater->TakeBenchmarkRequest()){
   bool ok=!files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running;
   unsigned total_started=CTimer::GetClockTicks(),hash_us=0,generate_us=0,save_us=0,load_us=0;bool regenerated=false;
   m_Logger.Write(FromKernel,LogNotice,"Transition BENCH begin heap-low %u",unsigned(CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW)));
   auto* candidate=groove::alloc_cpu_new<groove::ProjectState>();
   if(!candidate)ok=false;
   if(ok){unsigned started=CTimer::GetClockTicks();m_Logger.Write(FromKernel,LogNotice,"Transition BENCH hash begin");const bool current=groove::benchmark_file_matches(m_Storage,"BENCH.MGXS");hash_us=uint32_t(CTimer::GetClockTicks()-started);if(!current){regenerated=true;started=CTimer::GetClockTicks();groove::make_benchmark_project(*candidate);generate_us=uint32_t(CTimer::GetClockTicks()-started);started=CTimer::GetClockTicks();ok=groove::save_recoverable(m_Storage,"BENCH.MGXS",*candidate);save_us=uint32_t(CTimer::GetClockTicks()-started);}}
   delete candidate;
   if(ok){m_Logger.Write(FromKernel,LogNotice,"Transition BENCH load begin");unsigned started=CTimer::GetClockTicks();ok=files.load("BENCH.MGXS");load_us=uint32_t(CTimer::GetClockTicks()-started);}
   m_Logger.Write(FromKernel,LogNotice,"Benchmark timing us total %u hash %u generate %u save %u load %u regenerated %u",uint32_t(CTimer::GetClockTicks()-total_started),hash_us,generate_us,save_us,load_us,unsigned(regenerated));
   if(ok){m_Logger.Write(FromKernel,LogNotice,"Transition BENCH cleanup begin");unsigned files_removed=0,dirs_removed=0;ok=m_Storage.CleanBenchmarkData(files_removed,dirs_removed);m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Benchmark DATA cleanup files %u dirs %u ok %u",files_removed,dirs_removed,unsigned(ok));}
   if(ok)ok=performance.play();
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,ok?"Benchmark loaded and playback requested":"Benchmark failed; inspect saved session and sample asset");
  }
  unsigned stress_mode=updater->TakePlaitsStressRequest();
  if(stress_mode){
   bool ok=!files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running && (stress_mode!=7 || (audio && audio->operational()));
   if(stress_mode==7){ok=ok && !wave4_original && !audio_stress_preparing;if(ok){wave4_original=groove::alloc_cpu_new<groove::ProjectState>(*project);ok=wave4_original!=nullptr;if(ok){files.copy_bindings(wave4_bindings);native_records=0;}}}
   auto* candidate=groove::alloc_cpu_new<groove::ProjectState>();groove::WavView prepared[groove::kSampleAssets]{};
   if(!candidate)ok=false;
   if(ok){if(stress_mode==4)groove::make_daisy_drum_demo_project(*candidate);else if(stress_mode==7)groove::make_fx_stress_project(*candidate);else groove::make_plaits_stress_project(*candidate,stress_mode==3?300:120);}
   if(ok)ok=m_Storage.prepare(*candidate,prepared);
   if(ok)ok=engine->stage_project(*candidate,prepared);
   if(ok){*project=*candidate;files.set_bindings(prepared);performance.project_loaded();ok=performance.play();}
   delete candidate;
   if(stress_mode==7 && !ok && wave4_original)wave4_restore=true;
   m_Logger.Write(FromKernel,LogNotice,"Stress preserves DATA library");
   if(stress_mode==7){if(ok){fx_stress_running=true;fx_stress_last_throw=255;fx_stress_start=fx_stress_next_morph=CTimer::GetClockTicks64()/1000;fx_stress_audio=audio;fx_start_underrun=GrooveXHCIIsoEvents(0,2);fx_start_overrun=GrooveXHCIIsoEvents(1,3);audio->BeginTimingWindow();engine->begin_stage_profile(nullptr);fx_profile_started=false;performance.set_mode(groove::UiMode::Effects);}m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,ok?"FX stress loaded and playback requested":"FX stress failed; inspect state");continue;}
   if(ok){if(stress_mode==4)m_Logger.Write(FromKernel,LogNotice,"Daisy drum demo playing six published voices");else m_Logger.Write(FromKernel,LogNotice,"Plaits stress tempo %u",stress_mode==3?300u:120u);}
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,stress_mode==4?(ok?"Daisy drum demo loaded and playback requested":"Daisy drum demo failed; inspect state"):(ok?"Plaits stress loaded and playback requested":"Plaits stress failed; inspect state"));
  }
  if(fx_stress_running){uint64_t ms=CTimer::GetClockTicks64()/1000;bool stopped=!performance.playing(),complete=ms-fx_stress_start>=30000;
   if(stopped || complete){auto stage=engine->end_stage_profile();if(complete && !stopped)performance.stop();fx_stress_running=false;if(wave4_original)wave4_restore=true;m_Logger.Write(FromKernel,LogNotice,"FX stages sampled %u avg-us control %u events %u tracks %u output %u",stage.calls,stage.calls?unsigned(stage.control/stage.calls):0,stage.calls?unsigned(stage.events/stage.calls):0,stage.calls?unsigned(stage.tracks/stage.calls):0,stage.calls?unsigned(stage.output/stage.calls):0);m_Logger.Write(FromKernel,LogNotice,"FX stage max-us control %u events %u tracks %u output %u",stage.max_control,stage.max_events,stage.max_tracks,stage.max_output);m_Logger.Write(FromKernel,LogNotice,"FX stage peak-us total %u control %u events %u tracks %u output %u",stage.max_total,stage.peak_control,stage.peak_events,stage.peak_tracks,stage.peak_output);if(audio && audio==fx_stress_audio){auto worker=audio->EndWorkerWindow();m_Logger.Write(FromKernel,LogNotice,"FX worker blocks %u avg-us %u max-us %u over500 %u missing-frames %u",worker.blocks,worker.blocks?unsigned(worker.total_us/worker.blocks):0,worker.max_us,worker.over500,audio->worker_missing()-fx_start_missing);m_Logger.Write(FromKernel,LogNotice,"FX worker cumulative-overrun-max-us %u",worker.debt_max_us);auto timing=audio->EndTimingWindow();m_Logger.Write(FromKernel,LogNotice,"FX stress timing callbacks %u avg-us %u max-us %u over125 %u over250 %u bad %u usb-underrun %u usb-overrun %u",timing.callbacks,timing.callbacks?unsigned(timing.total_us/timing.callbacks):0,timing.max_us,timing.over_125us,timing.over_250us,timing.bad_chunks,GrooveXHCIIsoEvents(0,2)-fx_start_underrun,GrooveXHCIIsoEvents(1,3)-fx_start_overrun);}else m_Logger.Write(FromKernel,LogWarning,"FX stress audio device changed; timing unavailable");m_Logger.Write(FromKernel,LogNotice,complete?"FX stress 30 seconds completed":"FX stress stopped by transport");}
   else {unsigned segment=unsigned((ms-fx_stress_start)/5000);const unsigned modes[]={0,unsigned(groove::PerformanceFx::DelayThrow),unsigned(groove::PerformanceFx::PitchDive),unsigned(groove::PerformanceFx::ReverbThrow),unsigned(groove::PerformanceFx::PitchRise),unsigned(groove::PerformanceFx::ReverbFreeze)};unsigned desired=modes[segment<6?segment:5];if(desired!=fx_stress_last_throw){groove::Command effect{groove::CommandType::SetPerformanceFx,0,0,uint16_t(desired)};effect.scalar=0.7f;if(engine->submit(effect)){fx_stress_last_throw=desired;m_Logger.Write(FromKernel,LogNotice,"FX stress performance mode %u",desired);}}if(!fx_profile_started && ms-fx_stress_start>=2000){engine->begin_stage_profile(AudioProfileClock);audio->BeginWorkerWindow();fx_start_missing=audio->worker_missing();fx_profile_started=true;}if(ms>=fx_stress_next_morph){unsigned phase=unsigned(((ms-fx_stress_start)/250)%16);float position=float(phase<=8?phase:16-phase)/8.0f;groove::Command morph{groove::CommandType::SetMorph,0,0,0};morph.scalar=position;if(engine->submit(morph)){project->morph_position=position;performance.mark_dirty();}groove::Command macro{groove::CommandType::SetMacroValue,0,1,1};macro.scalar=position;if(engine->submit(macro))project->macros[1].value=position;fx_stress_next_morph=ms+250;}}}

  if(unsigned request=updater->TakeDspRequest()){
   optimization_fluid_pending=false;
   bool ok=audio && audio->operational() && !files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running && !(dsp_probe && dsp_probe->busy()) && !audio_stress_preparing && !wave4_original && !preset_check.busy();
   if(ok && !dsp_probe)dsp_probe=groove::alloc_new<CDspProbe>();
   if(ok){native_stop_requested=false;ok=dsp_probe && dsp_probe->start(*engine,*project,performance,files,audio,request==21);}
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"DSP request kind %u accepted %u",request,unsigned(ok));
  }
  if(optimization_dsp_pending && !ui_probe.busy()){
   optimization_dsp_pending=false;native_stop_requested=false;if(!dsp_probe)dsp_probe=groove::alloc_new<CDspProbe>();
   bool ok=dsp_probe && dsp_probe->start(*engine,*project,performance,files,audio,false,true);
   optimization_fluid_pending=ok;m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Optimization DSP four fixed conditions started %u",unsigned(ok));
  }
  if(dsp_probe && dsp_probe->busy()){unsigned begin=display.TaskBegin();dsp_probe->service(*engine,*project,performance,files,m_Storage,audio,native_stop_requested);display.TaskEnd(5,begin);if(!dsp_probe->busy() && audio){display.EndTrace();audio->SealAttribution();audio->TracePhases();if(optimization_fluid_pending){optimization_fluid_pending=false;fluid_probe.start(m_Storage,performance,display,audio);}}}
  if(!ui_probe.busy() && !optimization_dsp_pending && !(dsp_probe && dsp_probe->busy()) && !fluid_probe.busy()){display.DrainTrace();if(audio)audio->DrainAttribution();}
  if(fluid_probe.busy())fluid_probe.service(m_Storage,performance,display,audio);
  if(updater->TakeWave4TestRequest()){
   bool ok=audio && audio->operational() && !files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running && !(dsp_probe && dsp_probe->busy()) && !audio_stress_preparing && !wave4_original && !preset_check.busy();
   if(ok){m_Logger.Write(FromKernel,LogNotice,"Wave4 context input-db %d screen %u LED %u motion %u local %u",int(performance.device_settings().input_db),unsigned(performance.device_settings().screen),unsigned(performance.device_settings().led),unsigned(performance.visual_settings().motion),unsigned(performance.device_settings().local));wave4_original=groove::alloc_cpu_new<groove::ProjectState>(*project);ok=wave4_original!=nullptr;}
   if(ok){files.copy_bindings(wave4_bindings);performance.panic();if(!audio_stress)audio_stress=groove::alloc_new<groove::AudioStressAssets>(m_Storage);ok=audio_stress && audio_stress->start();audio_stress_preparing=ok;native_stop_requested=false;wave4=ok;wave4_restore=!ok;}
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Wave4 draft revision 2 requested ok %u; no qualification thresholds",unsigned(ok));
  }
  if(wave4_restore && wave4_original && !files.recording_busy()){
   performance.dispatch({groove::ControllerInput::Type::KnobTouch,0,0});
   if(engine->stage_project(*wave4_original,wave4_bindings)){*project=*wave4_original;files.set_bindings(wave4_bindings);performance.project_loaded();wave4_restored_generation=engine->staged_generation();delete wave4_original;wave4_original=nullptr;wave4_restore=false;wave4=false;}
  }
  if(wave4_restored_generation && performance.visual().generation==wave4_restored_generation){
   bool cleaned=!audio_stress || audio_stress->cleanup();for(unsigned i=0;i<native_records;++i)cleaned=m_Storage.remove(native_record_paths[i]) && cleaned;native_records=0;
   m_Logger.Write(FromKernel,LogNotice,"Workload draft restored stopped cleaned %u",unsigned(cleaned));wave4_restored_generation=0;
  }
  if(updater->TakeAudioTestRequest()){
   bool ok=audio && audio->operational() && !files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running && !(dsp_probe && dsp_probe->busy()) && !audio_stress_preparing && !wave4_original;
   if(ok){wave4_original=groove::alloc_cpu_new<groove::ProjectState>(*project);ok=wave4_original!=nullptr;if(ok)files.copy_bindings(wave4_bindings);}
   if(ok){wave4=false;performance.panic();if(!audio_stress)audio_stress=groove::alloc_new<groove::AudioStressAssets>(m_Storage);ok=audio_stress && audio_stress->start();audio_stress_preparing=ok;native_stop_requested=false;wave4_restore=!ok;native_records=0;}
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Audio workload v1 requested ok %u; preparing four 48s streams",unsigned(ok));
  }
  if(audio_stress_preparing){if(audio_stress->service(m_Storage)){audio_stress_preparing=false;native_running=true;native_mixed=!wave4;native_case=0;native_start=0;native_audio=audio;}else if(audio_stress->failed()){audio_stress_preparing=false;if(wave4_original)wave4_restore=true;m_Logger.Write(FromKernel,LogWarning,"Audio workload asset preparation failed");}}
  if(updater->TakeNativeTestRequest()){
   bool ok=audio && audio->operational() && !files.recording_busy() && !updater->active() && !file_transfer->active() && !fx_stress_running && !native_running && !wave4_original;
   if(ok){wave4_original=groove::alloc_cpu_new<groove::ProjectState>(*project);ok=wave4_original!=nullptr;if(ok)files.copy_bindings(wave4_bindings);}
   if(ok){wave4=false;native_records=0;native_stop_requested=false;native_mixed=false;native_running=true;native_case=0;native_start=0;native_audio=audio;}
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Native workload v1 requested ok %u; DATA unchanged",unsigned(ok));
  }
  if(native_running){
   uint64_t ms=CTimer::GetClockTicks64()/1000;
   if(audio!=native_audio || !audio || !audio->operational() || updater->active() || native_stop_requested){
    engine->end_stage_profile();engine->submit({groove::CommandType::Panic,0,0,0});native_running=false;if(wave4_original){performance.stop();wave4_restore=true;}m_Logger.Write(FromKernel,LogWarning,"Native test aborted: STOP or audio/update changed");
   }else if(!native_start && !files.recording_busy()){
    if(native_case==(wave4?8u:19u)){native_running=false;wave4_restore=wave4_original!=nullptr;performance.stop();if(wave4){wave4_restore=true;m_Logger.Write(FromKernel,LogNotice,"Wave4 draft completed scenarios; restoring");}else m_Logger.Write(FromKernel,LogNotice,native_mixed?"Audio workload v1 completed 19 cases":"Native workload v1 completed 19 cases");}else{
    auto* candidate=groove::alloc_cpu_new<groove::ProjectState>();bool ok=candidate!=nullptr;
    if(ok){if(wave4){groove::make_wave4_workload(*candidate,native_case<5?native_case:0,audio_stress);if(native_case==7){candidate->song.enabled=true;candidate->song.length=1;candidate->song.sections[0].bars=4;}if(native_case==3){const char* path="GROOVE/LOADTEST.SF2";unsigned bank=0,program=0;uint32_t crc=0;bool available=m_Storage.size(path)>=32u*1024u*1024u && m_Storage.load(path,audio_stress->views[4]) && audio_stress->views[4].soundfont && groove::soundfont_first(audio_stress->views[4].soundfont,bank,program) && groove::file_checksum(m_Storage,path,crc);if(!available){m_Logger.Write(FromKernel,LogNotice,"Wave4 case 3 SKIPPED: valid >=32MiB GROOVE/LOADTEST.SF2 required");delete candidate;++native_case;continue;}groove::copy_text(candidate->assets[4].path,96,path);candidate->assets[4].metadata.content_crc=crc;for(auto& t:candidate->tracks){t.soundfont.values[0]=float(bank);t.soundfont.values[1]=float(program);}m_Logger.Write(FromKernel,LogNotice,"Wave4 SF2 bytes %u crc %08x bank %u program %u",unsigned(m_Storage.size(path)),crc,bank,program);}}else if(native_mixed)audio_stress->project(*candidate,native_case);else groove::make_native_stress_project(*candidate,native_case);ok=engine->stage_project(*candidate,(native_mixed || wave4)?audio_stress->views:nullptr);}
    if(ok){*project=*candidate;performance.project_loaded();if(native_mixed || wave4)files.set_bindings(audio_stress->views);ok=performance.play();if(ok && (native_mixed || (wave4 && native_case==4))){performance.select_track(12);ok=performance.request_file(groove::FileAction::AudioRecord);}}
    delete candidate;
    if(!ok){native_running=false;if(wave4_original)wave4_restore=true;m_Logger.Write(FromKernel,LogWarning,"Native case %u load failed",native_case);}
    else{native_start=CTimer::GetClockTicks64()/1000;native_profile_start=0;native_tone=0;native_profile=false;wave4_save=false;if(wave4)m_Logger.Write(FromKernel,LogNotice,"Wave4 case %u start revision 2 / 2s warmup + 10s window",native_case);performance.set_mode(groove::UiMode::Synth);
     if(wave4){performance.dispatch({groove::ControllerInput::Type::KnobTouch,0,0});if(native_case==5 || native_case==6){performance.select_track(0);performance.set_mode(groove::UiMode::Step);performance.set_shift(true);for(unsigned i=0;i<3 && !performance.tracker_view();++i)performance.soft_key(3);performance.set_shift(false);if(native_case==6)performance.dispatch({groove::ControllerInput::Type::KnobTouch,0,1});}else if(native_case==7)performance.set_mode(groove::UiMode::Song);}
     m_Logger.Write(FromKernel,LogNotice,native_mixed?"Audio case %u start: 12 native + 2 loop + 2 AUDIO + 1 record, 48k, version 1":"Native case %u start: 16 tracks, 48k, version 1",native_case);}
    }
   }else if(native_start){
    if(ms-native_tone>=500){if(!wave4 && native_case>=3)engine->submit({groove::CommandType::DiagnosticTone,0,0,2});native_tone=ms;}
    if(!native_profile && ms-native_start>=2000){audio->BeginTimingWindow();audio->BeginWorkerWindow();native_stream_misses=(native_mixed || wave4)?audio_stress->misses():0;native_missing=audio->worker_missing();engine->begin_stage_profile(AudioProfileClock,63);native_profile=true;native_profile_start=ms;frame_window=CTimer::GetClockTicks64();display.ResetFrameStats(frame_window);ui_service_last=frame_window;ui_service_gap=ui_service_over_frame=0;native_underrun=GrooveXHCIIsoEvents(0,2);native_overrun=GrooveXHCIIsoEvents(1,3);}
    if(wave4 && native_case==6 && ms-native_start>=7000 && performance.knob_touched(0)){performance.dispatch({groove::ControllerInput::Type::KnobTouch,0,0});m_Logger.Write(FromKernel,LogNotice,"Wave4 tracker overlay released; expiry follows monotonic clock");}
    if(wave4 && native_case==4 && native_profile && !wave4_save && ms-native_start>=3500){wave4_save=true;unsigned started=CTimer::GetClockTicks();bool ok=FreshProbeSong(m_Storage,save_probe_path) && files.start_project_save(save_probe_path,true);save_snapshot_us=uint32_t(CTimer::GetClockTicks()-started);save_service_max_us=0;save_probe_started_ms=ms;m_Logger.Write(FromKernel,LogNotice,"Wave4 concurrent save requested %u snapshot-us %u",unsigned(ok),save_snapshot_us);}
    if(native_profile && ms-native_profile_start>=10000){
     {auto f=display.FrameStats();m_Logger.Write(FromKernel,LogNotice,"Native case %u UI window-us %u frames %u/%u gap-us %u/%u over100ms %u/%u discarded %u/%u input-service-gap-us %u over-frame %u",native_case,unsigned(CTimer::GetClockTicks64()-frame_window),f.complete[0],f.complete[1],f.max_gap_us[0],f.max_gap_us[1],f.over100ms[0],f.over100ms[1],f.discarded[0],f.discarded[1],ui_service_gap,ui_service_over_frame);}
     m_Logger.Write(FromKernel,LogNotice,"Native admission active-mask %u refused-mask %u average-budget-us %u burst-budget-us %u",engine->native_active_mask(),engine->native_refused_mask(),groove::NativeBudget::average_limit,groove::NativeBudget::burst_limit);
     m_Logger.Write(FromKernel,LogNotice,"Native case %u measured-ms %u timing-revision 3",native_case,unsigned(ms-native_profile_start));
     auto stage=engine->end_stage_profile();auto timing=audio->EndTimingWindow();auto worker=audio->EndWorkerWindow();
     m_Logger.Write(FromKernel,LogNotice,"Native case %u worker blocks %u avg-us %u max-us %u over500 %u missing-frames %u",native_case,worker.blocks,worker.blocks?unsigned(worker.total_us/worker.blocks):0,worker.max_us,worker.over500,audio->worker_missing()-native_missing);
     m_Logger.Write(FromKernel,LogNotice,"Native case %u callbacks %u avg-us %u max-us %u over125 %u over250 %u bad %u underrun %u overrun %u",native_case,timing.callbacks,timing.callbacks?unsigned(timing.total_us/timing.callbacks):0,timing.max_us,timing.over_125us,timing.over_250us,timing.bad_chunks,GrooveXHCIIsoEvents(0,2)-native_underrun,GrooveXHCIIsoEvents(1,3)-native_overrun);
     m_Logger.Write(FromKernel,LogNotice,"Native case %u stages calls %u avg-tracks %u peak-total %u control %u events %u tracks %u output %u",native_case,stage.calls,stage.calls?unsigned(stage.tracks/stage.calls):0,stage.max_total,stage.peak_control,stage.peak_events,stage.peak_tracks,stage.peak_output);
     if(native_mixed || (wave4 && native_case==4)){m_Logger.Write(FromKernel,LogNotice,"Audio case %u stream-misses %u recording-failed %u",native_case,audio_stress->misses()-native_stream_misses,unsigned(engine->live_record_failed()));if(performance.live_recording())performance.request_file(groove::FileAction::RecordToggle);}
     if(native_mixed || wave4)m_Storage.TraceStreams();
     performance.stop();
     engine->submit({groove::CommandType::Panic,0,0,0});native_start=0;
     ++native_case;
    }
   }
  }

  if(updater->TakeLoopbackRequest()){
   bool ok=audio && !files.recording_busy() && !updater->active() && !file_transfer->active();
   if(ok)audio->ResetProbeStats();
   if(ok)ok=performance.stop();
   if(ok)ok=files.start_loopback_probe();
   if(ok)ok=engine->submit({groove::CommandType::DiagnosticTone,0,0,2});
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Loopback one-second input WAV requested ok %u",unsigned(ok));
  }
  if(updater->TakeKitSwapRequest()){
   kit_probe_playing=performance.playing();kit_probe_track=performance.selected_track();kit_probe_missing=audio?audio->worker_missing():0;kit_probe_out=GrooveXHCIIsoEvents(0,2);kit_probe_in=GrooveXHCIIsoEvents(1,3);kit_probe_started_us=CTimer::GetClockTicks();kit_service_max_us=0;kit_phases.last_us=kit_probe_started_us;kit_phases.last_missing=kit_probe_missing;bool ok=!updater->active() && !file_transfer->active() && !files.recording_busy() && files.start_kit_load("FACTORY/KITS/Techno-01.mgxk",true);
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Kit swap probe queued %u",unsigned(ok));
  }
  {bool ok=false,probe=false;if(files.take_kit_load_result(ok,probe) && probe){if(!ok)m_Logger.Write(FromKernel,LogWarning,"Kit swap failure: %s",files.kit_load_error());m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Kit swap probe ok %u track %u playing %u/%u elapsed-us %u usb-underrun %u usb-overrun %u",unsigned(ok),kit_probe_track,unsigned(kit_probe_playing),unsigned(performance.playing()),uint32_t(CTimer::GetClockTicks()-kit_probe_started_us),GrooveXHCIIsoEvents(0,2)-kit_probe_out,GrooveXHCIIsoEvents(1,3)-kit_probe_in);m_Logger.Write(FromKernel,LogNotice,"Kit swap worker-missing delta %u service-max-us %u",audio?audio->worker_missing()-kit_probe_missing:0,kit_service_max_us);}}
  if(updater->TakeChopTestRequest()){
   bool ready=audio && audio->operational() && performance.playing() && !chop_probe_pending && !updater->active() && !file_transfer->active() && !files.recording_busy() && !files.kit_load_busy();
   chop_probe_audio=audio;chop_probe_missing=audio?audio->worker_missing():0;chop_probe_out=GrooveXHCIIsoEvents(0,2);chop_probe_in=GrooveXHCIIsoEvents(1,3);
   unsigned started=CTimer::GetClockTicks();bool mapped=ready && performance.map_kit_slices();unsigned map_us=uint32_t(CTimer::GetClockTicks()-started);started=CTimer::GetClockTicks();bool chopped=mapped && performance.chop_to_steps();unsigned chop_us=uint32_t(CTimer::GetClockTicks()-started);
   m_Logger.Write(FromKernel,mapped && chopped?LogNotice:LogWarning,"Chop probe map %u phrase %u playing %u map-us %u phrase-us %u worker-missing %u",unsigned(mapped),unsigned(chopped),unsigned(performance.playing()),map_us,chop_us,audio?audio->worker_missing()-chop_probe_missing:0);
   if(mapped && chopped){chop_probe_pending=true;chop_probe_end_ms=CTimer::GetClockTicks64()/1000+2000;}
  }
  if(chop_probe_pending && CTimer::GetClockTicks64()/1000>=chop_probe_end_ms){chop_probe_pending=false;m_Logger.Write(FromKernel,LogNotice,"Chop probe after 2s playing %u audio-valid %u worker-missing %u usb-underrun %u usb-overrun %u",unsigned(performance.playing()),unsigned(audio && audio==chop_probe_audio && audio->operational()),audio && audio==chop_probe_audio?audio->worker_missing()-chop_probe_missing:0,GrooveXHCIIsoEvents(0,2)-chop_probe_out,GrooveXHCIIsoEvents(1,3)-chop_probe_in);}
  if(updater->TakeKitRecordTestRequest()){bool ok=!updater->active() && !file_transfer->active() && !native_running && !fx_stress_running && !preset_check.busy() && !(final_check && final_check->busy()) && audio && audio->operational() && kit_check.start(CTimer::GetClockTicks64()/1000);m_Logger.Write(FromKernel,LogNotice,"Kit record draft requested %u",unsigned(ok));}
  kit_check.service(CTimer::GetClockTicks64()/1000);
  if(updater->TakeMidiTestRequest()){
   bool ok=!midi_probe.active() && midi_connected && !midi->output_blocked() && !updater->active() && !file_transfer->active() && !files.recording_busy() && !files.project_save_busy() && !ui_probe.busy();
   if(ok)ok=performance.stop();
   if(ok){midi_input->release_all(*engine);ok=midi_probe.start(CTimer::GetClockTicks64(),midi->Dropped());midi_probe_reported=false;midi_probe_missing=audio?audio->worker_missing():0;midi_probe_out=GrooveXHCIIsoEvents(0,2);midi_probe_in=GrooveXHCIIsoEvents(1,3);}
   m_Logger.Write(FromKernel,LogNotice,"DIN MIDI start ok %u connected %u blocked %u count %u tx-cable 0",unsigned(ok),unsigned(midi_connected),unsigned(midi->output_blocked()),groove::MidiLoopbackProbe::kMessages);
  }
  unsigned ui_request=updater->TakeOptimizationRequest()?2:updater->TakeUiTestRequest()?1:0;
  if(ui_request){
   bool ok=!fluid_probe.busy() && !ui_probe.busy() && !updater->active() && !file_transfer->active() && !files.project_save_busy() && !performance.modal_active() && !native_running && !fx_stress_running && !(dsp_probe && dsp_probe->busy());
   if(ok){optimization_suite=ui_request==2;ok=display_calibration.start();calibrated_ui_pending=ok;}
   m_Logger.Write(FromKernel,LogNotice,"UI probe logical inputs started %u full-suite %u",unsigned(ok),unsigned(optimization_suite));
  }
  display_calibration.service(display,screen);
  if(calibrated_ui_pending && display_calibration.ready()){calibrated_ui_pending=false;unsigned begin=display.TaskBegin();bool ok=display_calibration.result() && ui_probe.start(*engine,*project,performance,files,m_Storage,audio);display.TaskEnd(6,begin);m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"UI fixed qualification after calibration started %u",unsigned(ok));}
  {bool active=ui_probe.busy();unsigned begin=display.TaskBegin();ui_probe.service(*engine,*project,performance,files,display,screen!=nullptr,audio);display.TaskEnd(4,begin);if(active && !ui_probe.busy() && audio){audio->TracePhases();optimization_dsp_pending=optimization_suite && ui_probe.completed();if(!optimization_dsp_pending)m_Logger.Write(FromKernel,LogNotice,"UI-only diagnostic completed idle 1; no DSP or Fluid chaining");}}
  if(updater->TakeSaveTestRequest()){
   if(audio)audio->SetTimingPhase(groove::AudioPhaseTiming::Save);
   bool ok=!updater->active() && !file_transfer->active() && files.project_save_available() && !files.project_save_busy();
   if(ok){unsigned started=CTimer::GetClockTicks();ok=FreshProbeSong(m_Storage,save_probe_path) && files.start_project_save(save_probe_path,true);save_snapshot_us=uint32_t(CTimer::GetClockTicks()-started);save_service_max_us=0;}
   if(ok)save_probe_started_ms=CTimer::GetClockTicks64()/1000;
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Project save probe started ok %u",unsigned(ok));
  }
  if(updater->TakeSampleWorkflowRequest()){
   bool ok=audio && !files.recording_busy() && !updater->active() && !file_transfer->active();
   if(ok)ok=performance.stop();
   if(ok)ok=files.start_sample_workflow_probe();
   if(ok)ok=engine->submit({groove::CommandType::DiagnosticTone,0,0,2});
   m_Logger.Write(FromKernel,ok?LogNotice:LogWarning,"Sampler one-second input-to-track requested ok %u",unsigned(ok));
  }
  groove::LoopbackRecordResult loopback;
  if(files.take_loopback_probe_result(loopback)){m_Logger.Write(FromKernel,loopback.verified && loopback.cleaned?LogNotice:LogWarning,"Loopback WAV saved %u verified %u cleaned %u frames %u rms-uv %u/%u peak-uv %u/%u tone-uv %u/%u input-drops %u",unsigned(loopback.saved),unsigned(loopback.verified),unsigned(loopback.cleaned),loopback.frames,loopback.rms_l_u,loopback.rms_r_u,loopback.peak_l_u,loopback.peak_r_u,loopback.tone_l_u,loopback.tone_r_u,loopback.dropped);if(audio){audio->EndProbeStats();auto probe=audio->GetProbeStats();m_Logger.Write(FromKernel,LogNotice,"Loopback USB peaks main %u phones %u input-right %u/%u uFS",probe.main_peak_u,probe.phones_peak_u,probe.input_right_peak_l_u,probe.input_right_peak_r_u);}}
  groove::ClockEvent clock_event; while(engine->pop_clock(clock_event)) {}
  if(!midi_probe.owns_output() && midi_retry.ready(CTimer::GetClockTicks64()/1000)){unsigned failures=midi_output.flush_realtime(*engine,[&](unsigned cable,const uint8_t* bytes,unsigned length){bool ok=midi_connected && midi->Send(cable,bytes,length);groove::DiagnosticEvent diag;diag.timestamp=CTimer::GetClockTicks64();diag.kind=ok?groove::DiagnosticKind::MidiOut:groove::DiagnosticKind::TransferError;performance.diagnostics().accept(diag);return ok;});midi_output_failures+=failures;if(failures)midi_retry.failed(CTimer::GetClockTicks64()/1000);else midi_retry.reset();bool fence_drained=midi_output.take_resync_request();if(midi_output.take_recovered() || fence_drained){midi_resync_pending=true;midi_transport_resync_pending=true;}}
  auto& midi_diag=performance.diagnostics();auto midi_status=groove::midi_diagnostic_status(hid!=nullptr,midi->Connected(),midi_connected,midi->output_blocked(),midi->binding_exhausted());unsigned midi_lost=midi->Dropped();if(midi_diag.midi_status!=midi_status || midi_diag.midi_dropped!=midi_lost){midi_diag.midi_status=midi_status;midi_diag.midi_dropped=midi_lost;performance.mark_dirty();}
  midi_input->service(*engine);
  auto midi_loss=[&](){unsigned dropped=midi->Dropped();if(!midi_input->recover_dropped(*engine,dropped,last_midi_drops))return false;m_Logger.Write(FromKernel,LogWarning,"MIDI input queue lost events total %u; releasing input notes",dropped);return true;};
  bool discard_midi=midi_loss();
  Mk3MidiEvent me;
  while(midi->Pop(me)) {
   if(!midi_connected)continue;
   if(midi_loss())discard_midi=true;
   if(discard_midi)continue;
   groove::DiagnosticEvent diag;diag.kind=groove::DiagnosticKind::MidiIn;diag.timestamp=CTimer::GetClockTicks64();diag.size=me.length;for(unsigned i=0;i<3;++i)diag.data[i]=me.bytes[i];performance.diagnostics().accept(diag);
   if(me.cable<16 && !midi_cables_seen[me.cable]) { midi_cables_seen[me.cable]=true; m_Logger.Write(FromKernel,LogNotice,"MIDI input seen on USB cable %u",unsigned(me.cable)); }
   if(midi_probe.active()){
    groove::MidiPacket raw{0,me.cable,me.length,{me.bytes[0],me.bytes[1],me.bytes[2]}};
    bool match=midi_probe.receive(raw,CTimer::GetClockTicks64());
    if(midi_probe.owns_output())m_Logger.Write(FromKernel,LogNotice,"DIN MIDI rx seq %u match %u cable %u len %u bytes %02x %02x %02x rtt-us %u",match?midi_probe.last_index():midi_probe.index(),unsigned(match),unsigned(me.cable),unsigned(me.length),unsigned(me.bytes[0]),unsigned(me.bytes[1]),unsigned(me.bytes[2]),match?midi_probe.last_rtt():0);
    continue; // Test traffic must not feed notes, transport or MIDI Thru.
   }
   if(updater->active() || file_transfer->active())continue;
   groove::MidiPacket packet{0,me.cable,me.length,{me.bytes[0],me.bytes[1],me.bytes[2]}};
   if(!midi_input->accept(*engine,*project,packet)) m_Logger.Write(FromKernel,LogWarning,"MIDI input rejected or audio command queue full");
  }
  midi_probe.service(CTimer::GetClockTicks64(),midi_connected && !updater->active() && !file_transfer->active(),midi->Dropped(),[&](const groove::MidiPacket& packet){
   bool ok=midi->Send(packet.cable,packet.bytes,packet.length);
   m_Logger.Write(FromKernel,LogNotice,"DIN MIDI tx seq %u ok %u cable %u len %u bytes %02x %02x %02x",midi_probe.index(),unsigned(ok),unsigned(packet.cable),unsigned(packet.length),unsigned(packet.bytes[0]),unsigned(packet.bytes[1]),unsigned(packet.bytes[2]));return ok;
  });
  if(!midi_probe_reported && !midi_probe.active()){
   midi_probe_reported=true;const auto& r=midi_probe.result();
   m_Logger.Write(FromKernel,LogNotice,"DIN MIDI result pass %u sent %u received %u unexpected %u timeout %u send-error %u disconnect %u dropped %u rx-mask %u",unsigned(r.passed()),r.sent,r.received,r.unexpected,r.timeouts,r.send_errors,r.disconnected,r.dropped,unsigned(r.cables));
   m_Logger.Write(FromKernel,LogNotice,"DIN MIDI timing min-us %u avg-us %u max-us %u missing %u underrun %u overrun %u",r.min_us,r.received?unsigned(r.total_us/r.received):0,r.max_us,audio?audio->worker_missing()-midi_probe_missing:0,GrooveXHCIIsoEvents(0,2)-midi_probe_out,GrooveXHCIIsoEvents(1,3)-midi_probe_in);
  }
  if(hid){auto& diag=performance.diagnostics();if(!diag.connected || usb_changed){hid->Describe(diag);if(diag.enumerated){m_Logger.Write(FromKernel,LogNotice,"Audio descriptor map: async OUT %u, explicit feedback %u, no explicit feedback %u, capture EP %u (implicit usage %u)%s",diag.async_out,diag.explicit_feedback,diag.async_without_feedback,diag.capture_endpoints,diag.implicit_capture_endpoints,diag.endpoints_truncated?" (endpoint list truncated)":"");for(unsigned i=0;i<diag.endpoint_count;++i){const auto& e=diag.endpoints[i];m_Logger.Write(FromKernel,LogNotice,"IF %u alt %u class %02x/%02x EP %02x attr %02x max %u interval %u",unsigned(e.interface_number),unsigned(e.alternate),unsigned(e.interface_class),unsigned(e.interface_subclass),unsigned(e.address),unsigned(e.attributes),unsigned(e.max_packet),unsigned(e.interval));}}}groove::DiagnosticEvent raw;while(hid->PopDiagnostic(raw)){diag.accept(raw);if(input_probe_until && input_probe_left && raw.data[0]!=2){unsigned count=raw.size<32?raw.size:32;bool changed=!input_probe_seen;for(unsigned i=0;i<count;++i)if(input_probe_raw[i]!=raw.data[i])changed=true;if(changed){char hex[65];const char* digits="0123456789abcdef";for(unsigned i=0;i<count;++i){input_probe_raw[i]=raw.data[i];hex[2*i]=digits[raw.data[i]>>4];hex[2*i+1]=digits[raw.data[i]&15];}hex[count*2]=0;input_probe_seen=true;--input_probe_left;m_Logger.Write(FromKernel,LogNotice,"Input raw size %u hex %s",unsigned(raw.size),hex);}}}}
  else if(performance.diagnostics().connected)performance.diagnostics().disconnected();
  service_input();
  auto project_action=performance.take_project_action();
  if(project_action!=groove::Performance::ProjectAction::None){
   auto* candidate=groove::alloc_cpu_new<groove::ProjectState>();groove::RecoverySource source;bool ok=candidate!=nullptr;if(ok && project_action==groove::Performance::ProjectAction::New)groove::make_empty_project(*candidate);else if(ok)ok=m_Storage.Load(*candidate,&source);
   groove::WavView prepared[groove::kSampleAssets]{};if(ok)ok=m_Storage.prepare(*candidate,prepared);
   if(ok && audio){audio->Cancel();unsigned start=CTimer::GetClockTicks();while(audio->IsActive() && CTimer::GetClockTicks()-start<2000000)m_Timer.MsDelay(1);if(audio->IsActive())ok=false;else{delete audio;audio=nullptr;}}
   if(ok){float maintenance[2];engine->render(maintenance,0);performance.poll();ok=performance.stop();engine->render(maintenance,0);if(ok)ok=engine->stage_project(*candidate,prepared);if(ok){engine->render(maintenance,0);*project=*candidate;for(unsigned i=0;i<groove::kSampleAssets;++i)sample_bindings[i]=prepared[i];files.set_bindings(prepared);performance.project_loaded();if(project_action==groove::Performance::ProjectAction::New){performance.new_project_name();files.clear_preserved();performance.notify(groove::UiNotice::NewProject);}else{performance.project_path(m_Storage.CurrentProject());if(!files.sync_preserved_from(m_Storage.preserved())){m_Logger.Write(FromKernel,LogError,"Boot preservation handoff failed");return ShutdownHalt;}performance.notify_recovery(source);}}}
   if(!ok){performance.notify(groove::UiNotice::Failed);m_Logger.Write(FromKernel,LogWarning,"Project action failed; previous project retained");}delete candidate;
  }
  if(performance.take_save_request()) {if(files.project_save_busy())performance.request_save();else if(files.project_save_available()){unsigned started=CTimer::GetClockTicks();bool queued=files.start_project_save(performance.project_path());save_snapshot_us=uint32_t(CTimer::GetClockTicks()-started);save_service_max_us=0;if(!queued)performance.notify(groove::UiNotice::SaveFailed);}else{performance.sync_ui_settings();files.copy_preserved_to(m_Storage.preserved());bool saved=files.save(performance.project_path());if(saved)performance.notify(groove::UiNotice::Saved);else if(performance.require_project_save_safe())performance.notify(groove::UiNotice::SaveFailed);if(saved)m_Logger.Write(FromKernel,LogNotice,"Project saved");else m_Logger.Write(FromKernel,LogWarning,"Project save failed; inspect SD");}}
  {bool saved=false,probe=false;if(files.take_project_save_result(saved,probe)){if(efficiency_probe && efficiency_probe->save_result(saved,probe)){save_stages={};}else{if(probe)m_Logger.Write(FromKernel,saved?LogNotice:LogWarning,"Project save probe completed ok %u elapsed-ms %u snapshot-us %u service-max-us %u",unsigned(saved),unsigned(CTimer::GetClockTicks64()/1000-save_probe_started_ms),save_snapshot_us,save_service_max_us);if(probe){for(unsigned phase=1;phase<7;++phase)m_Logger.Write(FromKernel,LogNotice,"Save phase %u max-us %u total-us %u ui-us %u",phase,save_stages.max[phase],save_stages.total[phase],save_stages.ui[phase]);save_stages={};}if(probe && save_probe_path[0]){bool readback=saved;bool cleaned=RemoveProbeSong(m_Storage,save_probe_path);m_Logger.Write(FromKernel,LogNotice,"Project probe byte-readback %u cleaned %u path %s",unsigned(readback),unsigned(cleaned),save_probe_path);save_probe_path[0]=0;}else m_Logger.Write(FromKernel,saved?LogNotice:LogWarning,saved?"Project saved":"Project save failed; inspect SD");}}}
  if(performance.take_transfer_request()){
   bool idle=!updater->diagnostic_busy() && !ui_probe.busy() && !deployment_check.busy() && !(efficiency_probe && efficiency_probe->busy()) && !display_calibration.busy() && !fluid_probe.busy() && !updater->active() && !files.transfer_busy() && !(dsp_probe && dsp_probe->busy()) && !preset_check.busy() && !kit_check.busy() && !wave4_original && !native_running && !fx_stress_running && !(final_check && final_check->busy());
   bool opened=idle && QuiesceAudio(audio,*engine,performance,false) && file_transfer->Open(false);
   if(opened){float scratch[2];engine->render(scratch,0);performance.poll();}
   if(!opened && !file_transfer->active())performance.notify(idle?groove::UiNotice::Failed:groove::UiNotice::FilesBusy);
  }

  file_transfer->Poll();
  if(performance.take_update_request()) {
   if(file_transfer->active()){performance.notify(groove::UiNotice::Failed);continue;}
   if(files.recording_busy()){performance.notify(groove::UiNotice::Failed);m_Logger.Write(FromKernel,LogWarning,"Stop and finish recording before update");continue;}
   CPrepareUpdate preparation(audio,*engine,performance,m_Storage,*project,*updater);
   if(!preparation.run())m_Logger.Write(FromKernel,LogWarning,"Update blocked: audio stop or project save failed");
  }
  unsigned now=m_Timer.GetTicks();bool stalled=audio_drain.update(audio!=nullptr,audio && audio->IsActive(),audio && audio->operational(),now,MSEC2HZ(3000));if(stalled!=performance.diagnostics().audio_drain_stalled){performance.diagnostics().audio_drain_stalled=stalled;performance.mark_dirty();if(stalled)m_Logger.Write(FromKernel,LogWarning,"USB audio drain stalled; restart may be required");}
  performance.diagnostics().audio_active=audio && audio->operational();if(audio){performance.diagnostics().tx_channels=audio->tx_channels();performance.diagnostics().rx_channels=audio->rx_channels();}else if(!hid){performance.diagnostics().tx_channels=performance.diagnostics().rx_channels=0;}performance.audio_available(audio && audio->operational());if(!audio && !updater->active() && !file_transfer->active()){float maintenance[2];engine->render(maintenance,0);performance.poll();}
  if(audio && !audio->IsActive()) { delete audio; audio=nullptr; last_audio_attempt=now; m_Logger.Write(FromKernel,LogWarning,"USB audio stopped or removed"); }
  if(!updater->active() && !file_transfer->active() && !audio && now-last_audio_attempt>=MSEC2HZ(2000)) {
   last_audio_attempt=now;
   // Optional provisional profile uses decimal USB endpoint addresses; absent
   // options never infer a capture-to-playback association.
   unsigned fb_out=CKernelOptions::Get()->GetAppOptionDecimal("mk3fbout",0);
   unsigned fb_in=CKernelOptions::Get()->GetAppOptionDecimal("mk3fbin",0);
   const CUSBDevice* target=hid?hid->GetDevice():nullptr;
   unsigned tx=0;int index=FindMk3Audio(target,fb_out,tx);
   if(index>=0) {
    int rx=FindMk3Input(target,unsigned(index),fb_in,performance.diagnostics());
    groove::AudioRenderSlot* render_slot=nullptr;
#ifdef GROOVE_MULTICORE_DISPLAY
    if(audio_worker_ready)render_slot=&display_worker->AudioSlot();
#endif
    audio=new CMk3Audio(*engine,unsigned(index),rx>=0,rx>=0?unsigned(rx):0,tx,render_slot);audio_owner=target;
    if((fb_out || fb_in) && !audio->SetImplicitFeedbackProfile(fb_out,fb_in))
     m_Logger.Write(FromKernel,LogWarning,"Invalid implicit-feedback profile; expected decimal OUT 1..15 and IN 129..143");
    if(!audio->start()) { if(!audio->IsActive()){delete audio;audio=nullptr;} m_Logger.Write(FromKernel,LogWarning,"MK3 USB audio start failed; inspect descriptors and soundopt"); } else m_Logger.Write(FromKernel,LogNotice,"MK3 USB audio started at system timer %u ms (unverified)",CTimer::GetClockTicks()/1000); }
  }
  if(now-last_stats>=MSEC2HZ(5000)) {
   last_stats=now;
   if(performance.dropped()!=last_command_drops) { m_Logger.Write(FromKernel,LogWarning,"Audio command queue dropped %u",performance.dropped()); last_command_drops=performance.dropped(); }
   {unsigned value=midi_output_failures;if(value!=reported_faults[0]){reported_faults[0]=value;m_Logger.Write(FromKernel,LogWarning,"MIDI output failed/unavailable total %u",value);}}
   {unsigned value=engine->event_dropped();if(value!=reported_faults[1]){reported_faults[1]=value;m_Logger.Write(FromKernel,LogWarning,"UI event queue dropped %u",value);}}
   {unsigned value=engine->input_audio_dropped();if(value!=reported_faults[2]){reported_faults[2]=value;m_Logger.Write(FromKernel,LogWarning,"USB input audio frames dropped %u",value);}}
   {unsigned value=engine->repeat_dropped();if(value!=reported_faults[3]){reported_faults[3]=value;m_Logger.Write(FromKernel,LogWarning,"MIDI repeat held-note capacity exceeded %u",value);}}
   {unsigned value=engine->clock_dropped();if(value!=reported_faults[4]){reported_faults[4]=value;m_Logger.Write(FromKernel,LogWarning,"MIDI clock queue dropped %u",value);}}
   {unsigned value=hid?hid->invalid_knobs():0;if(value!=reported_faults[5]){reported_faults[5]=value;m_Logger.Write(FromKernel,LogWarning,"Knob range mismatches %u; capture raw controls before selecting another counter profile",value);}}
   {unsigned value=audio?audio->worker_missing():0;if(value!=reported_faults[6]){reported_faults[6]=value;if(!periodic_diagnostics)m_Logger.Write(FromKernel,LogWarning,"Audio worker missing frames %u",value);}}
   {unsigned value=GrooveXHCIIsoEvents(0,2);if(value!=reported_faults[7]){reported_faults[7]=value;m_Logger.Write(FromKernel,LogWarning,"Iso OUT underrun total %u",value);}}
   {unsigned value=GrooveXHCIIsoEvents(1,3);if(value!=reported_faults[8]){reported_faults[8]=value;m_Logger.Write(FromKernel,LogWarning,"Iso IN overrun total %u",value);}}
   {unsigned value=audio?audio->GetStats().bad_chunks:0;if(value!=reported_faults[9]){reported_faults[9]=value;m_Logger.Write(FromKernel,LogWarning,"Audio bad chunks total %u",value);}}
   {unsigned value=audio?audio->GetGapStats().over1000:0;if(value!=reported_faults[10]){reported_faults[10]=value;m_Logger.Write(FromKernel,LogWarning,"Audio callback gaps over1ms total %u",value);}}
   if(!startup_health_logged || periodic_diagnostics){startup_health_logged=true;
   m_Logger.Write(FromKernel,LogNotice,"Iso ring ep2 underrun %u ep3 overrun %u",GrooveXHCIIsoEvents(0,2),GrooveXHCIIsoEvents(1,3));
   m_Logger.Write(FromKernel,LogNotice,"UI state mode %u playing %u modal %u update %u",unsigned(performance.mode()),unsigned(performance.playing()),unsigned(performance.modal_active()),unsigned(updater->active()));
   auto screen_stats=display.GetStats();m_Logger.Write(FromKernel,LogNotice,"Display submitted %u completed %u bytes %u max-usb-us %u max-reap-us %u max-draw-us %u draws %u max-loop-us %u worker %u/%u",screen_stats.submitted,screen_stats.completed,screen_stats.bytes,screen_stats.max_usb_us,screen_stats.max_reap_us,max_draw_us,draw_count,max_loop_us,screen_stats.worker_completed,screen_stats.worker_submitted);
   if(draw_timed_count)m_Logger.Write(FromKernel,LogNotice,"Display timing max-draw-minus-audio-us %u timed-draws %u",max_draw_minus_audio_us,draw_timed_count);
   if(audio){auto w=audio->GetWorkerStats();m_Logger.Write(FromKernel,LogNotice,"Audio worker missing frames %u blocks %u avg-us %u max-us %u",audio->worker_missing(),w.blocks,w.blocks?unsigned(w.total_us/w.blocks):0,w.max_us);}
   if(audio) { auto stats=audio->GetStats(); m_Logger.Write(FromKernel,LogNotice,"Audio frames %u callbacks %u max-callback-us %u bad-chunks %u over125 %u over250 %u",unsigned(stats.frames),stats.callbacks,stats.max_us,stats.bad_chunks,stats.over_125us,stats.over_250us);auto gaps=audio->GetGapStats();m_Logger.Write(FromKernel,LogNotice,"Audio callback gaps max-us %u over500 %u over1000 %u",gaps.maximum,gaps.over500,gaps.over1000); }
  }
  }
  service_ui();
  updater->PollTrialFallback();if(updater->active())updater->Process();
#ifdef GROOVE_MULTICORE_DISPLAY
  efficiency_core0.finish();
#endif
  if(updater->active()||updater->network_started()||file_transfer->active())m_Scheduler.usSleep(200);else m_Timer.usDelay(200);
 }
}
