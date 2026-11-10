"""Actual CircleDisplay: bounded lookahead versus serial encoder/ACK reference."""
import pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
# Reuse only declarations from the existing production-USB fixture; do not run it.
script=(root/'tests/usb_hot_paths_test.py').read_text()
ns={'__file__':str(root/'tests/usb_hot_paths_test.py')}
exec(compile(script[:script.index('with tempfile.TemporaryDirectory() as directory:')],str(root/'tests/usb_hot_paths_test.py'),'exec'),ns)
common=ns['common'].replace('std::vector<CUSBRequest*> queued;', 'std::vector<CUSBRequest*> queued;std::vector<std::vector<u8>> wire;').replace('queued.push_back(r);return true;', 'queued.push_back(r);wire.emplace_back(static_cast<u8*>(r->buffer),static_cast<u8*>(r->buffer)+r->size);return true;')
headers=ns['pipeline_headers'].replace('void* palloc(){return std::aligned_alloc(PAGE_SIZE,PAGE_SIZE);}', 'static bool deny_page=false;void* palloc(){return deny_page?nullptr:std::aligned_alloc(PAGE_SIZE,PAGE_SIZE);}')
main=r'''
#include <memory>
#include <cstdio>
using Packet=std::vector<u8>;
struct Image {u16 pixels[2][480*272]{};};
static void decode(Image& shown,const Packet& bytes){unsigned side=bytes[2],x=(bytes[8]<<8)|bytes[9],y=(bytes[10]<<8)|bytes[11],w=(bytes[12]<<8)|bytes[13],h=(bytes[14]<<8)|bytes[15],at=16,pixel=0;assert(side<2 && x+w<=480 && y+h<=272);
 while(pixel<w*h){unsigned op=bytes[at],count=(unsigned(bytes[at+1])<<16)|(unsigned(bytes[at+2])<<8)|bytes[at+3];at+=4;assert(op<=1 && count && pixel+2*count<=w*h);for(unsigned n=0;n<count;++n){unsigned data=op==1?at:at+n*4;for(unsigned j=0;j<2;++j){shown.pixels[side][(y+pixel/w)*480+x+pixel%w]=u16((bytes[data+j*2]<<8)|bytes[data+j*2+1]);++pixel;}}at+=op==1?4:count*4;}assert(at+8==bytes.size() && bytes[at]==3 && bytes[at+4]==0x40);
}
static void reference(const std::vector<u32>& pixels,unsigned side,groove::ui::PixelOrder order,std::vector<Packet>& expected){
 groove::DisplayJobSlot worker;assert(worker.enable_coalescing());std::vector<u16> shadow(480*272);std::vector<u8> page(PAGE_SIZE);bool valid[17]{};unsigned cursor=0,acked=0,first=0,rows=0;
 for(unsigned n=0;n<20;++n){groove::DisplayJob j;j.pixels=pixels.data();j.shadow=shadow.data();j.packet=page.data();j.side=side;j.max_rows=272;j.compact=j.coalesce=true;j.order=order;j.epoch=j.frame=1;j.cursor=cursor;j.acknowledged=acked;j.ack_first=first;j.ack_rows=rows;for(unsigned b=0;b<17;++b)j.valid[b]=valid[b];assert(worker.submit(j) && worker.process_one() && worker.collect(j));assert(j.result.valid && j.result.dirty);auto& r=j.result;expected.emplace_back(page.begin(),page.begin()+r.bytes);for(unsigned y=0;y<r.rows;++y)for(unsigned x=0;x<r.width;++x)shadow[(r.y+y)*480+r.left+x]=groove::ui::pack565(pixels[(r.y+y)*480+r.left+x],order);for(unsigned b=r.first;b<r.first+r.rows/16;++b)valid[b]=true;cursor=(r.first+r.rows/16)%17;first=r.first;rows=r.rows;++acked;if(r.last)return;}assert(false);
}
static unsigned run(unsigned pattern,groove::ui::PixelOrder order,unsigned failure,bool no_page){
 CMk3Display device;groove::DisplayJobSlot worker;assert(worker.enable_coalescing());CircleDisplay screen(static_cast<u8*>(palloc()));deny_page=no_page;assert(screen.UseWorker(&worker));deny_page=false;screen.Bind(&device);screen.SetPixelOrder(order);screen.NewFrame();screen.ResetFrameStats(CTimer::ticks);
 std::vector<u32> pixels[2];uint32_t rng=1;for(unsigned side=0;side<2;++side){pixels[side].resize(480*272);for(unsigned i=0;i<pixels[side].size();++i){rng=rng*1664525u+1013904223u;pixels[side][i]=pattern==0?0x2277aa:pattern==1?rng&0xffffff:pattern==2?(i%2?0xabccdf:0x123456):(i%480<120 || i%23==0?0x778899:rng&0xffffff);}}
 std::vector<Packet> expected[2];for(unsigned side=0;side<2;++side)reference(pixels[side],side,order,expected[side]);
 auto shown=std::make_unique<Image>();unsigned next=0,ahead=0,errors=0;bool invalidated=false;deny_page=false;
 for(unsigned step=0;step<10000 && !screen.FrameComplete();++step){CTimer::ticks+=100;
  if(next<device.host.queued.size()){
   auto* r=device.host.queued[next];assert(r->size==device.host.wire[next].size() && !std::memcmp(r->buffer,device.host.wire[next].data(),r->size));
   if(step%80==79){bool fail=failure==1 && next==0;if(!fail)decode(*shown,device.host.wire[next]);r->finish(!fail);++next;}
  }
  screen.Poll();
  if(failure==2 && !invalidated && next==0 && device.host.queued.size()==2 && step>10){screen.Invalidate();invalidated=true;}
  if(failure==3 && device.host.calls==2)device.host.reject_at=2;
  assert(device.host.queued.size()-next<=2);
  for(unsigned side=0;side<2;++side){bool ok=screen.present(side,pixels[side].data(),480,272);errors+=!ok;if(worker.busy()){bool two_pending=device.host.queued.size()-next==2;if(worker.process_one() && two_pending)++ahead;}}
 }
 deny_page=false;assert(screen.FrameComplete() && next==device.host.queued.size());assert(screen.FrameStats().complete[0]==1 && screen.FrameStats().complete[1]==1 && !screen.FrameStats().discarded[0] && !screen.FrameStats().discarded[1]);
 for(unsigned side=0;side<2;++side)for(unsigned i=0;i<pixels[side].size();++i)assert(shown->pixels[side][i]==groove::ui::pack565(pixels[side][i],order));
 if(!failure){std::vector<Packet> actual[2];for(auto& bytes:device.host.wire)actual[bytes[2]].push_back(bytes);assert(expected[0]==actual[0] && expected[1]==actual[1]);}
 if(failure==1 || failure==3)assert(errors);
 return ahead;
}

static void detach_prepared(){
 auto* old=new CMk3Display;groove::DisplayJobSlot worker;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.UseWorker(&worker));screen.Bind(old);screen.NewFrame();std::vector<u32> pixels(480*272);uint32_t rng=19;for(auto& p:pixels){rng=rng*1664525u+1013904223u;p=rng&0xffffff;}
 for(unsigned n=0;n<10;++n){for(unsigned side=0;side<2;++side){assert(screen.present(side,pixels.data(),480,272));worker.process_one();}}
 assert(old->host.queued.size()==2);
 auto* left=old->host.queued[0];auto* right=old->host.queued[1];void* left_page=left->buffer;void* right_page=right->buffer;unsigned freed=page_delete;delete old;
 CMk3Display next;screen.Bind(&next);unsigned consumed=0;auto shown=std::make_unique<Image>();
 for(unsigned step=0;step<1000 && !screen.FrameComplete();++step){CTimer::ticks+=100;while(consumed<next.host.queued.size()){assert(next.host.queued[consumed]->buffer!=left_page && next.host.queued[consumed]->buffer!=right_page);decode(*shown,next.host.wire[consumed]);next.host.queued[consumed++]->finish();}screen.Poll();for(unsigned side=0;side<2;++side){assert(screen.present(side,pixels.data(),480,272));worker.process_one();}}
 assert(screen.FrameComplete() && page_delete==freed);for(unsigned side=0;side<2;++side)for(unsigned i=0;i<pixels.size();++i)assert(shown->pixels[side][i]==groove::ui::pack565(pixels[i],groove::ui::PixelOrder::RGB));left->finish();right->finish();screen.Poll();assert(page_delete==freed+2);
}

static void scheduled_release(){
 CMk3Display device;groove::DisplayJobSlot worker;CircleDisplay screen(static_cast<u8*>(palloc()));assert(screen.UseWorker(&worker));screen.Bind(&device);std::vector<u32> pixels(480*272,0x2277aa);unsigned next=0;auto shown=std::make_unique<Image>();
 for(unsigned frame=0;frame<2;++frame){uint64_t release=uint64_t(CTimer::ticks)+8000;unsigned packets_before=device.host.queued.size();screen.NewFrame(release);unsigned before=screen.FrameStats().complete[0];
  for(unsigned n=0;n<79;++n){CTimer::ticks+=100;screen.Poll();for(unsigned side=0;side<2;++side){assert(screen.present(side,pixels.data(),480,272));worker.process_one();}assert(device.host.queued.size()==packets_before && screen.FrameStats().complete[0]==before && !screen.FrameComplete());}
  for(unsigned n=0;n<100 && !screen.FrameComplete();++n){CTimer::ticks+=100;assert(CTimer::ticks>=release);while(next<device.host.queued.size()){decode(*shown,device.host.wire[next]);device.host.queued[next++]->finish();}screen.Poll();for(unsigned side=0;side<2;++side){assert(screen.present(side,pixels.data(),480,272));worker.process_one();}}
  assert(screen.FrameComplete() && screen.FrameStats().complete[0]==before+1);if(frame==1)assert(device.host.queued.size()==packets_before);
 }
 for(unsigned side=0;side<2;++side)for(unsigned i=0;i<pixels.size();++i)assert(shown->pixels[side][i]==groove::ui::pack565(pixels[i],groove::ui::PixelOrder::RGB));
}
int main(){unsigned ahead=0,cases=0;for(auto order:{groove::ui::PixelOrder::RGB,groove::ui::PixelOrder::BGR})for(unsigned pattern=0;pattern<4;++pattern){ahead+=run(pattern,order,0,false);++cases;}
 assert(ahead);detach_prepared();scheduled_release();cases+=2;for(unsigned failure:{1u,2u,3u}){run(1,groove::ui::PixelOrder::RGB,failure,false);++cases;}run(1,groove::ui::PixelOrder::RGB,0,true);++cases;
 std::printf("Display pipeline: %u cases, same FIFO bytes/pixels RGB/BGR, %u preparations with both LCDs in flight, real ACK only, failure/rejection/epoch/optional-page fallback passed\n",cases,ahead);
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=pathlib.Path(directory)/'pipeline.cpp';p.write_text(common+headers+ns['structs']+ns['stub']+ns['helpers']+ns['methods']+ns['display']+main)
 exe=p.with_suffix('');subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)

# Actual normal-mode Ethernet branch, with a diagnostics-only network stand-in.
remote=(root/'firmware/update_runtime.cpp').read_text();begin=remote.index(' if(m_DiagnosticBusy)return false;',remote.index('bool CUpdateRuntime::Remote('));end=remote.index(' if((command==SerialCommand::Efficiency',begin)
code=r'''
#include <cassert>
enum class SerialCommand {Ethernet,Other};
struct CUpdateRuntime {bool m_DiagnosticBusy=false,m_Remote=true,m_Ready=true,m_Identity=true,m_Active=false,m_NetworkWifi=false;void* m_ControlListener=nullptr;bool upload=false;unsigned starts=0;void StartNetwork(bool wifi,bool diagnostics){assert(!wifi && diagnostics);++starts;m_ControlListener=this;}bool Remote(SerialCommand command){
'''+remote[begin:end]+r'''
return false;}};
int main(){CUpdateRuntime r;assert(r.Remote(SerialCommand::Ethernet) && r.starts==1 && !r.upload);assert(r.Remote(SerialCommand::Ethernet) && !r.upload);for(unsigned reason=0;reason<6;++reason){CUpdateRuntime q;if(reason==0)q.m_DiagnosticBusy=true;if(reason==1)q.m_Remote=false;if(reason==2)q.m_Ready=false;if(reason==3)q.m_Identity=false;if(reason==4)q.m_Active=true;if(reason==5)q.m_NetworkWifi=true;assert(!q.Remote(SerialCommand::Ethernet) && !q.starts && !q.upload);}}
'''
with tempfile.TemporaryDirectory() as directory:
 p=pathlib.Path(directory)/'network.cpp';p.write_text(code);exe=p.with_suffix('');subprocess.run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-Wall','-Wextra','-Werror',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('Normal Ethernet diagnostic bootstrap: explicit request, remote/ready/identity/mode gates, no firmware upload listener passed')
