#pragma once
#include "serial_control.h"
namespace groove { namespace update {
// Only the physical CDC dispatcher supplies a file service. Recovery supplies none.
template<class Service> bool control_usb_files(SerialCommand command,Service* files,uint64_t now){
 if(!files)return false;
 if(command==SerialCommand::FilesStop)return files->stop();
 if(command!=SerialCommand::FilesStart || !files->start())return false;
 files->activity(now);
 return true;
}
enum class UsbInputKind:uint8_t { None, Command, BinaryStart, FileStart };
struct UsbInput {UsbInputKind kind=UsbInputKind::None;SerialCommand command=SerialCommand::None;};
// USB CDC shares a stream between exact control lines and existing GUP1 data.
// A binary transfer may begin only at a control-line boundary in update mode.
class UsbControlMux {
 SerialControl control_;
public:
 UsbInput feed(uint8_t byte,bool update_active,bool file_available=false){
  if(control_.idle() && byte=='M' && file_available)return {UsbInputKind::FileStart,SerialCommand::None};
  if(control_.idle() && byte=='G' && update_active)return {UsbInputKind::BinaryStart,SerialCommand::None};
  SerialCommand command=control_.feed(byte);
  return command==SerialCommand::None?UsbInput{}:UsbInput{UsbInputKind::Command,command};
 }
};
}}
