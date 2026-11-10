"""Reproduce the ordered Circle patch set; execute the actual audio pool path."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];circle=Path(os.environ.get('CIRCLEHOME',root/'.deps/circle-build'))
assert (circle/'.git').exists(),'Pinned Circle checkout required for request-lifetime qualification'
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp);archive=subprocess.Popen(['git','-C',str(circle),'archive','HEAD'],stdout=subprocess.PIPE)
 subprocess.run(['tar','-x','-C',str(base)],stdin=archive.stdout,check=True);archive.stdout.close();assert archive.wait()==0
 subprocess.run(['git','init','-q',str(base)],check=True);subprocess.run(['git','-C',str(base),'add','lib/usb/usbdevice.cpp'],check=True)
 scripts=re.findall(r'python3 "\$root/(scripts/patch-circle[^" ]+\.py)"', (root/'scripts/build-firmware.sh').read_text())
 for script in scripts:
  subprocess.run(['python3',str(root/script),str(base)],check=True)
  if script.endswith('request-reuse.py'):break
 paths=['include/circle/usb/usbrequest.h','lib/usb/usbrequest.cpp','include/circle/usb/usbhiddevice.h','lib/usb/usbhiddevice.cpp','include/circle/usb/usbaudiostreaming.h','lib/usb/usbaudiostreaming.cpp']
 first={p:(base/p).read_bytes() for p in paths};subprocess.run(['python3',str(root/'scripts/patch-circle-request-reuse.py'),str(base)],check=True)
 assert first=={p:(base/p).read_bytes() for p in paths}
 source=(base/'lib/usb/usbaudiostreaming.cpp').read_text();start=source.index('boolean CUSBAudioStreamingDevice::SendChunk (');end=source.index('\nboolean CUSBAudioStreamingDevice::SetMute',start) if '\nboolean CUSBAudioStreamingDevice::SetMute' in source[start:] else source.index('\nboolean CUSBAudioStreamingDevice::SetVolume',start)
 # Only the real two submission bodies and the completion; controls are unrelated.
 tx=source[start:source.index('\nboolean CUSBAudioStreamingDevice::ReceiveChunk',start)]
 start=source.index('boolean CUSBAudioStreamingDevice::ReceiveChunk');rx=source[start:source.index('\nboolean ',start+10)]
 start=source.index('void CUSBAudioStreamingDevice::CompletionHandler');completion=source[start:source.index('\nvoid CUSBAudioStreamingDevice::SyncCompletionHandler',start)]
 assert all('new ' not in body and 'delete ' not in body for body in [tx,rx,completion])
 hid=(base/'lib/usb/usbhiddevice.cpp').read_text();start=hid.index('boolean CUSBHIDDevice::StartRequest');end=hid.index('\nvoid CUSBHIDDevice::CompletionStub',start)
 assert 'new ' not in hid[start:end] and 'delete ' not in hid[start:end]
 reuse=(base/'lib/usb/usbrequest.cpp').read_text();reuse=reuse[reuse.index('void CUSBRequest::Reuse'):reuse.index('CUSBRequest::~CUSBRequest')]
 code=r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include "usb_request_pool.h"
using boolean=bool;using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;constexpr bool TRUE=true,FALSE=false;constexpr unsigned USBErrorUnknown=99;enum{USBSpeedFull=0};
struct CUSBEndpoint{};struct Host;
struct CUSBRequest {
 void *m_pBuffer=nullptr,*m_pSetupData=nullptr,*m_pCompletionParam=nullptr,*m_pCompletionContext=nullptr;
 u32 m_nBufLen=0,m_nResultLen=0,m_USBError=7,m_nNumIsoPackets=5;int m_bStatus=1;bool m_bCompleteOnNAK=true,m_bCompleteImmediately=true;
 using Fn=void(*)(CUSBRequest*,void*,void*);Fn m_pCompletionRoutine=nullptr;
 static inline unsigned allocations=0,destructions=0;
 CUSBRequest(CUSBEndpoint*,void*,unsigned){++allocations;}~CUSBRequest(){++destructions;}
 void Reuse(void*,u32);void AddIsoPacket(u16){++m_nNumIsoPackets;}
 void SetCompletionRoutine(Fn f,void* p,void* c){m_pCompletionRoutine=f;m_pCompletionParam=p;m_pCompletionContext=c;}
 bool GetStatus(){return m_bStatus;}unsigned GetResultLength(){return m_nResultLen;}
 void finish(bool ok,unsigned n){m_bStatus=ok;m_nResultLen=n;m_pCompletionRoutine(this,m_pCompletionParam,m_pCompletionContext);}
};
struct Host {bool accept=true;std::vector<CUSBRequest*> queued;bool SubmitAsyncRequest(CUSBRequest* r){if(!accept)return false;queued.push_back(r);return true;}};
struct Device{unsigned GetSpeed(){return 1;}};
struct CUSBAudioStreamingDevice {
 using TCompletionRoutine=void(unsigned,void*);struct TDataRequest {CUSBRequest* request=nullptr;TCompletionRoutine* routine=nullptr;void* param=nullptr;};
 groove::UsbRequestPool<TDataRequest,8> m_Requests;CUSBEndpoint endpoint,*m_pEndpointData=&endpoint,*m_pEndpointSync=nullptr;CUSBRequest* m_pSyncRequest=nullptr;
 bool m_bSynchronousSync=false,m_bDeferredImplicit=true,m_bSinglePacketImplicit=true,m_bSyncEPActive=false;unsigned m_nPacketsPerChunk=1,m_SyncEPBuffer=0;u16 m_usPacketSizeBytes[32]{};Host host;Device device;
 CUSBAudioStreamingDevice(){for(unsigned i=0;i<8;++i)m_Requests.at(i).request=new CUSBRequest(m_pEndpointData,nullptr,0);}
 ~CUSBAudioStreamingDevice(){assert(!m_Requests.busy());for(unsigned i=0;i<8;++i)delete m_Requests.at(i).request;}
 Host* GetHost(){return &host;}Device* GetDevice(){return &device;}bool UseSynchronousChunks(){return false;}void UpdateChunkSize(){}
 bool SendChunk(const void*,unsigned,TCompletionRoutine*,void*);bool ReceiveChunk(void*,unsigned,TCompletionRoutine*,void*);
 static void CompletionHandler(CUSBRequest*,void*,void*);static void SyncCompletionHandler(CUSBRequest*,void*,void*){}
};
'''+reuse+tx+rx+completion+r'''
struct Context{CUSBAudioStreamingDevice* device;unsigned received=0,calls=0;bool reenter=false;u8 buffer[72]{};};
void client(unsigned n,void* p){auto& c=*static_cast<Context*>(p);c.received=n;++c.calls;if(c.reenter){c.reenter=false;assert(c.device->ReceiveChunk(c.buffer,72,client,&c));}}
int main(){CUSBAudioStreamingDevice device;Context c{&device};unsigned allocations=CUSBRequest::allocations;
 for(unsigned i=0;i<8;++i)assert(device.ReceiveChunk(c.buffer,72,client,&c));assert(!device.ReceiveChunk(c.buffer,72,client,&c));
 auto* first=device.host.queued[0];assert(first->m_nNumIsoPackets==1 && !first->m_bCompleteOnNAK && !first->m_bCompleteImmediately);
 c.reenter=true;first->finish(true,0);assert(c.calls==1 && c.received==0 && device.host.queued.back()==first);
 for(unsigned i=1;i<8;++i)device.host.queued[i]->finish(true,72);device.host.queued.back()->finish(false,0);assert(c.calls==9 && c.received==~0u && !device.m_Requests.busy());
 device.host.accept=false;assert(!device.SendChunk(c.buffer,72,client,&c));assert(!device.m_Requests.busy());assert(!device.ReceiveChunk(c.buffer,72,client,&c));assert(!device.m_Requests.busy());
 device.host.accept=true;assert(device.SendChunk(c.buffer,72,client,&c));device.host.queued.back()->finish(true,72);assert(c.received==72 && CUSBRequest::allocations==allocations && CUSBRequest::destructions==0);
}
'''
 p=base/'reuse.cpp';p.write_text(code);exe=base/'reuse'
 subprocess.run(['c++','-std=c++17','-DRASPPI=4','-O1','-g','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'src'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Pinned Circle patches are reproducible/idempotent; real request reset, eight-slot audio, reentry, zero capture and rejected ownership passed')
