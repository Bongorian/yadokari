#include "performance.h"
#include "project_codec.h"
#include <memory>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace groove;
struct Bytes {std::vector<uint8_t> data;size_t pos=0;bool write(const uint8_t* p,size_t n){data.insert(data.end(),p,p+n);return true;}bool read(uint8_t* p,size_t n){if(pos+n>data.size())return false;__builtin_memcpy(p,data.data()+pos,n);pos+=n;return true;}bool at_end()const{return pos==data.size();}};
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::FM;
 auto fm=engine_target(Instrument::FM,0),plaits=engine_target(Instrument::PlaitsMacro,0),model=engine_target(Instrument::PlaitsMacro,5);
 assert(fm!=plaits && !valid_target_address(32) && !target_descriptor(0xffe0));
 for(unsigned e=0;e<instrument_count();++e)for(unsigned i=0;i<target_count(Instrument(e));++i){auto target=target_at(Instrument(e),i);assert(valid_target_address(target));assert(target_index(Instrument(e),target)==i);}
 auto& lane=t.automation[0];lane.enabled=true;lane.parameter=fm;lane.values[0]=.2f;
 GestureOverlay overlay;overlay.apply({0,fm,.3f,0,0,GestureTarget::Parameter});
 auto& macro=p->macros[0];macro.active=true;macro.value=1;macro.routes[0]={true,fm,1,.4f,.4f,LayerMode::Replace};
 ParameterLock locks[kLocks]={{fm,.5f,true}};
 auto value=resolve_parameter(*p,t,0,32,locks,0,true,fm,.1f,nullptr,true,&overlay,0);
 assert(std::fabs(value.value-.6f)<.00001f && value.sources==(FromAutomation|FromGesture|FromMacro|FromLock|FromControlAll));
 // Identical local index in another engine never inherits these influences.
 t.instrument=Instrument::PlaitsMacro;value=resolve_parameter(*p,t,0,32,locks,0,true,fm,.1f,nullptr,true,&overlay,0);
 assert(value.sources==0 && value.value==t.engine_parameters[0]);
 macro.routes[0]={true,3,1,.4f,.4f,LayerMode::Replace};assert(resolve_parameter(*p,t,0,3,nullptr,0).value==.4f);
 lane.parameter=model;lane.values[0]=2;macro.routes[0]={true,model,1,4,4,LayerMode::Replace};
 ParameterLock model_lock[kLocks]={{model,3,true}};
 assert(resolve_target_value(*p,t,0,model,0,model_lock,0).value==3);
 assert(resolve_target_value(*p,t,0,model,0,nullptr,0).value==4);
 macro.active=false;assert(resolve_target_value(*p,t,0,model,0,nullptr,0).value==2);
 // UI binds old control adapters immediately; Undo retains the actual owner.
 make_empty_project(*p);p->tracks[0].instrument=Instrument::FM;
 auto e=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*e,*p);float audio[2]{};
 auto flush=[&](){e->render(audio,0);ui->poll();};assert(e->stage_project(*p));ui->project_loaded();flush();
 assert(ui->set_lock(0,32,.7f));flush();assert(t.steps[0].locks[0].id==fm);
 AutomationLane authored;authored.enabled=true;authored.parameter=32;for(float& v:authored.values)v=.2f;assert(ui->automation(0,authored));flush();assert(t.automation[0].parameter==fm);
 MacroRoute route{true,32,0xffff,.1f,.9f,LayerMode::Replace};assert(ui->macro_route(0,0,route));flush();assert(p->macros[0].routes[0].parameter==fm);
 assert(ui->set_instrument(Instrument::PlaitsMacro));flush();assert(t.automation[0].parameter==fm && t.steps[0].locks[0].id==fm);
 assert(resolve_parameter(*p,t,0,32,t.steps[0].locks,0).sources==0);assert(ui->undo());flush();assert(t.instrument==Instrument::FM);
 assert(resolve_parameter(*p,t,0,32,t.steps[0].locks,0).value==.7f);
 // Step presets bind their own owner, not the persistent track's engine.
 p->sounds[255].instrument=Instrument::PlaitsMacro;t.steps[1].sound=255;assert(ui->set_lock(1,32,.8f));flush();assert(t.steps[1].locks[0].id==plaits);
 assert(ui->set_step_sample_lock(2,300,3));flush();assert(ui->step_sample_lock_value(2,300)==3);assert(ui->set_step_sample_lock(2,300,-1));flush();assert(ui->step_sample_lock_value(2,300)==-1);
 p->gesture.count=1;p->gesture.points[0]={0,model,3,0,0,GestureTarget::Parameter};
 Bytes file;assert(encode_project(file,*p));auto restored=std::make_unique<ProjectState>();assert(decode_project(file,*restored));
 assert(restored->patterns[0].tracks[0].steps[0].locks[0].id==fm && restored->patterns[0].tracks[0].steps[1].locks[0].id==plaits);
 assert(restored->patterns[0].tracks[0].automation[0].parameter==fm && restored->macros[0].routes[0].parameter==fm && restored->gesture.points[0].parameter==model);
 Bytes again;assert(encode_project(again,*restored) && again.data==file.data);
 // Raw old engine IDs are not accepted as persisted targets in the new format.
 restored->patterns[0].tracks[0].steps[0].locks[0].id=32;assert(!valid_project(*restored));
 // The admitted polyphony must use the resolved MODEL, not the preset base.
 make_empty_project(*p);p->tracks[0].instrument=Instrument::PlaitsMacro;p->tracks[0].plaits_model=8;
 t.steps[0].enabled=true;t.steps[0].chord[0]=64;t.steps[0].chord[1]=67;t.steps[0].chord[2]=71;
 t.steps[0].locks[0]={model,2,true};assert(e->stage_project(*p));flush();assert(e->submit({CommandType::Start,0,0,0}));
 float chord[200]{};e->render(chord,100);assert(e->plaits_note_count(0)==2 && e->poly_owner_masks_valid());for(float x:chord)assert(std::isfinite(x));
 assert(e->submit({CommandType::Stop,0,0,0}));flush();ui->project_loaded();
 assert(e->submit({CommandType::GestureRecord,0,0,1}));flush();assert(ui->set_parameter(model,3));flush();
 assert(p->gesture.count==1 && p->gesture.points[0].parameter==model && p->gesture.points[0].value==3);
 assert(e->submit({CommandType::GestureRecord,0,0,0}));flush();
 ui->set_mode(UiMode::Step);assert(ui->pad(9,0x10,800));assert(ui->knob_parameter(1)==model);assert(ui->knob(1,1000));flush();assert(t.steps[5].locks[0].id==model && t.steps[5].locks[0].value==4 && p->tracks[0].plaits_model==3);assert(ui->pad(9,0x20,0));
 ui->set_mode(UiMode::Modulation);assert(ui->soft_key(1));AutomationLane models;models.parameter=model;assert(ui->automation(0,models));flush();assert(ui->knob(3,1000));flush();assert(t.automation[0].values[0]==1);
 assert(ui->soft_key(2));MacroRoute models_route{true,model,1,0,23,LayerMode::Replace};assert(ui->macro_route(0,0,models_route));flush();assert(ui->knob(3,1000));flush();assert(p->macros[0].routes[0].low==1);
 std::puts("Scoped targets: engine isolation, priority, MODEL, UI binding, Undo, step presets and GUID round trip passed");
}
