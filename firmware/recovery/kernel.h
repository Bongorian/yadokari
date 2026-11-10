#pragma once
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/bcmwatchdog.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include "../recovery_watchdog.h"
class CRecoveryKernel {
 CActLED m_LED;CKernelOptions m_Options;CDeviceNameService m_Names;CSerialDevice m_Serial;CExceptionHandler m_Exception;CInterruptSystem m_Interrupt;CTimer m_Timer;CLogger m_Logger;CScheduler m_Scheduler;CRecoveryWatchdog m_Watchdog;CUSBHCIDevice m_USB;CEMMCDevice m_SD;
public:CRecoveryKernel():m_Timer(&m_Interrupt),m_Logger(m_Options.GetLogLevel(),&m_Timer),m_USB(&m_Interrupt,&m_Timer,TRUE),m_SD(&m_Interrupt,&m_Timer,&m_LED){}
 bool Initialize();void Run();
};
