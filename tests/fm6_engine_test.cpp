#include "engine.h"
#include "performance.h"
#include "dx7_import.h"
#include "../platform/desktop/file_access.h"
#include "plaits/resources.h"
#include <filesystem>
#include <memory>
#include <cassert>
#include <unistd.h>
using namespace groove;
int main(){
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);auto t=track_at(*p,0,0);t.instrument=Instrument::Fm6;t.fm6.values[0]=32;
 snapshot_sound(p->sounds[255],t);t.fm6.values[1]=1.5f;assert(p->sounds[255].fm6.values[1]==1);mount_sound(t,p->sounds[255]);
 ChunkBuffer bytes;assert(encode_project_buffer(bytes,*p));ChunkSource source{bytes.data,bytes.size,0};auto out=std::make_unique<ProjectState>();assert(decode_project(source,*out));assert(out->tracks[0].fm6.values[0]==32 && out->sounds[255].fm6.values[0]==32);
 auto e=std::make_unique<Engine>();assert(e->stage_project(*p));float audio[256]{};e->render(audio,0);Command note{CommandType::MidiInput,0,69,0x90};note.scalar=110;assert(e->submit(note));double energy=0;for(unsigned b=0;b<40;++b){e->render(audio,128);for(float x:audio){assert(__builtin_isfinite(x));energy+=x*x;}}assert(energy>.001);
 auto ui=std::make_unique<Performance>(*e,*p);ui->set_mode(UiMode::Synth);assert(ui->knob_parameter(0)==engine_target(Instrument::Fm6,0));assert(ui->knob(0,1000));e->render(audio,0);assert(t.fm6.values[0]>32);assert(ui->undo());e->render(audio,0);assert(t.fm6.values[0]==32);
 auto cwd=std::filesystem::current_path();char root[]="/tmp/mk3-dx7-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);DesktopFiles fs;uint8_t bank[4104]={0xf0,0x43,0,9,0x20,0};__builtin_memcpy(bank+6,plaits::fm_patches_table[1],4096);unsigned sum=0;for(unsigned i=6;i<4102;++i)sum+=bank[i];bank[4102]=uint8_t((-sum)&127);bank[4103]=0xf7;assert(fs.write("bank.syx",0,bank,sizeof bank,true));char folder[160];assert(import_dx7_bank(fs,"bank.syx",folder,sizeof folder));FileEntry entries[40];assert(fs.list(folder,entries,40)==32);char path[192];assert(join_path(path,sizeof path,folder,entries[0].name));assert(read_project_file(fs,path,*out));assert(out->content_kind==ContentKind::Preset && out->tracks[0].instrument==Instrument::Fm6 && out->tracks[0].fm6.values[0]==-1);for(auto& sound:out->sounds)assert(sound.instrument==Instrument::Empty);
 char second[160];assert(import_dx7_bank(fs,"bank.syx",second,sizeof second));assert(__builtin_strcmp(folder,second));bank[4102]^=1;assert(fs.write("broken.syx",0,bank,sizeof bank,true));assert(!import_dx7_bank(fs,"broken.syx",second,sizeof second));assert(fs.exists("bank.syx"));std::filesystem::current_path(cwd);std::filesystem::remove_all(root);
 printf("FM6: engine/render/UI/codec/independent pool and portable 32-preset import passed; energy %.6f\n",energy);
}
