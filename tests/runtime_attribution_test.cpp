#include "render_audit.h"
#include "audio_slow_trace.h"
#include "display_attribution.h"
#include "display_frame_gate.h"
#include "usb_progress.h"
#include <cassert>
#include <cstdio>
#include <memory>
static unsigned tick=0;static uint32_t clock_us(){return tick;}
int main(){using namespace groove;
 RenderAudit a;a.begin(clock_us,100,true);a.generation(4);tick=102;auto c=a.command_begin();tick=103;{RenderResetScope reset(a,RenderAudit::GroupReset);tick=107;}tick=110;a.command_end(c,7,2);tick=115;a.control_end();tick=140;a.visual(6);a.finish(5);auto s=a.sample();assert(s.total==40 && s.control==15 && s.commands==8 && s.command_count==1 && s.reset[RenderAudit::GroupReset]==4 && s.generation_before==4 && s.generation_after==5 && s.top_kind==7 && s.top_us==8);
 a.begin(clock_us,200,false);assert(!a.clock());tick=205;c=a.command_begin();assert(a.clock());tick=210;a.command_end(c,8,3);tick=220;a.control_end();tick=230;a.finish(6);assert(a.sample().total==30 && a.sample().control==20 && a.sample().commands==5);
 auto slow=std::make_unique<AudioSlowTrace>();assert(slow->quiescent());allocation_admission_hook=+[](size_t,unsigned,size_t){return false;};assert(!slow->begin());allocation_admission_hook=nullptr;assert(slow->begin() && slow->worker_enter());slow->worker_exit(true,{1,501,1,2,s});slow->worker_exit(true,{2,500,1,1,{}});slow->worker_exit(true,{3,900,6,6,{}});slow->worker_exit(true,{5,507,6,6,{},16,27000,4000,0,600});slow->worker_exit(true,{6,700,6,6,{},32,37000,4000,0,600});slow->seal();assert(!slow->ready() && !slow->quiescent());slow->worker_exit(true,{4,800,7,7,s});assert(slow->quiescent() && slow->ready() && slow->count()==3 && !slow->lost());assert(slow->next()->at==1 && slow->next()->poly_peak==16 && slow->next()->at==4 && !slow->next());assert(slow->begin());slow->seal();assert(!slow->worker_enter() && slow->ready());assert(slow->begin());slow->seal();slow->acknowledge();assert(slow->ready() && slow->quiescent() && !slow->count());
 UsbProgress p;p.enabled(true);auto first=p.open(100),second=p.open(120);assert(first && second && !p.open(130));p.audio(200);p.audio(300);auto f=p.close(first,400),r=p.close(second,500);assert(f.callbacks==2 && f.max_gap==100 && r.callbacks==2 && r.max_gap==200);assert(!p.close(first,600).callbacks);p.enabled(false);assert(!p.open(700));
 DisplayAttribution d;assert(d.begin(8));d.frame({9,8,1,90,92,100,110,{},{},{}});d.complete(9,0,80,140,17);d.complete(9,1,85,145,17);assert(d.job({9,0,110,112,113,114,120,122,1,4,1,32,1,1})==1);d.task(3,110,250);assert(d.next_job()->ended==120);assert(d.next_task()->kind==3);auto* frame=d.next_frame();assert(frame->origin[0]==80 && frame->completed[1]==145 && frame->window[1]==17);assert(!d.next_frame());
 UiFrameClock cadence;assert(cadence.due(10000,true,1) && cadence.due_at()==10000);assert(!cadence.due(20000,true,1));assert(!cadence.due(50000,false,1));assert(cadence.due(51000,true,1) && cadence.due_at()==33334);assert(cadence.due(52000,true,2) && cadence.due_at()==52000);
 UiFrameClock ahead;assert(ahead.due(0,true,1,8000) && ahead.release_at()==0);assert(!ahead.due(25333,true,1,8000));assert(ahead.due(25334,true,1,8000) && ahead.due_at()==25334 && ahead.release_at()==33334);assert(!ahead.due(33334,true,1,8000));assert(!ahead.due(60000,false,1,8000));assert(ahead.due(61000,true,1,8000) && ahead.release_at()==66667);assert(ahead.due(62000,true,2,8000) && ahead.release_at()==62000);
 UiFrameClock nominal;unsigned releases=0,draws=0;for(uint64_t us=0;us<10000000;us+=100){if(nominal.due(us,true,1,8000)){++draws;releases+=nominal.release_at()<10000000;}}assert(releases==300 && draws==301); // Last prepared frame is for the next interval; USB release remains30Hz.
 std::puts("Attribution: command/reset containment, phase seal ownership, FIFO audio progress and exact frame/window clocks passed");
}
