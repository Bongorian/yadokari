#pragma once
#include <circle/bcmwatchdog.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
// Circle's watchdog feed preserves PM_RSTS. Explicitly select recovery (1)
// once, so an application timeout cannot repeat the last explicit slot handoff.
// Uses the same documented partition mask as CBcmWatchdog::Restart; tryboot
// itself is handled exclusively through the official firmware mailbox.
class CRecoveryWatchdog {
 CBcmWatchdog m_Watchdog;bool m_Selected=false;
public:
 void Start(unsigned seconds=15){if(!m_Selected){write32(ARM_PM_RSTS,ARM_PM_PASSWD|1|(read32(ARM_PM_RSTS)&ARM_PM_RSTS_PART_CLEAR));m_Selected=true;}m_Watchdog.Start(seconds);}
};
