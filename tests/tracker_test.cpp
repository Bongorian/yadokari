#include "ui_renderer.h"
#include "tracker_view.h"
#include <memory>
#include <cassert>
#include <cstdio>
using namespace groove;
int main(){
 assert(tracker_row(0,-1,7)==6 && tracker_row(6,1,7)==0 && tracker_row(255,1,256)==0);StepState step;assert(!tracker_hidden(step));step.microtiming=-4;assert(tracker_hidden(step));step=StepState{};step.ratchet=3;assert(tracker_hidden(step));KitStepState kit;assert(!tracker_hidden(kit));kit.fx_lock[0]=19;assert(tracker_hidden(kit));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->tracks[0].instrument=Instrument::Fm6;track_at(*p,0,0).length=7;track_at(*p,0,1).length=12;auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Step);auto cycle=[&](){ui->set_shift(true);assert(ui->soft_key(3));ui->set_shift(false);};cycle();assert(ui->step_overview());cycle();assert(ui->tracker_view() && ui->follow_display());assert(ui->encoder(3));assert(ui->selected_step()==3 && !ui->follow_display());assert(ui->encoder_push());float audio[128];e->render(audio,0);assert(track_at(*p,0,0).steps[3].enabled);assert(ui->encoder_direction(0x08));assert(ui->selected_track()==1);assert(ui->encoder_direction(0x20));assert(ui->selected_track()==0);
 auto renderer=std::make_unique<UiRenderer>();ui->animate(1000);renderer->draw(*p,*ui);{FILE* image=fopen("build/tracker.ppm","wb");assert(image);fprintf(image,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){auto c=renderer->pixels[x/480][y*480+x%480];unsigned char rgb[]={uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)};fwrite(rgb,1,3,image);}fclose(image);}cycle();assert(!ui->tracker_view() && !ui->step_overview() && track_at(*p,0,0).steps[3].enabled);assert(ui->undo());e->render(audio,0);assert(!track_at(*p,0,0).steps[3].enabled);
 ui->set_mode(UiMode::Effects);assert(ui->soft_key(3));assert(ui->set_parameter(94,19));e->render(audio,0);assert(!__builtin_strcmp(ui->knob_label(1),"MODULATOR"));assert(ui->knob(1,1000));e->render(audio,0);assert(p->tracks[0].insert_fx[1]==1);renderer->draw(*p,*ui);printf("Tracker: unequal-length wrapping, hidden attributes, shared edits/Undo, view cycling, navigation and Vocoder choice UI passed\n");
}
