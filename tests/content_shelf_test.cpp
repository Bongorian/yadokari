#include "content_shelf.h"
#include "wav_writer.h"
#include "../platform/desktop/file_access.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <unistd.h>
using namespace groove;
int main(){
 auto previous=std::filesystem::current_path();char root[]="/tmp/mgx12-shelf-XXXXXX";assert(mkdtemp(root) && !chdir(root));DesktopFiles fs;assert(fs.directory("PRESETS") && fs.directory("FACTORY") && fs.directory("FACTORY/PRESETS") && fs.directory("ASSETS") && fs.directory("ASSETS/SAMPLES") && fs.directory("ASSETS/SAMPLES/NESTED"));
 auto p=std::make_unique<ProjectState>();make_empty_project(*p);p->content_kind=ContentKind::Preset;p->tracks[0].instrument=Instrument::VirtualAnalog;assert(parse_preset_guid("f0123456789abcde0123456789abcdef",p->tracks[0].preset_identity.guid));p->tracks[0].preset_identity.revision=1;p->tracks[0].preset_identity.origin=PresetOrigin::Factory;copy_text(p->tracks[0].preset_name,96,"Factory Bass");assert(save_recoverable(fs,"FACTORY/PRESETS/Bass.mgxp",*p));
 p->tracks[0].preset_identity.origin=PresetOrigin::User;p->tracks[0].preset_identity.parent=p->tracks[0].preset_identity.guid;p->tracks[0].preset_identity.parent_revision=1;p->tracks[0].preset_identity.revision=2;copy_text(p->tracks[0].preset_name,96,"My Bass");assert(save_recoverable(fs,"PRESETS/My Bass.mgxp",*p));
 PresetLibrary index;assert(index.refresh(fs) && index.size()==2 && index.complete());unsigned visible=0,originals=0;for(unsigned i=0;i<index.size();++i){visible+=index.visible(i,Instrument::VirtualAnalog,false);originals+=index.visible(i,Instrument::VirtualAnalog,false,true);}assert(visible==1 && originals==1);assert(index.latest(p->tracks[0].preset_identity.guid)==2);PresetIdentity fixed=p->tracks[0].preset_identity;fixed.revision=1;assert(index.exact(fixed)>=0 && index[unsigned(index.exact(fixed))].factory);
 PresetIdentity fresh;assert(allocate_user_identity(fs,index,fresh,&fixed) && fresh.guid.bytes[0]==0x80 && fresh.revision==1 && fresh.parent==fixed.guid);
 uint8_t bytes[54]{};assert(wav_header_ch(bytes,5,48000,1));assert(fs.write("ASSETS/SAMPLES/NESTED/Moved.wav",0,bytes,sizeof bytes,true));uint32_t crc=0;assert(file_checksum(fs,"ASSETS/SAMPLES/NESTED/Moved.wav",crc));SampleAssetState asset;copy_text(asset.path,96,"../wrong/path.wav");asset.metadata.content_crc=crc;AssetCatalog assets;assert(assets.resolve(fs,asset) && !__builtin_strcmp(asset.path,"ASSETS/SAMPLES/NESTED/Moved.wav"));
 assert(fs.move(asset.path,"ASSETS/SAMPLES/Renamed.bin"));assert(assets.resolve(fs,asset) && !__builtin_strcmp(asset.path,"ASSETS/SAMPLES/Renamed.bin"));
 // An edited cached non-match must be reconsidered even without a timestamp
 // assumption; a changed path hint is never accepted under its previous CRC.
 bytes[44]^=1;assert(fs.write(asset.path,0,bytes,sizeof bytes,true));SampleAssetState changed;copy_text(changed.path,96,"missing.wav");assert(file_checksum(fs,asset.path,changed.metadata.content_crc));assert(assets.resolve(fs,changed) && !__builtin_strcmp(changed.path,asset.path));assert(!assets.resolve(fs,asset));
 assert(fs.remove(changed.path));assert(!assets.resolve(fs,changed));
 ShelfScan stopped;unsigned visits=0;auto stop=[&](const char*,const FileEntry&){++visits;stopped.stop=true;};walk_content_shelf(fs,"",stop,stopped);assert(visits==1 && stopped.files==1);

 SoundState a,b;a.instrument=b.instrument=Instrument::Sampler;a.sample_slot=3;b.sample_slot=9;SampleAssetState aa[64]{},bb[64]{};aa[3].metadata.content_crc=bb[9].metadata.content_crc=123;copy_text(aa[3].path,96,"old.wav");copy_text(bb[9].path,96,"renamed.wav");copy_text(a.preset_name,96,"Old");copy_text(b.preset_name,96,"New");assert(same_preset_content(a,aa,b,bb));b.sample_edit.speed=2;assert(!same_preset_content(a,aa,b,bb));
 std::filesystem::current_path(previous);std::filesystem::remove_all(root);std::puts("Content shelf: exact revisions, user precedence/originals, new identity, CRC relocation/rename/missing and canonical content passed");
}
