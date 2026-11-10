#include "session_files.h"
#include "midi_import.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <unistd.h>
using namespace groove;
static void mapping(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto sound=std::make_unique<SoundState>();sound->instrument=Instrument::Layer;sound->layer.count=2;
 for(unsigned i=0;i<64;++i){std::snprintf(p->assets[i].path,96,"occupied-%u.wav",i);p->assets[i].metadata.content_crc=100+i;}
 for(unsigned i:{2u,5u,30u,35u})p->assets[i]={};
 SampleAssetState incoming[64]{};for(unsigned i:{0u,3u,10u,15u}){std::snprintf(incoming[i].path,96,"source-%u.wav",i);incoming[i].metadata.content_crc=i+1;}
 for(unsigned i=0;i<2;++i){auto& leaf=sound->layer.parts[i].sound;leaf.instrument=Instrument::WaveSequence;leaf.sample_slot=i?10:0;leaf.creative.length[0]=2;leaf.creative.value[0][0]=0;leaf.creative.value[0][1]=i?5:3;}
 MaterialMapping map;assert(plan_material_mapping(*p,*sound,incoming,map));assert(map.slots[0]==2 && map.slots[3]==5 && map.slots[10]==30 && map.slots[15]==35);
 remap_sound_assets(*sound,map.slots);assert(sound->layer.parts[0].sound.sample_slot==2 && sound->layer.parts[1].sound.sample_slot==30);
 // The base is an address anchor, not necessarily a referenced material slot.
 sound->layer.count=1;auto& leaf=sound->layer.parts[0].sound;leaf.sample_slot=63;leaf.creative.value[0][0]=1;leaf.creative.value[0][1]=4;assert(plan_material_mapping(*p,*sound,incoming,map));assert((map.slots[63]+1)%64==map.slots[0] && (map.slots[63]+4)%64==map.slots[3]);
}
int main(){
 mapping();auto cwd=std::filesystem::current_path();char root[]="/tmp/review-workflows-XXXXXX";assert(mkdtemp(root) && !chdir(root));
 DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);auto p=std::make_unique<ProjectState>();auto engine=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*engine,*p);SessionFiles files(fs,library,*engine,*p,*ui);float audio[128];
 auto settle=[&]{for(unsigned i=0;i<32;++i){engine->render(audio,64);ui->poll();}};
 auto reset=[&]{make_empty_project(*p);p->tracks[0].instrument=Instrument::DrumKit;assert(engine->stage_project(*p));ui->project_loaded();settle();assert(ui->select_kit_pad(3));};reset();
 auto t=track_at(*p,0,0);t.kit.steps[3][0].enabled=1;t.kit.steps[3][0].velocity=73;t.kit.steps[3][0].sample_lock=7;t.steps[0].lock_only=true;t.steps[0].locks[0]={3,.6f,true};
 assert(ui->rotate_track(1));assert(t.kit.steps[3][1].enabled && t.kit.steps[3][1].sample_lock==7 && t.steps[1].lock_only);settle();assert(ui->undo());settle();assert(t.kit.steps[3][0].enabled && !t.kit.steps[3][1].enabled);assert(ui->redo());settle();assert(t.kit.steps[3][1].enabled);
 assert(ui->extend_track() && ui->extend_track());settle();assert(t.length==32 && t.kit.steps[3][17].sample_lock==7 && t.steps[17].lock_only);assert(ui->undo());settle();assert(t.length==16);
 assert(ui->double_pattern());settle();assert(t.length==32 && t.kit.steps[3][17].velocity==73 && t.steps[17].lock_only);assert(ui->undo());settle();
 // DUPLICATE must skip a kit-only destination and an automation-only one.
 track_at(*p,1,0).kit.steps[5][4].enabled=1;track_at(*p,2,0).automation[0].enabled=true;
 assert(ui->duplicate_pattern());settle();assert(p->active_pattern==3 && track_at(*p,3,0).kit.steps[3][1].velocity==73 && track_at(*p,1,0).kit.steps[5][4].enabled);
 for(unsigned length:{16u,80u}){reset();auto selected=track_at(*p,0,0);selected.length=uint16_t(length);selected.kit.steps[4][7].enabled=1;assert(ui->euclidean(5,3));settle();unsigned count=0;for(unsigned i=0;i<length;++i)count+=selected.kit.steps[3][i].enabled;assert(count==5 && selected.kit.steps[4][7].enabled);assert(ui->undo());settle();for(unsigned i=0;i<length;++i)assert(!selected.kit.steps[3][i].enabled);}
 reset();{KitStepState hit;hit.enabled=1;assert(ui->set_kit_step(3,2,hit));}settle();assert(track_at(*p,0,0).kit.steps[3][2].enabled && !track_at(*p,0,0).steps[2].enabled);unsigned velocity[16];bool enabled[16];for(unsigned i=0;i<16;++i){velocity[i]=track_at(*p,0,0).kit.steps[3][i].velocity;enabled[i]=track_at(*p,0,0).kit.steps[3][i].enabled;}assert(ui->dice());settle();bool changed=false;for(unsigned i=0;i<16;++i){changed|=velocity[i]!=track_at(*p,0,0).kit.steps[3][i].velocity || enabled[i]!=bool(track_at(*p,0,0).kit.steps[3][i].enabled);assert(!track_at(*p,0,0).steps[i].enabled);}assert(changed);{KitStepState hit;hit.enabled=1;assert(ui->set_kit_step(3,2,hit));}settle();
 // One recorded event, zero note-ons: CC-only and note-off-only imports
 // must reject instead of committing an empty kit.
 const uint8_t statuses[]={0xb0,0x80};for(auto status:statuses){uint8_t midi[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,8,0,status,36,0,0,0xff,0x2f,0};assert(fs.write("empty-notes.mid",0,midi,sizeof midi,true));MidiImport import(fs);assert(import.start("empty-notes.mid",*p,0,0));for(unsigned i=0;i<100 && import.busy();++i)import.service();assert(!import.done() && import.notes()==0 && track_at(*p,0,0).kit.steps[3][2].enabled);}
 // A pitched spill must not overwrite dormant kit lanes on an Empty track.
 track_at(*p,1,1).kit.steps[0][0].enabled=1;
 const uint8_t pitched[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,0x90,100,96,24,0x80,100,0,0,0xff,0x2f,0};assert(fs.write("spill.mid",0,pitched,sizeof pitched,true));{MidiImport parser(fs);assert(parser.start("spill.mid",*p,0,0));for(unsigned i=0;i<100 && parser.busy();++i)parser.service();assert(parser.done() && !(parser.spill_mask()&2) && (parser.spill_mask()&4));}
 // Tape is optional per leaf; a co-located mandatory Sampler still fails.
 make_empty_project(*p);auto layer=track_at(*p,0,0);layer.instrument=Instrument::Layer;layer.layer.count=1;layer.layer.parts[0].sound.instrument=Instrument::TapeMicrosound;assert(engine->stage_project(*p));ui->project_loaded();settle();
 assert(files.save_preset("Tape-Layer.mgxp"));settle();auto saved=std::make_unique<ProjectState>();assert(read_project_file(fs,"Tape-Layer.mgxp",*saved) && saved->tracks[0].layer.parts[0].sound.instrument==Instrument::TapeMicrosound);
 layer.layer.count=2;layer.layer.parts[1].sound.instrument=Instrument::Sampler;assert(!files.save_preset("Missing-Sampler.mgxp") && !fs.exists("Missing-Sampler.mgxp"));
 std::filesystem::current_path(cwd);std::filesystem::remove_all(root);std::puts("R3/R5/R9/R10/R11/R12: Kit operation audit/Undo, empty-note import rejection, per-child WaveRing remap and optional leaf saves passed");
}
