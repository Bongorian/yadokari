// Loopback-only HTTP adapter for qualification of the firmware's shared core.
#include "file_http_header.h"
#include "file_web_assets.h"
#include "../../tests/file_transfer_fixtures.h"
#include "../../platform/desktop/file_access.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
using namespace groove;
static volatile sig_atomic_t running=1;
static void stop(int){running=0;}
static bool send_all(int socket,const void* input,size_t bytes){const char* p=static_cast<const char*>(input);while(bytes && running){ssize_t n=send(socket,p,bytes,MSG_NOSIGNAL);if(n<=0)return false;p+=n;bytes-=size_t(n);}return !bytes;}
int main(int argc,char** argv){if(argc!=2 || chdir(argv[1]))return 2;struct sigaction sa{};sa.sa_handler=stop;sigaction(SIGTERM,&sa,nullptr);sigaction(SIGINT,&sa,nullptr);
 DesktopFiles fs;if(!file_fixture::files(fs))return 3;auto project=std::make_unique<ProjectState>();make_empty_project(*project);copy_text(project->assets[0].path,sizeof project->assets[0].path,"ASSETS/SAMPLES/Locked.wav");UserFiles files(fs,*project);files.open("PROJECTS/Open.mgxs");FileWeb web(files,"TESTCODE");
 int listener=socket(AF_INET,SOCK_STREAM,0);sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(listener<0 || bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof address) || listen(listener,4))return 4;socklen_t size=sizeof address;getsockname(listener,reinterpret_cast<sockaddr*>(&address),&size);std::printf("PORT %u\n",ntohs(address.sin_port));std::fflush(stdout);
 while(running){int client=accept(listener,nullptr,nullptr);if(client<0)continue;timeval timeout{2,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
  char header[2048]{};unsigned used=0;bool parsed=false;while(running && used+1<sizeof header){ssize_t n=recv(client,header+used,1,0);if(n!=1)break;header[++used]=0;if(used>=4 && !std::memcmp(header+used-4,"\r\n\r\n",4)){parsed=true;break;}}
  FileHttpHeader request;bool ok=parsed && request.parse(header);bool put=ok && !std::strcmp(request.method,"PUT");bool root=ok && !std::strcmp(request.method,"GET") && !std::strcmp(request.url,"/");uint8_t buffer[32768];uint64_t received=0;
  if(put){ok=web.begin_put(request.url,request.code,request.length);while(ok && running && received<request.length){unsigned wanted=unsigned(request.length-received>sizeof buffer?sizeof buffer:request.length-received);ssize_t n=recv(client,buffer,wanted,0);if(n<=0){ok=false;break;}ok=web.put_chunk(received,buffer,unsigned(n));received+=uint64_t(n);}if(ok && running)ok=web.finish_put();else files.cancel();}
  else if(ok && !root){if(request.length){ok=false;}else web.request(request.method,request.url,request.code,nullptr,0);}
  unsigned status=!parsed?400:!ok && !put?400:root?200:web.status;const char* body=root?kFileWebPage:web.body();size_t bytes=root?sizeof kFileWebPage-1:web.bytes();bool download=!root && web.file[0] && status==200;uint64_t length=download?web.file_bytes:bytes;
  char response[512];int n=std::snprintf(response,sizeof response,"HTTP/1.1 %u Result\r\nContent-Length: %llu\r\nConnection: close\r\nContent-Type: %s\r\n%s\r\n",status,static_cast<unsigned long long>(length),root?"text/html":web.content_type,web.authorized(request.code)?"Set-Cookie: MGX=TESTCODE; HttpOnly; SameSite=Strict; Path=/api/v1/\r\n":"");
  if(running && send_all(client,response,size_t(n))){if(download){for(uint64_t at=0;running && at<length;){unsigned got=0,wanted=unsigned(length-at>sizeof buffer?sizeof buffer:length-at);if(!files.get(web.file,at,buffer,wanted,got) || !got || !send_all(client,buffer,got))break;at+=got;}}else send_all(client,body,bytes);}close(client);
 }files.close();close(listener);return 0;
}
