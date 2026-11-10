#include "file_http_deadline.h"
#include "display_frame_gate.h"
#include "recovery_frame.h"
#include <cassert>
#include <cstdio>
using namespace groove;
struct Canvas {unsigned value=0;};struct Updater {void Draw(Canvas& c){++c.value;}};
struct Display {DisplayFrameGate gate;unsigned frames=0;void NewFrame(){++frames;gate.new_frame();}bool FrameComplete()const{return gate.complete();}};
int main(){
 FileHttpDeadline deadline;deadline.receive(1);assert(!deadline.expired(4000000));deadline.commit();assert(!deadline.expired(60000000));deadline.response(60000000);assert(!deadline.expired(60000001));assert(!deadline.expired(65000000));assert(deadline.expired(65000001));deadline.progress(65000001);assert(!deadline.expired(65000002));deadline.receive(70000000);assert(deadline.expired(75000001));
 Updater updater;Canvas canvas;Display display;RecoveryFramePublisher publisher;
 for(unsigned i=1;i<=5;++i){assert(publisher.draw(updater,canvas,display,true));assert(canvas.value==i && display.frames==i);assert(!publisher.draw(updater,canvas,display,true));assert(canvas.value==i && display.frames==i);for(unsigned side=0;side<2;++side){assert(display.gate.needs_scan(side));display.gate.scanned(side);}assert(display.gate.complete());}
 std::puts("R6/R14: commit is not receive-idle, response has a fresh deadline, repeated Recovery frames reach both LCD scan gates passed");
}
