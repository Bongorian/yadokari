#include "../../src/recovery_frame.h"
#include "kernel.h"
#include "../kernel_size_marker.h"
#include "../update_runtime.h"
#include "../serial_update_control.h"
#include "../circle_display.h"
#include "../../src/controller_output.h"
#include <circle/alloc.h>
bool CRecoveryKernel::Initialize(){
 if(!m_Serial.Initialize(115200))return false;
 static const char started[]="recovery boot: serial ready\n";m_Serial.Write(started,sizeof(started)-1);
 if(!m_Logger.Initialize(&m_Serial)){static const char message[]="recovery boot: logger initialization failed\n";m_Serial.Write(message,sizeof(message)-1);return false;}
 if(!m_Interrupt.Initialize()){static const char message[]="recovery boot: interrupt initialization failed\n";m_Serial.Write(message,sizeof(message)-1);return false;}
 if(!m_Timer.Initialize()){static const char message[]="recovery boot: timer initialization failed\n";m_Serial.Write(message,sizeof(message)-1);return false;}
 m_Logger.Write("recovery",LogNotice,"Recovery firmware %u: initializing SD and USB host",GrooveFirmwareVersion());
 m_Watchdog.Start(15);
 if(!m_SD.Initialize()){m_Logger.Write("recovery",LogError,"SD controller initialization failed");return false;}
 if(!m_USB.Initialize()){m_Logger.Write("recovery",LogError,"USB host initialization failed");return false;}
 m_Logger.Write("recovery",LogNotice,"SD and USB host initialized");return true;
}
void CRecoveryKernel::Run(){
 CUpdateRuntime updater(&m_Interrupt);updater.Initialize(true);groove::update::SerialControl serial_control;auto* renderer=new groove::SoftwareCanvas();CircleDisplay display(static_cast<u8*>(palloc()));groove::ControllerOutputPump leds;u8 pads[16]{},buttons[62]{};updater.Leds(pads,buttons);leds.update(pads,buttons);unsigned last=0;groove::RecoveryFramePublisher frames;
 m_Logger.Write("recovery",LogNotice,"Independent recovery; any update pad cancels five-second active-slot handoff");
 for(;;){m_Watchdog.Start(15);bool changed=m_USB.UpdatePlugAndPlay();auto* hid=static_cast<CMk3Hid*>(m_Names.GetDevice("mk3hid",FALSE));auto* screen=static_cast<CMk3Display*>(m_Names.GetDevice("mk3display",FALSE));if(!hid || !screen || hid->GetDevice()!=screen->GetDevice())screen=nullptr;if(!hid)updater.ReleasePads();if(changed){display.Bind(nullptr);display.Invalidate();leds.invalidate();}display.Bind(screen);if(hid){Mk3Pad input;while(hid->Pop(input)){if(input.type==Mk3EventType::Pad)updater.Pad(input.index,input.tag);else if(input.type==Mk3EventType::ControlByte || input.type==Mk3EventType::ControlBaseline)updater.Control(input.index,input.tag,input.type==Mk3EventType::ControlBaseline);}}PollSerialUpdate(m_Serial,serial_control,updater,[]{return false;});updater.PollUsbControl([]{return false;});updater.PollNetworkControl([]{return false;});updater.Process();unsigned now=CTimer::GetClockTicks();if(now-last>=50000){last=now;frames.draw(updater,*renderer,display,screen!=nullptr);updater.Leds(pads,buttons);leds.update(pads,buttons);if(hid)leds.flush(*hid);}display.present(0,renderer->pixels[0],480,272);display.present(1,renderer->pixels[1],480,272);m_Scheduler.MsSleep(1);}
}
