#pragma once
namespace groove {namespace bulk{class Service;}class IFileAccess;struct ProjectState;class Performance;}
class CUpdateRuntime;
class CProjectStorage;
class CFileTransfer {
 struct Impl;Impl* impl_;
public:
 CFileTransfer(CProjectStorage&,const groove::ProjectState&,groove::Performance&,CUpdateRuntime&);
 ~CFileTransfer();
 // Kernel must quiesce/join audio first; it resumes audio after Close.
 bool Open(bool busy,bool station=false,bool usb=false);
 groove::bulk::Service* UsbPort();
 void UsbPrepare(bool (*open)(void*),void* owner);
 void Close();
 bool active()const;
 void Poll();
};
