#include "display_rectangle.h"
#include "display_frame_gate.h"
#include "display_job_slot.h"
#include <cstring>
#include <cassert>
#include <memory>
#include <initializer_list>
int main(){
 auto pixels=std::make_unique<uint32_t[]>(480*272);auto shadow=std::make_unique<uint16_t[]>(480*272);auto packet=std::make_unique<uint8_t[]>(480*64*2+28);bool valid[17];for(bool& v:valid)v=true;
 auto a=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get());assert(a.valid && !a.dirty && !a.last);
 pixels[5*480+7]=0xff0000;pixels[20*480+40]=0x00ff00;
 a=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get());assert(a.valid && a.dirty && a.first==0 && a.y==0 && a.rows==16 && a.left==4 && a.width==4 && a.bytes==20+4*16*2+8);assert(!a.last);assert(packet[0]==0x84 && packet[2]==0);
 // A nearby change with the same aligned columns is cheaper to combine.
 pixels[20*480+40]=0;pixels[20*480+7]=0x00ff00;
 a=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get());assert(a.rows==32 && a.width==4 && a.last);
 pixels[20*480+7]=0;
 a=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get());assert(a.rows==16 && a.bytes==156 && a.last);
 valid[16]=false;a=groove::prepare_display_rectangle(1,pixels.get(),shadow.get(),valid,16,groove::ui::PixelOrder::BGR,packet.get());assert(a.valid && a.dirty && a.first==16 && a.y==256 && a.rows==16 && a.left==0 && a.width==480 && packet[2]==1);
 assert(!a.last); // Cursor wrapping must see the outstanding first-band pixel.
 pixels[5*480+7]=0;
 a=groove::prepare_display_rectangle(1,pixels.get(),shadow.get(),valid,16,groove::ui::PixelOrder::BGR,packet.get());assert(a.last);
 for(bool& v:valid)v=false;
 a=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get(),64);assert(a.rows==64 && a.bytes==480*64*2+28 && !a.last);
 // Reconstruct a complete device image from acknowledged packets. last must
 // mean exact snapshot equality, including cursor wrap and invalid bands.
 for(unsigned max_rows:{32u,64u,256u})for(auto order:{groove::ui::PixelOrder::RGB,groove::ui::PixelOrder::BGR})for(bool compact:{false,true})for(unsigned pattern=0;pattern<4;++pattern){
  uint32_t rng=1;for(unsigned i=0;i<480*272;++i){rng=rng*1664525u+1013904223u;pixels[i]=pattern==0?rng&0xffffff:pattern==1?0x234567:pattern==2?(i%2?0xabcdef:0x123456):(i%480<120 || i%23==0?0x778899:rng&0xffffff);shadow[i]=0;}
  for(bool& v:valid)v=false;
  groove::DisplayJobSlot slot;auto cached_packet=std::make_unique<uint8_t[]>(groove::kDisplayPacketCapacity);
  unsigned cursor=13,packets=0,acknowledged=0,ack_first=0,ack_rows=0;
  for(;;){
   auto r=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,cursor,order,packet.get(),max_rows,compact);
   groove::DisplayJob job;job.pixels=pixels.get();job.shadow=shadow.get();job.packet=cached_packet.get();job.cursor=cursor;job.max_rows=max_rows;job.compact=compact;job.order=order;job.epoch=3;job.frame=19;job.acknowledged=acknowledged;job.ack_first=ack_first;job.ack_rows=ack_rows;for(unsigned band=0;band<17;++band)job.valid[band]=valid[band];
   auto check_cached=[&](){assert(slot.submit(job));assert(slot.process_one());groove::DisplayJob done;assert(slot.collect(done));const auto& c=done.result;assert(c.valid==r.valid && c.dirty==r.dirty && c.last==r.last && c.first==r.first && c.rows==r.rows && c.left==r.left && c.width==r.width && c.bytes==r.bytes);assert(!std::memcmp(packet.get(),cached_packet.get(),r.bytes));};
   check_cached();check_cached(); // A refused/failed submission has no ACK; resend exactly the same rectangle.
   assert(r.valid && r.dirty && r.bytes<=groove::kDisplayPacketCapacity);assert(++packets<=17);
   unsigned at=16,pixel=0;
   while(pixel<r.rows*r.width){assert(at+4<=r.bytes-8);unsigned op=packet[at],count=(unsigned(packet[at+1])<<16)|(unsigned(packet[at+2])<<8)|packet[at+3];at+=4;assert((op==0 || op==1) && count && pixel+count*2<=r.rows*r.width);assert(at+(op==1?4:count*4)<=r.bytes-8);
    for(unsigned n=0;n<count;++n){unsigned data=op==1?at:at+n*4;for(unsigned j=0;j<2;++j){shadow[(r.y+pixel/r.width)*480+r.left+pixel%r.width]=uint16_t(unsigned(packet[data+j*2])<<8|packet[data+j*2+1]);++pixel;}}at+=op==1?4:count*4;
   }
   assert(at+8==r.bytes && packet[at]==3 && packet[at+4]==0x40);
   if(compact && pattern>0)assert(r.bytes<20+r.width*r.rows*2+8);
   for(unsigned band=r.first;band<r.first+r.rows/16;++band)valid[band]=true;
   bool equal=true;for(unsigned i=0;i<480*272;++i)if(!valid[i/(480*16)] || shadow[i]!=groove::ui::pack565(pixels[i],order)){equal=false;break;}
   assert(r.last==equal);cursor=(r.first+r.rows/16)%17;
   ++acknowledged;ack_first=r.first;ack_rows=r.rows;
   if(r.last){job.acknowledged=acknowledged;job.ack_first=ack_first;job.ack_rows=ack_rows;assert(slot.submit(job));assert(slot.process_one());groove::DisplayJob done;assert(slot.collect(done));assert(done.result.valid && !done.result.dirty);
    // A fresh frame and epoch must rescan even when buffer pointers are reused.
    for(unsigned band=0;band<17;++band)job.valid[band]=valid[band];
    pixels[0]^=0x00ffffff;job.frame++;assert(slot.submit(job));assert(slot.process_one());assert(slot.collect(done));assert(done.result.valid && done.result.dirty);pixels[0]^=0x00ffffff;job.epoch++;assert(slot.submit(job));assert(slot.process_one());assert(slot.collect(done));assert(done.result.valid && !done.result.dirty);
    auto clean=groove::prepare_display_rectangle(0,pixels.get(),shadow.get(),valid,cursor,order,packet.get(),max_rows,compact);assert(clean.valid && !clean.dirty);break;}
  }
 }
 groove::UiFrameClock clock;unsigned frames=0;for(uint64_t us=0;us<10000000;us+=100)frames+=clock.due(us);assert(frames==300);assert(clock.due(12000000));assert(!clock.due(12000001));
 groove::UiFrameClock paced;assert(paced.due(0,true,1));assert(!paced.due(1000,false,2));assert(paced.due(2000,true,2));assert(!paced.due(2001,true,2));assert(!paced.due(33334,false,3));assert(!paced.due(65000,false,3));assert(paced.due(65000,true,3));assert(!paced.due(65001,true,3));assert(paced.due(66667,true,3));assert(!paced.due(400000,false,4));assert(paced.due(400000,true,4));assert(!paced.due(400001,true,4));
 groove::DisplayFrameStats stats;stats.reset(1);stats.completed(0,33334);stats.completed(1,34000);stats.completed(0,150000);assert(stats.complete[0]==2 && stats.complete[1]==1 && stats.over100ms[0]==1 && stats.over100ms[1]==0);assert(stats.max_gap_us[0]==116666);stats.reset(200000);assert(!stats.complete[0]);
 assert(!groove::prepare_display_rectangle(2,pixels.get(),shadow.get(),valid,0,groove::ui::PixelOrder::RGB,packet.get()).valid);
}
