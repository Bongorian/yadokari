#include "performance.h"
#include "midi_input_router.h"
#include "ui_renderer.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);for(unsigned t=0;t<16;++t){p->tracks[t].instrument=Instrument::Additive;p->midi_input_route[t]=15;}p->sounds[128].instrument=Instrument::PhaseDist;p->sounds[255].instrument=Instrument::VirtualAnalog;p->sounds[7].instrument=Instrument::Unknown;
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[128]{};e->render(audio,0);auto ui=std::make_unique<Performance>(*e,*p);auto router=std::make_unique<MidiInputRouter>();auto pump=[&](){e->render(audio,64);ui->poll();};auto midi=[&](unsigned s,unsigned a=0,unsigned b=0){MidiPacket packet{0,0,uint8_t(s>=0xf8?1:(s&0xf0)==0xc0?2:3),{uint8_t(s),uint8_t(a),uint8_t(b)}};assert(router->accept(*e,*p,packet));pump();};
 assert(ui->play());pump();assert(ui->playing());ui->set_mode(UiMode::Settings);for(unsigned i=0;i<5;++i)assert(ui->encoder_direction(0x08));assert(ui->soft_key(5));assert(ui->modulation_page()==3);assert(ui->soft_key(4));pump();assert(ui->multitimbral() && !ui->playing() && !ui->play());
 // Fixed routes override all sixteen customized normal routes. CC120 releases the original owner.
 for(unsigned ch=0;ch<16;++ch){midi(0x90|ch,60,100);assert(e->poly_note_count(ch)==1);midi(0xb0|ch,120,0);assert(e->poly_note_count(ch)==0);}
 midi(0xfa);midi(0xfb);assert(!ui->playing());midi(0x91,62,100);midi(0xfc);assert(e->poly_note_count(1)==1);midi(0xb1,120,0);
 midi(0xb1,0,0);midi(0xb1,32,1);midi(0xc1,0);assert(p->tracks[1].instrument==Instrument::PhaseDist);midi(0xc1,127);assert(p->tracks[1].instrument==Instrument::VirtualAnalog);
 midi(0xb1,32,0);midi(0xc1,0);assert(p->tracks[1].instrument==Instrument::VirtualAnalog);midi(0xc1,7);assert(p->tracks[1].instrument==Instrument::VirtualAnalog);midi(0xb1,0,1);midi(0xc1,0);assert(p->tracks[1].instrument==Instrument::VirtualAnalog);assert(p->tracks[0].instrument==Instrument::Additive);
 // Release queued before a mode transition cannot strand voices; normal routes remain intact.
 midi(0x90,65,100);midi(0xb0,64,127);midi(0x80,65);assert(e->poly_note_count(0)==1);assert(ui->soft_key(4));pump();assert(!ui->multitimbral() && !ui->playing() && e->poly_note_count(0)==0);midi(0x90,66,100);assert(e->poly_note_count(15)==1);midi(0x80,66);assert(p->midi_input_route[0]==15);assert(ui->play());pump();assert(ui->playing());
 assert(ui->set_multitimbral(true));pump();assert(e->stage_project(*p));pump();assert(!ui->multitimbral());for(unsigned i=0;i<256;++i)assert(i==7 || i==128 || i==255 || p->sounds[i].instrument==Instrument::Empty);
 puts("Multitimbral: 16 fixed channels, bank/PC boundaries and refusal, transport isolation, sustain transition, normal routes restored, transient mode passed");
}
