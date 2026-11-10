#pragma once
#include "update_runtime.h"
#include <circle/serial.h>
#include <circle/string.h>
#include <circle/util.h>
// UART is a physically trusted control channel, enabled only by DATA:/REMOTE.ON.
// The caller owns and polls this parser from the non-audio main task.
template<class Enter> void PollSerialUpdate(CSerialDevice& serial,groove::update::SerialControl& parser,CUpdateRuntime& updater,Enter enter){
 using groove::update::SerialCommand;
 unsigned char bytes[64];int count=serial.Read(bytes,sizeof bytes);if(count<=0)return;
 for(int i=0;i<count;++i){SerialCommand command=parser.feed(bytes[i]);if(command==SerialCommand::None)continue;
  if(command==SerialCommand::Status){CString status;updater.RemoteStatus(status);const char* text=status;serial.Write(text,strlen(text));continue;}
  if(command==SerialCommand::Log && updater.remote_enabled()){CString log;updater.RemoteLog(log);const char* text=log;serial.Write(text,strlen(text));continue;}
  if(command==SerialCommand::Hardware && updater.remote_enabled()){CString info;updater.RemoteHardware(info);const char* text=info;serial.Write(text,strlen(text));continue;}
  bool ok=false;if(updater.remote_ready())ok=command==SerialCommand::Enter?(updater.active()?updater.RemoteEnterActive():enter()):updater.Remote(command);
  CString result;result.Format("GBX1 RESULT command=%u ok=%u\n",unsigned(command),unsigned(ok));const char* text=result;serial.Write(text,strlen(text));
 }
}
