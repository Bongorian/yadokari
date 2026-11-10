#include "../src/usb_implicit_cadence.h"
#include "../src/usb_receive_lengths.h"
#include "../src/usb_request_fifo.h"
#include "../src/sync_output_guard.h"
#include "../src/midi_dma_buffer.h"
#include "../src/pcm_output.h"
#include <cassert>
int main() {
 uint8_t sample[3]={}; groove::pcm24le(0.5f,sample); float round=groove::pcm24le_read(sample);
 assert(round>0.499f && round<0.501f);
 groove::pcm24le(-0.5f,sample); round=groove::pcm24le_read(sample);
 assert(round< -0.499f && round> -0.501f);
 const uint8_t min24[3]={0,0,128}; assert(groove::pcm24le_read(min24)==-1.0f);
 groove::MidiDmaBuffer midi;assert(midi.prepare(4,64));auto* first=midi.data();assert((uintptr_t(first)&63)==0);first[0]=42;assert(midi.prepare(4,64) && midi.data()==first && midi.data()[0]==42);assert(!midi.prepare(4,3));assert(midi.prepare(512,64));assert((uintptr_t(midi.data())&63)==0);
 groove::SyncOutputGuard output;assert(output.ready());output.completed(false,2000);assert(output.ready());output.completed(true,3000000);assert(output.ready());output.completed(false,3000000);assert(!output.ready());output.reset();assert(output.ready());
 groove::UsbReceiveLengths lengths;lengths.reset();lengths.submitted(0,104);lengths.submitted(1,80);
 assert(lengths.completed(0));assert(lengths.completed(80));lengths.submitted(0,104);lengths.submitted(1,80);
 assert(!lengths.completed(112));assert(!lengths.completed(~0u));lengths.reset();lengths.submitted(0,104);assert(lengths.completed(48));
 lengths.reset(8);for(unsigned i=0;i<8;++i)lengths.submitted(i,48+i);for(unsigned i=0;i<8;++i)assert(lengths.completed(48+i));lengths.submitted(0,48);assert(lengths.completed(48));
 groove::UsbRequestFifo<int,8> fifo;int requests[9]{};for(auto& item:requests) {if(fifo.size()<8)assert(fifo.push(&item));else assert(!fifo.push(&item));}assert(fifo.front()==&requests[0]);assert(fifo.erase(&requests[3]));assert(fifo.erase(&requests[0]));assert(fifo.front()==&requests[1]);assert(!fifo.erase(&requests[8]));assert(fifo.push(&requests[8]));for(unsigned i=1;i<8;++i)if(i!=3)assert(fifo.erase(&requests[i]));assert(fifo.front()==&requests[8] && fifo.erase(&requests[8]) && fifo.empty());
 groove::UsbImplicitCadence q; unsigned frames=0;
 q.reset(2); assert(!q.take(frames)); assert(q.ready()==2);
 assert(q.capture(0)); assert(q.take(frames) && frames==0); assert(q.completed()); assert(q.capture(5)); assert(q.capture(6)); assert(q.capture(5));
 assert(q.take(frames) && frames==5); assert(q.take(frames) && frames==6); assert(!q.take(frames));
 assert(q.completed()); assert(q.take(frames) && frames==5); assert(q.completed()); assert(q.completed());
 assert(!q.completed()); assert(!q.take(frames));
 q.reset(q.Capacity); for(unsigned i=0;i<q.Capacity;++i) assert(q.capture(i+1));
 assert(q.capture(q.Capacity+1)); assert(q.queued()==17 && q.dropped()==q.Capacity-16);
 for(unsigned i=q.Capacity-15;i<=q.Capacity+1;++i)assert(q.take(frames)&&frames==i);
 assert(q.queued()==0);
 q.reset(q.Capacity);for(unsigned i=0;i<q.Capacity;++i)assert(q.capture(i));
 for(unsigned i=0;i<q.Capacity;++i)assert(q.take(frames)&&frames==i);
 assert(q.queued()==0 && !q.take(frames));
 assert(q.captures()==q.Capacity && q.takes()==q.Capacity && q.completions()==0 && q.zero_captures()==1);
 q.reset(2); assert(q.queued()==0 && q.ready()==2); assert(q.capture(6)); assert(q.take(frames) && frames==6);
}
