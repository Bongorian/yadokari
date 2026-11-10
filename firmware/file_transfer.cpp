#include "../src/file_http_deadline.h"
#include "file_transfer.h"
#include "update_runtime.h"
#include "project_storage.h"
#include "storage_ui_wait.h"
#include "../src/performance.h"
#include "../src/file_http_header.h"
#include "../src/file_web_assets.h"
#include "../src/file_dhcp.h"
#include "../src/file_bulk.h"
#include "../src/transfer_lifetime.h"
#include <circle/net/netbuffer.h>
#include <circle/net/transportlayer.h>
#include <circle/net/mdnspublisher.h>
#include <circle/bcmrandom.h>
#include <circle/sched/scheduler.h>

using namespace groove;
struct CFileTransfer::Impl {
 UserFiles files;bulk::Service usb;bool (*usb_open)(void*)=nullptr;void* usb_owner=nullptr;Performance& ui;CUpdateRuntime& updater;FileWeb* web=nullptr;
 TransferView view{};char ap_ssid[40]{};CNetSubSystem* net=nullptr;CTransportLayer* transport=nullptr;
 int listener=-1,client=-1;CSocket* dhcp=nullptr;FileDhcp leases;CmDNSPublisher* mdns=nullptr;bool published=false;unsigned mdns_attempts=0;u64 mdns_due=0;
 TransferLifetime lifetime;FileHttpDeadline http_deadline;u64 upload_activity=0;bool want_close=false,putting=false,headers=false,responding=false,root=false,download=false,owned_request=false;
 char header[2048]{};unsigned header_used=0,body_used=0;uint8_t body[32768]{};
 FileHttpHeader request;CString response;unsigned response_at=0,output_at=0,output_used=0;u64 download_at=0,input_total=0;uint8_t output[32768]{};
 const char* response_body=nullptr;unsigned response_bytes=0;
 Impl(CProjectStorage& fs,const ProjectState& p,Performance& perf,CUpdateRuntime& runtime):files(fs,p),usb(files),ui(perf),updater(runtime){files.reference_identity([](void* owner,const char* a,const char* b){return static_cast<CProjectStorage*>(owner)->same_reference(a,b);},&fs);usb.callbacks([](void* p){auto& x=*static_cast<Impl*>(p);return x.usb_open && x.usb_open(x.usb_owner);},[](void* p){static_cast<Impl*>(p)->want_close=true;},this);}
 static bool Continue(void* context){auto& x=*static_cast<Impl*>(context);if((!x.view.usb && x.lifetime.expired(CTimer::GetClockTicks64())) || x.ui.take_transfer_close())x.want_close=true;return !x.want_close;}
 void Disconnect(bool abort){if(abort && owned_request)files.cancel();if(client>=0 && transport)transport->Disconnect(client);client=-1;header_used=body_used=response_at=output_at=output_used=0;putting=headers=responding=root=download=owned_request=false;input_total=0;request={};response="";response_body=nullptr;response_bytes=0;download_at=0;}
 void Respond(unsigned status,const char* type,const char* text,unsigned size,u64 file_length=0){http_deadline.response(CTimer::GetClockTicks64());if(status>=400)copy_text(view.status,sizeof view.status,status==403?"CODE REQUIRED":status==413?"REQUEST TOO LARGE":files.error()==UserFileError::Ok?"REQUEST REJECTED":user_file_error(files.error()));CString extra;if(web && web->authorized(request.code))extra.Format("Set-Cookie: MGX=%s; HttpOnly; SameSite=Strict; Path=/api/v1/\r\n",view.code);
  if(download){extra.Append("Content-Disposition: attachment; filename*=UTF-8''");const char* base=filename(web->file);const char hex[]="0123456789ABCDEF";for(const char* p=base;*p;++p){unsigned c=uint8_t(*p);char encoded[]={'%',hex[c>>4],hex[c&15],0};extra.Append(encoded);}extra.Append("\r\n");}
  response.Format("HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %llu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n%s\r\n",status,status==200?"OK":"Error",type,static_cast<unsigned long long>(download?file_length:size),static_cast<const char*>(extra));response_body=text;response_bytes=size;response_at=0;responding=true;}

 void Bad(unsigned status=400){static const char error[]="{\"error\":\"INVALID_HTTP\"}";Respond(status,"application/json",error,sizeof error-1);}
 void Execute(){
  if(!__builtin_strcmp(request.method,"GET") && !__builtin_strcmp(request.url,"/")){root=true;Respond(200,"text/html; charset=utf-8",kFileWebPage,sizeof kFileWebPage-1);return;}
  StorageUiWait::Scope responsive;web->request(request.method,request.url,request.code,body,body_used);download=web->file[0]!=0;Respond(web->status,web->content_type,web->body(),web->bytes(),web->file_bytes);upload_activity=CTimer::GetClockTicks64();
  if(web->status==200)copy_text(view.status,sizeof view.status,files.uploading()?"RECEIVING":"READY");
 }
 void FinishPut(){copy_text(view.status,sizeof view.status,"VALIDATING");ui.transfer_view(view);StorageUiWait::Scope responsive;http_deadline.commit();web->finish_put();Respond(web->status,web->content_type,web->body(),web->bytes());copy_text(view.status,sizeof view.status,web->status==200?"READY":user_file_error(files.error()));}
 void Feed(const uint8_t* bytes,unsigned n){unsigned i=0;while(i<n && !responding){
   if(!headers){if(header_used+1>=sizeof header){Bad(413);return;}header[header_used++]=char(bytes[i++]);header[header_used]=0;
    if(header_used>=4 && !__builtin_memcmp(header+header_used-4,"\r\n\r\n",4)){if(!request.parse(header)){Bad();return;}headers=true;
     if(!__builtin_strcmp(request.method,"PUT")){if(!web->begin_put(request.url,request.code,request.length)){Respond(web->status,web->content_type,web->body(),web->bytes());return;}putting=owned_request=true;upload_activity=CTimer::GetClockTicks64();http_deadline.progress(upload_activity);copy_text(view.status,sizeof view.status,"RECEIVING");}
     else if(!request.length){if(i!=n){Bad();return;}Execute();}
    }
   }else{
    unsigned room=sizeof body-body_used;uint64_t remaining=request.length-input_total;unsigned count=n-i;if(count>room)count=room;if(count>remaining)count=unsigned(remaining);if(!count){Bad(413);return;}__builtin_memcpy(body+body_used,bytes+i,count);body_used+=count;i+=count;input_total+=count;
    if(input_total==request.length && i!=n){if(owned_request)files.cancel();Bad();return;}
    if(putting && (body_used==sizeof body || input_total==request.length)){StorageUiWait::Scope responsive;bool ok=web->put_chunk(files.received(),body,body_used);body_used=0;upload_activity=CTimer::GetClockTicks64();http_deadline.progress(upload_activity);if(!ok){Respond(web->status,web->content_type,web->body(),web->bytes());return;}}
    if(input_total==request.length){if(putting)FinishPut();else Execute();}
   }
  }}

 int Send(const void* data,unsigned n){if(!n)return 0;if(!transport->GetStatus(client).bTxReady)return 0;unsigned maximum=transport->GetMSS(client);if(!maximum)return -1;if(n>maximum)n=maximum;auto* packet=new CNetBuffer(CNetBuffer::TCPSend,n,data);int result=transport->Send(packet,MSG_DONTWAIT,client);if(result<0){delete packet;return result;}return int(n);}
 void Write(){int n=0;const char* h=response;unsigned header_length=__builtin_strlen(h);
  if(response_at<header_length){n=Send(h+response_at,header_length-response_at);if(n>0)response_at+=unsigned(n);}
  else if(download){if(output_at==output_used){if(download_at==web->file_bytes){Disconnect(false);return;}unsigned got=0,wanted=unsigned(web->file_bytes-download_at>sizeof output?sizeof output:web->file_bytes-download_at);if(!files.get(web->file,download_at,output,wanted,got) || got!=wanted){Disconnect(false);return;}output_used=got;output_at=0;}n=Send(output+output_at,output_used-output_at);if(n>0){output_at+=unsigned(n);download_at+=unsigned(n);}}
  else {if(output_at==response_bytes){Disconnect(false);return;}n=Send(response_body+output_at,response_bytes-output_at);if(n>0)output_at+=unsigned(n);}
  if(n<0){Disconnect(true);return;}if(n>0)http_deadline.progress(CTimer::GetClockTicks64());
 }
 void Dhcp(){if(!dhcp)return;uint8_t input[1600],reply[320];CIPAddress peer;u16 port=0;int n=dhcp->ReceiveFrom(input,sizeof input,MSG_DONTWAIT,&peer,&port);if(n<=0 || port!=68)return;unsigned bytes=leases.reply(input,unsigned(n),reply,sizeof reply,CTimer::GetClockTicks64());if(bytes){const u8 address[]={255,255,255,255};CIPAddress broadcast(address);dhcp->SendTo(reply,bytes,MSG_DONTWAIT,broadcast,68);}}
 void Publish(){u64 now=CTimer::GetClockTicks64();if(published || mdns_attempts>=5 || now<mdns_due || !net->IsRunning())return;mdns_due=now+1000000;++mdns_attempts;if(!mdns)mdns=new CmDNSPublisher(net);published=mdns->PublishService("Groovebox files","_http._tcp",80);CLogger::Get()->Write("files",LogNotice,"mDNS publish %u attempt %u",unsigned(published),mdns_attempts);if(published){CString host=CmDNSDaemon::Get()->GetHostname();host.Append(".local");copy_text(view.discovery,sizeof view.discovery,host);}else if(mdns_attempts==5)copy_text(view.discovery,sizeof view.discovery,"mDNS unavailable / use address");}
};
CFileTransfer::CFileTransfer(CProjectStorage& fs,const ProjectState& project,Performance& ui,CUpdateRuntime& updater):impl_(new Impl(fs,project,ui,updater)){}
CFileTransfer::~CFileTransfer(){Close();delete impl_;}
bool CFileTransfer::active()const{return impl_->view.active;}
groove::bulk::Port* CFileTransfer::UsbPort(){return &impl_->usb;}
void CFileTransfer::UsbPrepare(bool (*open)(void*),void* owner){impl_->usb_open=open;impl_->usb_owner=owner;}
bool CFileTransfer::Open(bool busy,bool station,bool usb){auto& x=*impl_;if(x.view.active || busy || x.updater.active())return false;x.net=nullptr;x.transport=nullptr;if(!x.ui.stop() || !x.ui.panic() || !x.ui.stop_audition())return false;x.view={};x.want_close=false;x.files.continuation(Impl::Continue,&x);x.mdns_attempts=0;x.mdns_due=0;x.leases=FileDhcp{};x.view.active=true;x.lifetime.open(CTimer::GetClockTicks64());if(usb){if(!x.files.open(x.ui.project_path())){x.view={};return false;}x.view.usb=x.view.started=true;copy_text(x.view.ssid,sizeof x.view.ssid,"USB-C / MGF1 v1");copy_text(x.view.status,sizeof x.view.status,"USB FILE SESSION");x.ui.transfer_view(x.view);return true;}CBcmRandomNumberGenerator rng;CString code;code.Format("%08x",rng.GetNumber());copy_text(x.view.code,sizeof x.view.code,code);CString ssid;ssid.Format("Groovebox-%08x",rng.GetNumber());copy_text(x.ap_ssid,sizeof x.ap_ssid,ssid);copy_text(x.view.ssid,sizeof x.view.ssid,ssid);copy_text(x.view.status,sizeof x.view.status,"STARTING WI-FI");x.ui.transfer_view(x.view);
 copy_text(x.view.status,sizeof x.view.status,"READY TO START");x.view.station=station;x.ui.transfer_view(x.view);return true;
}

void CFileTransfer::Close(){auto& x=*impl_;if(!x.view.active)return;bool browse=x.ui.take_transfer_browse();x.ui.take_transfer_start();char directory[192];copy_text(directory,sizeof directory,x.files.destination()[0]?x.files.destination():"PROJECTS/");parent_path(directory);x.Disconnect(true);if(x.listener>=0 && x.transport && x.transport->GetOwnPort(x.listener)==80)x.transport->Disconnect(x.listener);x.listener=-1;if(x.published){x.mdns->UnpublishService("Groovebox files");x.published=false;}delete x.dhcp;x.dhcp=nullptr;delete x.web;x.web=nullptr;x.usb.disconnected();x.files.close();bool radio_ok=!x.view.started || x.view.usb || x.updater.EndFilesNetwork();x.net=nullptr;x.transport=nullptr;x.lifetime.close();x.view={};x.ui.transfer_view(x.view);if(browse){copy_text(x.ui.browser().directory,sizeof x.ui.browser().directory,directory);x.ui.browser().cursor=0;x.ui.set_mode(UiMode::Browser);x.ui.request_file(FileAction::Refresh);}if(!radio_ok)x.ui.notify(UiNotice::Failed);CLogger::Get()->Write("files",LogNotice,"HTTP transfer mode stopped");x.ui.mark_dirty();}
void CFileTransfer::Poll(){auto& x=*impl_;if(!x.view.active)return;u64 now=CTimer::GetClockTicks64();if(x.want_close || (!x.view.usb && x.lifetime.expired(now)) || x.ui.take_transfer_close()){Close();return;}x.view.remaining=x.view.usb?30:x.lifetime.remaining(now);if(x.view.usb){x.view.received=x.files.received();x.view.length=x.files.length();x.ui.transfer_view(x.view);return;}
 if(x.ui.take_transfer_start() && !x.view.started && !x.view.failed){x.view.station=x.ui.transfer_view().station;x.view.started=true;copy_text(x.view.status,sizeof x.view.status,"STARTING WI-FI");x.ui.transfer_view(x.view);x.net=x.updater.BeginFilesNetwork(x.ap_ssid,x.view.station);
  if(!x.net){x.view.failed=true;copy_text(x.view.status,sizeof x.view.status,x.updater.NetworkStatus());x.ui.transfer_view(x.view);return;}
  x.view.station=!x.updater.FilesAccessPoint();copy_text(x.view.ssid,sizeof x.view.ssid,x.view.station?"EXISTING WI-FI":x.ap_ssid);if(!x.files.open(x.ui.project_path())){x.view.failed=true;copy_text(x.view.status,sizeof x.view.status,"DATA RECOVERY REQUIRED");x.ui.transfer_view(x.view);return;}x.web=new FileWeb(x.files,x.view.code);x.transport=x.net->GetTransportLayer();
  if(!x.view.station){x.dhcp=new CSocket(x.net,IPPROTO_UDP);if(x.dhcp->Bind(67)<0 || x.dhcp->SetOptionBroadcast(TRUE)<0){x.view.failed=true;copy_text(x.view.status,sizeof x.view.status,"DHCP START FAILED");x.ui.transfer_view(x.view);return;}}
  copy_text(x.view.status,sizeof x.view.status,"WAITING FOR WI-FI");
 }
 if(x.view.failed){x.ui.transfer_view(x.view);return;}
 if(!x.view.started && !x.view.failed){x.view.station=x.ui.transfer_view().station;copy_text(x.view.ssid,sizeof x.view.ssid,x.view.station?"EXISTING WI-FI":x.ap_ssid);}
 if(!x.net || !x.web){x.ui.transfer_view(x.view);return;}x.Dhcp();if(!x.net->IsRunning()){x.ui.transfer_view(x.view);return;}if(x.listener<0){x.listener=x.transport->Listen(80,IPPROTO_TCP);if(x.listener<0){x.view.failed=true;copy_text(x.view.status,sizeof x.view.status,"HTTP START FAILED");x.ui.transfer_view(x.view);return;}CString ip;x.net->GetConfig()->GetIPAddress()->Format(&ip);CString address;address.Format("http://%s/",static_cast<const char*>(ip));copy_text(x.view.address,sizeof x.view.address,address);copy_text(x.view.status,sizeof x.view.status,"READY");CLogger::Get()->Write("files",LogNotice,"HTTP transfer mode started / AP %u / port 80",unsigned(x.updater.FilesAccessPoint()));}
 x.Publish();if(x.listener>=0 && x.transport->GetOwnPort(x.listener)!=80)x.listener=-1;if(x.files.uploading() && now-x.upload_activity>30000000){x.files.cancel();copy_text(x.view.status,sizeof x.view.status,"UPLOAD TIMED OUT");}
 if(x.client<0 && x.listener>=0 && x.transport->IsConnected(x.listener)){CIPAddress peer;u16 port;if(x.transport->Accept(&peer,&port,x.listener)>=0){x.client=x.listener;x.listener=x.transport->Listen(80,IPPROTO_TCP);x.http_deadline.receive(now);}else{x.transport->Disconnect(x.listener);x.listener=-1;}}
 if(x.client>=0){if(x.http_deadline.expired(now))x.Disconnect(true);else if(x.responding)x.Write();else {CNetBuffer* packet=nullptr;int n=x.transport->Receive(&packet,MSG_DONTWAIT,x.client);if(n<0)x.Disconnect(true);else if(n>0){x.http_deadline.progress(now);x.Feed(static_cast<const uint8_t*>(packet->GetPtr()),unsigned(n));}else if(x.transport->GetStatus(x.client).bRxReady && !x.transport->IsConnected(x.client))x.Disconnect(true);delete packet;}}
 x.view.received=x.files.received();x.view.length=x.files.length();x.ui.transfer_view(x.view);
}
