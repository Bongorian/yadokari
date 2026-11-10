#include "session_files.h"
#include "../platform/desktop/file_access.h"
#include "../platform/desktop/sample_assets.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <unistd.h>
using namespace groove;
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 char directory[]="build/named-preset-XXXXXX";assert(mkdtemp(directory) && chdir(directory)==0);
 DesktopFiles fs;DesktopSampleAssets library;library.files(&fs);
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);
 p->tracks[0].instrument=Instrument::FM;p->tracks[0].level=.31f;p->tracks[0].pan=-.4f;p->tracks[0].root_note=47;p->tracks[0].midi_channel=9;p->tracks[0].compressor[5]=.7f;p->patterns[0].tracks[0].steps[2].enabled=true;
 auto engine=std::make_unique<Engine>();auto ui=std::make_unique<Performance>(*engine,*p);SessionFiles files(fs,library,*engine,*p,*ui);float scratch[2]{};
 auto flush=[&](){engine->render(scratch,0);ui->poll();};assert(engine->stage_project(*p));ui->project_loaded();flush();
 auto begin=[&](const char* name){assert(ui->request_file(FileAction::SavePreset));assert(ui->browser_editing() && !ui->file_action_pending());assert(copy_text(ui->browser().edit.name,96,name));assert(ui->browser_edit_key(0));assert(files.service());};
 begin("Bell One");assert(fs.exists("PRESETS/Bell One.mgxp"));assert(p->sounds[p->tracks[0].preset_slot].identity.assigned());
 auto saved=std::make_unique<ProjectState>();assert(read_project_file(fs,"PRESETS/Bell One.mgxp",*saved));assert(saved->content_kind==ContentKind::Preset && !__builtin_strcmp(saved->tracks[0].preset_name,"Bell One"));assert(saved->tracks[0].instrument==Instrument::FM && !saved->patterns[0].tracks[0].steps[2].enabled);assert(saved->sounds[0].instrument==Instrument::Empty);
 assert(ui->set_parameter(1,.62f));flush();assert(files.load_preset("PRESETS/Bell One.mgxp"));flush();assert(p->tracks[0].level==.62f && p->tracks[0].pan==-.4f && p->tracks[0].compressor[5]==.7f && p->tracks[0].root_note==47 && p->tracks[0].midi_channel==9 && p->patterns[0].tracks[0].steps[2].enabled);assert(!__builtin_strcmp(p->tracks[0].preset_name,"Bell One"));assert(ui->undo());flush();assert(!__builtin_strcmp(p->tracks[0].preset_name,"Bell One") && p->tracks[0].level==.62f);
 uint32_t original=0,after=0;assert(file_checksum(fs,"PRESETS/Bell One.mgxp",original));begin("Bell One");assert(ui->browser_confirmation() && !__builtin_strcmp(ui->soft_label(0),"OVERWRITE"));assert(file_checksum(fs,"PRESETS/Bell One.mgxp",after) && original==after);assert(ui->browser_edit_key(1));assert(!ui->file_action_pending());
 assert(ui->set_parameter(32,.8f));flush();begin("Bell One");assert(ui->browser_confirmation());assert(ui->browser_edit_key(0) && files.service());assert(read_project_file(fs,"PRESETS/Bell One.mgxp",*saved));assert(saved->tracks[0].engine_parameters[0]==p->tracks[0].engine_parameters[0]);
 assert(ui->set_instrument(Instrument::DrumKit));flush();p->tracks[0].kit.pads[0].level=.23f;p->patterns[0].tracks[0].kit.steps[0][2].enabled=true;assert(engine->stage_project(*p));ui->project_loaded();flush();assert(files.save_preset("PRESETS/Kit One.mgx"));p->patterns[0].tracks[0].kit.steps[0][2].enabled=false;p->patterns[0].tracks[0].kit.steps[0][5].enabled=true;p->tracks[0].level=.42f;p->tracks[0].kit.pads[0].level=.9f;assert(engine->stage_project(*p));ui->project_loaded();flush();assert(files.load_preset("PRESETS/Kit One.mgx"));flush();assert(p->tracks[0].kit.pads[0].level==.23f && p->tracks[0].level==.42f);assert(!p->patterns[0].tracks[0].kit.steps[0][2].enabled && p->patterns[0].tracks[0].kit.steps[0][5].enabled);

 // Every suffix can carry every kind; Browser labels and OPEN follow TYPE.
 const char* suffixes[]={".mgx",".MGXS",".mgxk",".mgxp"};
 auto content=std::make_unique<ProjectState>();
 for(unsigned kind=1;kind<=3;++kind)for(const char* suffix:suffixes){
  make_empty_project(*content);content->content_kind=ContentKind(kind);content->tempo=137;
  if(kind!=1)content->tracks[0].preset_identity=builtin_factory_identity(kind);
  content->tracks[0].instrument=kind==2?Instrument::DrumKit:Instrument::FM;
  char path[64];std::snprintf(path,sizeof path,"kind-%u%s",kind,suffix);
  assert(save_recoverable(fs,path,*content));assert(data_content_kind(fs,path)==kind);
  ui->browser().directory[0]=0;files.refresh();bool found=false;
  for(unsigned i=0;i<ui->browser().count;++i)if(!__builtin_strcmp(ui->browser().entries[i].name,path)){ui->browser().cursor=i;found=true;assert(ui->browser().entries[i].content_kind==kind);break;}
  assert(found);p->tempo=103;assert(engine->stage_project(*p));ui->project_loaded();flush();
  assert(ui->request_file(FileAction::Open) && files.service());flush();
  assert(p->tempo==(kind==1?137:103));assert(p->tracks[0].instrument==content->tracks[0].instrument);
  // Old versions are still rejected under each accepted suffix.
  uint8_t old_version=uint8_t(kFormatVersion-1);assert(fs.write(path,4,&old_version,1,false));
  assert(!data_content_kind(fs,path));files.refresh();
  for(unsigned i=0;i<ui->browser().count;++i)if(!__builtin_strcmp(ui->browser().entries[i].name,path))ui->browser().cursor=i;
  assert(ui->request_file(FileAction::Open) && !files.service());assert(fs.remove(path));
 }
 assert(ui->set_instrument(Instrument::FM));flush();begin("Typed.MGXK");assert(fs.exists("PRESETS/Typed.mgxp"));
 assert(read_project_file(fs,"PRESETS/Typed.mgxp",*saved) && !__builtin_strcmp(saved->tracks[0].preset_name,"Typed"));
 assert(ui->begin_project_save_as());assert(copy_text(ui->browser().edit.name,96,"New Song.mgxp"));assert(ui->browser_edit_key(0) && files.service());
 assert(data_suffix(ui->project_path(),"New Song.mgxs") && data_content_kind(fs,ui->project_path())==1);
 for(const char* suffix:suffixes){char name[64],out[64];std::snprintf(name,sizeof name,"Test%s",suffix);
  assert(data_content_name(out,sizeof out,name,2) && !__builtin_strcmp(out,"Test.mgxk"));
  std::snprintf(name,sizeof name,"Test%s.tmp",suffix);assert(data_mgx_temporary(name));
  std::snprintf(name,sizeof name,"Test%s.bak",suffix);assert(data_disposable_file(name));
 }
 char tiny[4];assert(!data_content_name(tiny,sizeof tiny,"Test",1));assert(!data_mgx_file("fake.mgxz"));
 // WAVESEQ keeps offsets (including fallback waveform selection) during remap.
 make_empty_project(*p);p->tracks[0].instrument=Instrument::WaveSequence;
 p->tracks[0].creative.length[0]=2;p->tracks[0].creative.value[0][0]=0;p->tracks[0].creative.value[0][1]=3;
 uint8_t wav[108]{};assert(wav_header(wav,16,48000));for(unsigned i=44;i<sizeof wav;++i)wav[i]=uint8_t(i*7);
 assert(fs.write("wave.wav",0,wav,sizeof wav,true));assert(copy_text(p->assets[0].path,160,"wave.wav"));
 assert(file_checksum(fs,"wave.wav",p->assets[0].metadata.content_crc));
 WavView bindings[kSampleAssets]{};assert(library.load("wave.wav",bindings[0]));files.set_bindings(bindings);
 assert(engine->stage_project(*p,bindings));ui->project_loaded();flush();assert(files.save_preset("PRESETS/Ring.mgxp"));
 // Clear the authored Ring registration before the fixture deliberately replaces its resource metadata.
 p->sounds[p->tracks[0].preset_slot]=SoundState{};p->tracks[0].preset_identity={};p->tracks[0].preset_slot=kNoPreset;p->tracks[0].preset_fx={};
 // Slot zero belongs to another sound; slot three is an intentional empty fallback.
 p->tracks[0].instrument=Instrument::FM;p->tracks[1].instrument=Instrument::Sampler;p->tracks[1].sample_slot=0;
 p->assets[0].metadata.root=48;p->sounds[200].instrument=Instrument::TapeMicrosound;p->sounds[200].sample_slot=3;
 assert(engine->stage_project(*p,bindings));ui->project_loaded();flush();assert(files.load_preset("PRESETS/Ring.mgxp"));flush();
 unsigned base=p->tracks[0].sample_slot;assert(base!=0 && (base+3)%64!=3);
 assert(p->tracks[0].creative.value[0][0]==0 && p->tracks[0].creative.value[0][1]==3);
 assert(p->assets[base].metadata.root!=48 && !p->assets[(base+3)%64].path[0] && p->assets[0].metadata.root==48);
 assert(ui->undo());flush();assert(p->tracks[0].instrument==Instrument::FM && p->tracks[1].sample_slot==0);
 assert(ui->redo());flush();assert(p->tracks[0].instrument==Instrument::WaveSequence && p->tracks[0].sample_slot==base);
 // Collection includes pool/freeze/audio references and excludes loose assets.
 for(unsigned slot:{10u,11u,12u,13u,14u}){assert(copy_text(p->assets[slot].path,96,"wave.wav"));assert(file_checksum(fs,"wave.wav",p->assets[slot].metadata.content_crc));}
 p->sounds[255].instrument=Instrument::Sampler;p->sounds[255].sample_slot=10;
 p->patterns[1].tracks[0].frozen_asset=11;p->audio[2].clips[0].asset=12;p->audio[2].clips[0].frames=16;
 p->patterns[2].tracks[0].mutation.frozen=true;p->patterns[2].tracks[0].mutation_source[0].locks[0]={300,13,true};
 assert(valid_project(*p));assert(files.collect("collected"));assert(read_project_file(fs,"collected/PROJECTS/project.mgxs",*saved));
 for(unsigned slot:{10u,11u,12u,13u})assert(saved->assets[slot].path[0]);
 assert(!saved->assets[14].path[0] && p->assets[14].path[0] && fs.exists("wave.wav"));
 // Optional-source engines retain their generator fallback, but never hide a missing declared file.
 assert(ui->set_instrument(Instrument::TapeMicrosound));flush();p->tracks[0].sample_slot=20;
 assert(files.save_preset("PRESETS/Fallback.mgxp"));assert(files.load_preset("PRESETS/Fallback.mgxp"));flush();
 assert(read_project_file(fs,"PRESETS/Fallback.mgxp",*saved));assert(copy_text(saved->assets[20].path,160,"missing.wav"));
 assert(save_recoverable(fs,"PRESETS/Missing.mgxp",*saved));assert(!files.load_preset("PRESETS/Missing.mgxp"));
 std::puts("named preset: save, explicit overwrite/cancel, provenance, independent strip/phrase, kit definition and empty pool passed");
}
