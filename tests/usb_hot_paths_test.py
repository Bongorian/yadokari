"""Run production USB state/callback bodies with an asynchronous host stub."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'firmware/mk3_usb.cpp').read_text();header=(root/'firmware/mk3_usb.h').read_text()
common=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdlib>
#include "latest_led_state.h"
#include "usb_request_pool.h"
#include "sync_output_guard.h"
#include "output_retry.h"
#include "display_wire_facts.h"
#include "usb_progress.h"
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using uint8_t=std::uint8_t;using uint64_t=std::uint64_t;
constexpr int FALSE=0,LogWarning=1,LogNotice=2;const char* kHid="hid";const char* kDisplay="display";
static unsigned request_new=0,request_delete=0,endpoint_delete=0,page_delete=0;
struct CTimer {static inline unsigned ticks=0;static unsigned GetClockTicks(){return ticks;}static uint64_t GetClockTicks64(){return ticks;}};
struct CUSBEndpoint{~CUSBEndpoint(){++endpoint_delete;}};
struct CUSBRequest {
 using Callback=void(*)(CUSBRequest*,void*,void*);
 void* buffer=nullptr;unsigned size=0,result=0;bool ok=true;Callback callback=nullptr;void* param=nullptr;
 CUSBRequest(CUSBEndpoint*,void* p,unsigned n):buffer(p),size(n){++request_new;}
 ~CUSBRequest(){++request_delete;}
 void Reuse(void* p,unsigned n){buffer=p;size=n;result=0;ok=true;callback=nullptr;param=nullptr;}
 void SetCompletionRoutine(Callback c,void* p,void*){callback=c;param=p;}
 bool GetStatus(){return ok;}unsigned GetResultLength(){return result;}
 void finish(bool success=true,bool short_write=false){ok=success;result=size-(short_write?1:0);callback(this,param,nullptr);}
};
struct Host {std::vector<CUSBRequest*> queued;unsigned reject_at=99,calls=0;
 bool SubmitAsyncRequest(CUSBRequest* r){if(calls++==reject_at)return false;queued.push_back(r);return true;}
};
struct CDeviceNameService{static CDeviceNameService* Get(){static CDeviceNameService x;return &x;}void RemoveDevice(const char*,int){}};
struct CLogger{static CLogger* Get(){static CLogger x;return &x;}template<class... T>void Write(const char*,int,const char*,T...){}};
void pfree(void* p){++page_delete;std::free(p);}
'''
# Compile the actual transfer structs and Submit/Complete/Release/destructor.
structs=header[header.index('struct Mk3DisplayOwner;'):header.index('class CMk3Display :')]
helpers=source[source.index('static void FreeDisplayOwner'):source.index('boolean CMk3Display::Configure()')]
methods=source[source.index('void CMk3Display::Completed('):source.index('extern "C" CUSBFunction*')]
stub=r'''
struct CMk3Display{
 bool m_Registered=false,m_AsyncFenced=false;CUSBEndpoint* m_Out=nullptr;Mk3DisplayOwner* m_Lifetime=nullptr;Host host;
 CMk3Display(){m_Out=new CUSBEndpoint;m_Lifetime=new Mk3DisplayOwner{};m_Lifetime->endpoint=m_Out;for(auto& c:m_Lifetime->requests){c.request=new CUSBRequest(m_Out,nullptr,0);}}
 ~CMk3Display();Host* GetHost(){return &host;}bool OutputReady()const{return !m_AsyncFenced;}void FenceOutput(){m_AsyncFenced=true;}
 static void Completed(CUSBRequest*,void*,void*);bool Submit(const void*,unsigned,Mk3DisplayTransfer*);
};
'''
main=r'''
int main(){
 std::vector<u8> bytes(61468),other(4096,42);for(unsigned i=0;i<bytes.size();++i)bytes[i]=u8(i);
 auto* d=new CMk3Display;auto* left=new Mk3DisplayTransfer;auto* right=new Mk3DisplayTransfer;Mk3DisplayTransfer third;unsigned allocated=request_new;
 groove::display_usb_progress.enabled(true);CTimer::ticks=100;assert(d->Submit(bytes.data(),bytes.size(),left));assert(d->Submit(other.data(),other.size(),right));assert(!d->Submit(bytes.data(),3,&third));assert(request_new==allocated);
 assert(d->host.queued[0]->buffer==bytes.data() && d->host.queued[0]->size==bytes.size());assert(d->host.queued[1]->buffer==other.data());
 auto* first=d->host.queued[0];auto* last=d->host.queued[1];groove::display_usb_progress.audio(200);groove::display_usb_progress.audio(300);CTimer::ticks=400;first->finish();assert(left->audio_callbacks==2 && left->audio_gap==100);assert(left->state==1 && right->state==0);Mk3DisplayRelease(left);
 delete d;assert(request_delete==0 && endpoint_delete==0);CTimer::ticks=500;last->finish();assert(right->audio_callbacks==2 && right->audio_gap==200);assert(right->state==1 && request_new==allocated && request_delete==0);Mk3DisplayRelease(right);assert(request_delete==2 && endpoint_delete==1);
 {CMk3Display normal;auto* p=new Mk3DisplayTransfer;assert(normal.Submit(bytes.data(),700,p));normal.host.queued.back()->finish(true,true);assert(p->state==2);Mk3DisplayRelease(p);}
 {CMk3Display reject;reject.host.reject_at=0;Mk3DisplayTransfer p;assert(!reject.Submit(bytes.data(),bytes.size(),&p));assert(!reject.m_Lifetime->busy());}
 groove::LatestLedState latest;u8 a[2]={1,2},b[2]={3,4},packet[81]{};unsigned kind=0,length=0;
 assert(latest.update(0,a,2) && latest.prepare(packet,kind,length));latest.complete(true);
 assert(latest.update(0,b,2) && latest.prepare(packet,kind,length));assert(latest.update(0,a,2));latest.complete(false);assert(latest.prepare(packet,kind,length) && packet[0]==1);latest.complete(true);assert(!latest.pending());
 struct Item {unsigned value=0;};groove::UsbRequestPool<Item,8> pool;Item* items[8];for(auto& p:items){p=pool.acquire();assert(p);}assert(!pool.acquire());assert(pool.release(items[3]));assert(!pool.release(items[3]));assert(pool.acquire()==items[3]);for(auto* p:items)assert(pool.release(p));assert(!pool.busy());
}
'''
# Exercise real LED output callbacks and task service: acceptance, latest-only,
# failure/short write, no resend, immutable in-flight page, timeout quarantine.
led_owner=source[source.index('struct Mk3LedOwner {'):source.index('bool CMk3Hid::RestartInput')]
led=source[source.index('bool CMk3Hid::set_pad_strip_leds'):source.index('static void FreeDisplayOwner')]
ledstub=r'''
struct Mk3LedOwner;
struct CMk3Hid {
 Mk3LedOwner* m_Output=nullptr;bool m_Registered=false;u8* m_LedBuffer=nullptr;unsigned m_PadReportBytes=81,m_ButtonReportBytes=63,m_LedStarted=0;
 groove::SyncOutputGuard m_OutputGuard;groove::LatestLedState m_Latest;groove::OutputRetry m_LedRetry;Host host;
 ~CMk3Hid();Host* GetHost(){return &host;}void ServiceOutput();static void LedCompleted(CUSBRequest*,void*,void*);
 bool set_pad_strip_leds(const uint8_t*,const uint8_t*);bool set_button_leds(const uint8_t*);
};
'''
ledmain=r'''
void attach(CMk3Hid& h){auto* o=new Mk3LedOwner;o->endpoint=new CUSBEndpoint;o->packet=static_cast<u8*>(std::malloc(65536));o->request=new CUSBRequest(o->endpoint,nullptr,0);h.m_Output=o;}
int main(){
 u8 pads[16]{},strip[25]{},buttons[62]{};unsigned after=0;
 {CMk3Hid h;attach(h);assert(h.set_pad_strip_leds(pads,strip));h.ServiceOutput();assert(h.host.queued.size()==1);auto* r=h.host.queued.back();std::vector<u8> original(static_cast<u8*>(r->buffer),static_cast<u8*>(r->buffer)+r->size);
  pads[0]=24;assert(h.set_pad_strip_leds(pads,strip));pads[0]=36;assert(h.set_pad_strip_leds(pads,strip));h.ServiceOutput();assert(h.host.queued.size()==1 && !std::memcmp(original.data(),r->buffer,r->size));
  r->finish();h.ServiceOutput();assert(h.host.queued.size()==2);unsigned allocated=request_new;h.host.queued.back()->finish(true,true);h.ServiceOutput();assert(h.host.queued.size()==2);CTimer::ticks+=100001;h.ServiceOutput();assert(h.host.queued.size()==3 && request_new==allocated);
  h.host.queued.back()->finish();h.ServiceOutput();assert(!h.m_Latest.pending());assert(h.set_pad_strip_leds(pads,strip));h.ServiceOutput();assert(h.host.queued.size()==3);
  assert(h.set_button_leds(buttons));h.ServiceOutput();assert(h.host.queued.size()==4);h.host.queued.back()->finish();h.ServiceOutput();after=request_delete;
 }
 assert(request_delete==after+1);
 CUSBRequest* pending=nullptr;after=request_delete;
 {CMk3Hid h;attach(h);assert(h.set_button_leds(buttons));h.ServiceOutput();pending=h.host.queued.back();CTimer::ticks+=3000001;h.ServiceOutput();assert(!h.m_OutputGuard.ready());}
 assert(request_delete==after);pending->finish();assert(request_delete==after);GrooveReapUsbOutput();assert(request_delete==after+1);
}
'''
display=(root/'firmware/circle_display.h').read_text()
display='\n'.join(line for line in display.splitlines() if not line.startswith('#include') and not line.startswith('#pragma'))
pipeline_headers='''
#include "backends.h"
#include "mk3_output.h"
#include "ui/framebuffer565.h"
#include "display_frame_gate.h"
#include "display_job_slot.h"
#include "display_attribution.h"
#include "boot_splash.h"
#include "boot_splash_version.h"
constexpr unsigned PAGE_SIZE=65536;
void* palloc(){return std::aligned_alloc(PAGE_SIZE,PAGE_SIZE);}
'''
pipeline_main=r'''
void drain(CMk3Display& device,unsigned& next){while(next<device.host.queued.size())device.host.queued[next++]->finish();}
int main(){
 std::vector<u32> pixels(480*272,0x2277aa);
 {CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.BootSplash(&device));assert(device.host.queued.size()==2 && !screen.BootSplashComplete());
  for(unsigned side=0;side<2;++side){auto* p=static_cast<u8*>(device.host.queued[side]->buffer);assert(p[2]==side && p[8]==0 && p[9]==0 && p[10]==0 && p[11]==0 && p[12]==1 && p[13]==224 && p[14]==1 && p[15]==16);}
  device.host.queued[0]->finish();assert(!screen.BootSplashComplete());device.host.queued[1]->finish();assert(screen.BootSplashComplete());
  std::vector<u16> shadow(480*272);std::vector<u32> original(480*272);screen.NewFrame();
  for(unsigned side=0;side<2;++side){assert(groove::boot_splash_shadow(side,shadow.data()));for(unsigned i=0;i<shadow.size();++i)original[i]=groove::ui::expand565(shadow[i]);assert(screen.present(side,original.data(),480,272));}assert(device.host.queued.size()==2 && screen.FrameComplete());
  original[100*480+100]=0xff00ff;screen.NewFrame();assert(screen.present(1,original.data(),480,272));assert(device.host.queued.size()==3);auto* p=static_cast<u8*>(device.host.queued.back()->buffer);assert(p[2]==1 && (unsigned(p[12])*256+p[13])<480);device.host.queued.back()->finish();assert(screen.present(1,original.data(),480,272));
 }
 {CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.BootSplash(&device));device.host.queued[0]->finish();device.host.queued[1]->finish(false);assert(!screen.BootSplashComplete());assert(!screen.present(1,pixels.data(),480,272));assert(screen.present(1,pixels.data(),480,272));assert(device.host.queued.size()==3);device.host.queued.back()->finish();}

 {CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));screen.Bind(&device);screen.NewFrame();
  assert(screen.present(0,pixels.data(),480,272));assert(screen.present(1,pixels.data(),480,272));assert(device.host.queued.size()==2);
  auto* l=device.host.queued[0];auto* r=device.host.queued[1];assert(l->buffer!=r->buffer);std::vector<u8> snapshot(static_cast<u8*>(l->buffer),static_cast<u8*>(l->buffer)+l->size);
  pixels[0]=0xffffff;screen.NewFrame();assert(screen.present(0,pixels.data(),480,272));assert(device.host.queued.size()==2 && !std::memcmp(snapshot.data(),l->buffer,l->size));assert(!screen.FrameComplete());
  unsigned next=0;for(unsigned i=0;i<40 && !screen.FrameComplete();++i){drain(device,next);assert(screen.present(0,pixels.data(),480,272));assert(screen.present(1,pixels.data(),480,272));}assert(screen.FrameComplete());assert(screen.FrameStats().complete[0] && screen.FrameStats().complete[1]);drain(device,next);
 }
 {CMk3Display device;groove::DisplayJobSlot worker;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.UseWorker(&worker));screen.Bind(&device);screen.NewFrame();
  assert(screen.present(0,pixels.data(),480,272) && worker.process_one());assert(screen.present(0,pixels.data(),480,272));
  assert(screen.present(1,pixels.data(),480,272) && worker.process_one());assert(screen.present(1,pixels.data(),480,272));assert(device.host.queued.size()==2);
  screen.Invalidate();device.host.queued[0]->finish();assert(screen.present(0,pixels.data(),480,272));assert(worker.process_one());assert(screen.present(0,pixels.data(),480,272));assert(device.host.queued.size()==3 && !screen.FrameComplete());
  device.host.queued[1]->finish();device.host.queued[2]->finish(false);assert(!screen.present(0,pixels.data(),480,272));assert(!screen.FrameComplete());screen.Invalidate();
  unsigned next=3;for(unsigned i=0;i<40 && !screen.FrameComplete();++i){drain(device,next);assert(screen.present(0,pixels.data(),480,272));if(worker.busy())worker.process_one();assert(screen.present(1,pixels.data(),480,272));if(worker.busy())worker.process_one();}assert(screen.FrameComplete());drain(device,next);if(worker.busy())worker.process_one();
 }
 {auto* old=new CMk3Display;CircleDisplay screen(static_cast<u8*>(palloc()));screen.Bind(old);assert(screen.present(0,pixels.data(),480,272));auto* delayed=old->host.queued[0];void* old_page=delayed->buffer;unsigned freed=page_delete;delete old;
  CMk3Display replacement;screen.Bind(&replacement);assert(screen.present(0,pixels.data(),480,272));assert(replacement.host.queued[0]->buffer!=old_page && page_delete==freed);
  delayed->finish();screen.Bind(&replacement);assert(page_delete==freed+1);replacement.host.queued[0]->finish();
 }
 {CMk3Display device;CircleDisplay screen(static_cast<u8*>(palloc()));screen.Bind(&device);assert(screen.present(0,pixels.data(),480,272));assert(screen.present(1,pixels.data(),480,272));CTimer::ticks+=3000001;assert(!screen.present(0,pixels.data(),480,272) && !device.OutputReady());unsigned next=0;drain(device,next);screen.Bind(nullptr);}
 {CMk3Display devices[10];CircleDisplay bounded(static_cast<u8*>(palloc()));for(unsigned i=0;i<9;++i){bounded.Bind(&devices[i]);assert(bounded.present(0,pixels.data(),480,272));assert(devices[i].host.queued.size()==1);bounded.Bind(nullptr);}bounded.Bind(&devices[9]);assert(bounded.present(0,pixels.data(),480,272) && devices[9].host.queued.empty());devices[0].host.queued[0]->finish();bounded.Bind(&devices[9]);devices[8].host.queued[0]->finish();assert(bounded.present(0,pixels.data(),480,272) && devices[9].host.queued.size()==1);for(unsigned i=1;i<8;++i)devices[i].host.queued[0]->finish();bounded.Bind(&devices[9]);devices[9].host.queued[0]->finish();}

}
'''
# Real pad report encoder has the exact 42/81-byte format.
ledcommon=common+'\n#include "mk3_output.h"\n'
with tempfile.TemporaryDirectory() as directory:
 directory=pathlib.Path(directory)
 for name,code in [('bulk',common+structs+stub+helpers+methods+main),('led',ledcommon+ledstub+led_owner+led+ledmain),('pipeline',common+pipeline_headers+structs+stub+helpers+methods+display+pipeline_main)]:
  p=directory/(name+'.cpp');p.write_text(code);exe=directory/name
  subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('USB hot paths: production callbacks preserve ordered bytes, full ACK, fixed requests, latest LEDs and detached DMA')
