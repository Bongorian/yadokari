#pragma once
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <circle/string.h>
#include <fatfs/ff.h>
#ifdef GROOVE_UPDATE_WIFI
#include <wlan/bcm4343.h>
#endif

// A bounded, explicit operator diagnostic. It never replaces the live update
// network or opens a file server. Failed driver initialization is not retried
// in place: the upstream driver owns singleton/task state until reboot.
class CWlanPreflight {
 bool m_Attempted=false,m_Open=false;
 u64 m_Deadline=0;
#ifdef GROOVE_UPDATE_WIFI
 CBcm4343Device* m_Device=nullptr;
#endif
public:
 void Start(){
  if(m_Attempted)return;
  m_Attempted=true;
#ifdef GROOVE_UPDATE_WIFI
  const char* assets[]={"brcmfmac43455-sdio.bin","brcmfmac43455-sdio.txt","brcmfmac43455-sdio.clm_blob"};
  bool ready=true;
  for(const char* name:assets){CString path;path.Format("DATA:/firmware/%s",name);FILINFO info{};bool present=f_stat(static_cast<const char*>(path),&info)==FR_OK && !(info.fattrib&AM_DIR) && info.fsize;
   CLogger::Get()->Write("wlan-check",LogNotice,"Asset %s present %u bytes %u",name,unsigned(present),present?unsigned(info.fsize):0);ready=ready && present;}
  if(!ready){CLogger::Get()->Write("wlan-check",LogWarning,"WLAN unavailable: required DATA firmware absent; no driver/AP attempted");return;}
  m_Device=new CBcm4343Device("DATA:/firmware/");
  bool initialized=m_Device->Initialize();
  CLogger::Get()->Write("wlan-check",LogNotice,"Driver initialized %u",unsigned(initialized));
  if(!initialized)return;
  m_Open=m_Device->CreateOpenNet("GROOVEBOX-CHECK",6,false);
  CLogger::Get()->Write("wlan-check",LogNotice,"AP create %u SSID GROOVEBOX-CHECK channel 6; no HTTP; timeout 60s",unsigned(m_Open));
  m_Deadline=CTimer::GetClockTicks64()+60000000;
#else
  CLogger::Get()->Write("wlan-check",LogWarning,"WLAN unavailable: not compiled");
#endif
 }
 void Poll(){
#ifdef GROOVE_UPDATE_WIFI
  if(m_Open && CTimer::GetClockTicks64()>=m_Deadline){bool stopped=m_Device->DestroyOpenNet();m_Open=false;CLogger::Get()->Write("wlan-check",LogNotice,"AP stopped %u",unsigned(stopped));}
#endif
 }
};
