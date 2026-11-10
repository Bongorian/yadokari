#!/usr/bin/env python3
"""Exercise the actual CircleDisplay class with deterministic USB/clock stand-ins."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
out=root/'build/display-snapshot-test'
out.mkdir(parents=True,exist_ok=True)
header=(root/'firmware/circle_display.h').read_text()
body='\n'.join(line for line in header.splitlines() if not line.startswith('#include') and line!='#pragma once')
prefix=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <cstdio>
#include "backends.h"
#include "display_frame_gate.h"
#include "display_job_slot.h"
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;
constexpr unsigned PAGE_SIZE=65536;
void* palloc(){return malloc(PAGE_SIZE);}void pfree(void* p){free(p);}
struct CTimer {inline static uint64_t now=0;static uint32_t GetClockTicks(){return uint32_t(now);}static uint64_t GetClockTicks64(){return now;}};
struct Mk3DisplayTransfer;
struct Mk3DisplayOwner {int* endpoint=nullptr;Mk3DisplayTransfer* active=nullptr;bool detached=false;};
struct Mk3DisplayTransfer {unsigned state=0,expected=0,completed_at=0;Mk3DisplayOwner* owner=nullptr;};
struct CMk3Display {
 Mk3DisplayTransfer* transfer=nullptr;const u8* bytes=nullptr;uint64_t due=0;bool fail=false;
 u16 shown[2][480*272]{};
 bool OutputReady(){return true;}void FenceOutput(){}
 bool Submit(const void* p,unsigned size,Mk3DisplayTransfer* t){assert(!transfer);transfer=t;bytes=static_cast<const u8*>(p);t->expected=size;due=CTimer::now+12000;return true;}
 unsigned be(unsigned at){return unsigned(bytes[at])*256+bytes[at+1];}
 void poll(){if(!transfer || CTimer::now<due)return;if(!fail){unsigned side=bytes[2],x=be(8),y=be(10),w=be(12),h=be(14),at=16,pixel=0;
   while(pixel<w*h){assert(at+4<=transfer->expected-8);unsigned op=bytes[at],count=(unsigned(bytes[at+1])<<16)|be(at+2);at+=4;assert((op==0 || op==1) && count && pixel+count*2<=w*h);assert(at+(op==1?4:count*4)<=transfer->expected-8);
    for(unsigned n=0;n<count;++n){unsigned data=op==1?at:at+n*4;for(unsigned j=0;j<2;++j){shown[side][(y+pixel/w)*480+x+pixel%w]=uint16_t(be(data+j*2));++pixel;}}at+=op==1?4:count*4;}
   assert(at+8==transfer->expected && bytes[at]==3 && bytes[at+4]==0x40);}
  transfer->completed_at=CTimer::GetClockTicks();transfer->state=fail?2:1;transfer=nullptr;}
};
'''
main=r'''
int main(){
 auto device=std::make_unique<CMk3Display>();groove::DisplayJobSlot worker;
 auto pixels=std::make_unique<uint32_t[]>(480*272);
 CircleDisplay display(static_cast<u8*>(palloc()));assert(display.UseWorker(&worker));display.Bind(device.get());display.ResetFrameStats(1);
 unsigned prior[2]{},verified=0,slot=~0u;
 for(unsigned tick=1;tick<=650;++tick){
  CTimer::now=tick*1000;
  // At 12ms/rectangle, two complete dirty screens span a 30Hz slot.
  // Keep changing the latest renderer while its previous snapshot is active.
  unsigned next=tick/33;
  if(tick<450 && next!=slot){slot=next;for(unsigned i=0;i<480*272;++i)pixels[i]=0x202020u+(slot%20)*0x080808u;display.NewFrame();}
  if(tick==200)display.Invalidate();
  device->fail=tick>=300 && tick<320;
  device->poll();worker.process_one();
  display.present(tick%2,pixels.get(),480,272);
  const auto& stats=display.FrameStats();
  for(unsigned side=0;side<2;++side)if(stats.complete[side]!=prior[side]){
   assert(!device->fail);assert(stats.complete[side]==prior[side]+1);prior[side]=stats.complete[side];
   // A completion must represent a whole immutable frame, not mixed strips
   // from several newer renderer images or an unacknowledged transfer.
   u16 color=device->shown[side][0];assert(color);
   for(unsigned i=1;i<480*272;++i)assert(device->shown[side][i]==color);
   ++verified;
  }
 }
 assert(verified>=10 && prior[0]>=4 && prior[1]>=4);assert(display.FrameComplete());
 printf("snapshot completion verified %u, left %u right %u; slot crossing, invalidation and failed ACK passed\n",verified,prior[0],prior[1]);
}
'''
source=out/'test.cpp';source.write_text(prefix+body+main)
subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'src'),str(source),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
