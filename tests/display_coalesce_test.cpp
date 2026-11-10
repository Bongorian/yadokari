#include "display_job_slot.h"
#include "display_wire_facts.h"
#include "display_probe_packet.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
#include <initializer_list>
using namespace groove;
static void decode(const uint8_t* bytes,const PreparedDisplayRect& r,uint16_t* shadow){
 unsigned at=16,pixel=0;
 while(at<r.bytes-8){unsigned op=bytes[at],n=(unsigned(bytes[at+1])<<16)|(unsigned(bytes[at+2])<<8)|bytes[at+3];at+=4;assert(op<=1 && n);
  for(unsigned i=0;i<n;++i)for(unsigned j=0;j<2;++j){unsigned data=op?at:at+i*4;assert(pixel<r.rows*r.width);shadow[(r.y+pixel/r.width)*480+r.left+pixel%r.width]=uint16_t((unsigned(bytes[data+2*j])<<8)|bytes[data+2*j+1]);++pixel;}
  at+=op?4:n*4;
 }
 assert(pixel==r.rows*r.width && at==r.bytes-8);
}
static unsigned render(bool combined,unsigned pattern,ui::PixelOrder order,unsigned side){
 auto pixels=std::make_unique<uint32_t[]>(480*272);auto shadow=std::make_unique<uint16_t[]>(480*272);auto packet=std::make_unique<uint8_t[]>(kDisplayPacketCapacity);auto previous=std::make_unique<uint8_t[]>(kDisplayPacketCapacity);
 uint32_t rng=0x1234;for(unsigned i=0;i<480*272;++i){rng=rng*1664525+1013904223;pixels[i]=pattern==1 || pattern==2?rng&0xffffff:0;shadow[i]=pattern==2?ui::pack565(pixels[i],order):0;}
 if(pattern==0 || pattern==2){pixels[16*480+64]=0xabcdef;pixels[14*16*480+(pattern==2?448:64)]=0xfedcba;}
 DisplayJobSlot slot;allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(!slot.enable_coalescing());allocation_admission_hook=nullptr;assert(slot.enable_coalescing());
 DisplayJob job;job.side=side;job.pixels=pixels.get();job.shadow=shadow.get();job.packet=packet.get();job.compact=true;job.coalesce=combined;job.max_rows=combined?272:256;job.epoch=13;job.frame=41;for(auto& v:job.valid)v=pattern!=1;
 job.order=order;unsigned count=0;
 for(;;){assert(slot.submit(job));assert(slot.process_one());DisplayJob done;assert(slot.collect(done));auto r=done.result;if(pattern==1 || pattern==2)assert(done.coalesce_attempted==0);assert(r.valid && r.dirty && r.bytes<=kDisplayPacketCapacity);auto facts=display_wire_facts(packet.get(),r.bytes);assert(facts.valid && (facts.literal_pairs+facts.repeated_pairs)*2==r.width*r.rows);std::memcpy(previous.get(),packet.get(),r.bytes);
  assert(slot.submit(job));assert(slot.process_one());DisplayJob retry;assert(slot.collect(retry));assert(retry.result.bytes==r.bytes && !std::memcmp(previous.get(),packet.get(),r.bytes));
  decode(packet.get(),r,shadow.get());for(unsigned band=r.first;band<r.first+r.rows/16;++band)job.valid[band]=true;assert(++count<=17);++job.acknowledged;job.ack_first=r.first;job.ack_rows=r.rows;job.cursor=(r.first+r.rows/16)%17;
  if(r.last)break;
 }
 for(unsigned i=0;i<480*272;++i)assert(shadow[i]==ui::pack565(pixels[i],order));
 return count;
}
int main(){
 for(auto order:{ui::PixelOrder::RGB,ui::PixelOrder::BGR})for(unsigned side=0;side<2;++side)for(unsigned pattern=0;pattern<3;++pattern){unsigned old=render(false,pattern,order,side),now=render(true,pattern,order,side);if(pattern==0)assert(old==2 && now==1);if(pattern==2)assert(old==2 && now==2);}
 uint8_t packet[8192]{};unsigned a=display_probe_packet(0,0,0,128,0x1234,1,packet);auto literal=display_wire_facts(packet,a);PreparedDisplayRect r;r.width=128;r.rows=16;r.bytes=a;uint16_t pixels[480*16]{};decode(packet,r,pixels);
 unsigned b=display_probe_packet(0,0,0,128,0x1234,2,packet);auto mixed=display_wire_facts(packet,b);assert(a==b && literal.commands==1 && mixed.commands==16 && literal.literal_pairs==mixed.literal_pairs+mixed.repeated_pairs);decode(packet,r,pixels);for(unsigned y=0;y<16;++y)for(unsigned x=0;x<128;++x)assert(pixels[y*480+x]==0x1234);
 unsigned c=display_probe_packet(1,0,0,128,0x1234,0,packet);auto run=display_wire_facts(packet,c);assert(run.valid && run.commands==1 && c<a && run.repeated_pairs==literal.literal_pairs);
 std::puts("LCD coalescing: sparse2>1, incompressible fallback, failure replay, RGB/BGR and both LCDs exact; matched-byte/changed-command probes passed");
}
