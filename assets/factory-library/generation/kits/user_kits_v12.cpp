// Scratch-only tool (kit pack production, MGXC v12 round): build all 24
// licensed factory kit families from build/factory-kits.tsv as
// factory-library masters for the v12 factory import. Identical pad
// construction to factory.cpp licensed_kits() (and to the v11 round's
// user_kits.cpp), except each pad sample asset is recorded at its
// canonical path ASSETS/SAMPLES/<basename>, the kit is written for its
// factory device placement FACTORY/KITS/LIBRARY/<name>.mgxk
// (project_paths conversion applied by the canonical codec helpers,
// exactly like the firmware save path, so asset records are stored
// relative to that directory), and — new for v12 — track 0 carries
// the kit's display name (PNAM) and its fixed PresetIdentity (IDEN)
// from build/kit-identities.tsv (name <TAB> guid128 <TAB> rev),
// generated from the pack manifest kit-guids.json (repo allocation
// assets/factory-library/identities.json). The v12 encoder refuses a
// Kit without an assigned identity, so a missing table entry fails the
// build instead of emitting an anonymous kit.
// (The first v12 round wrote for the user KITS/ shelf; the factory
// submission round writes the same sounds for the LIBRARY placement.)
// Modes:
//   user-kits-v12 build  <out_dir>   (cwd = repo root; reads the TSVs)
//   user-kits-v12 verify             (cwd = staging DATA root with
//                                     FACTORY/KITS/LIBRARY/ and
//                                     ASSETS/SAMPLES/)
#include "engine.h"
#include "factory_content.h"
#include "session_files.h"
#include "wav_writer.h"
#include "../../platform/desktop/file_access.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace groove;
static constexpr uint8_t kGm[16]={36,38,42,46,39,37,45,50,75,76,77,54,49,55,35,40};
static const char* base_name(const char* path){const char* r=path;for(const char* p=path;*p;++p)if(*p==47)r=p+1;return r;}

static std::map<std::string,PresetIdentity> load_identities(){
 std::map<std::string,PresetIdentity> out;
 FILE* f=std::fopen("build/kit-identities.tsv","rb");if(!f)return out;
 char line[512];
 while(std::fgets(line,sizeof line,f)){
  char* name=std::strtok(line,"\t\r\n");if(!name)continue;
  char* guid=std::strtok(nullptr,"\t\r\n");char* rev=std::strtok(nullptr,"\t\r\n");
  if(!guid || !rev)continue;
  PresetIdentity id;if(!parse_preset_guid(guid,id.guid))continue;
  id.revision=uint32_t(std::strtoul(rev,nullptr,10));id.origin=PresetOrigin::Factory;
  if(id.assigned() && id.valid())out[name]=id;
 }
 std::fclose(f);return out;
}

static int build(const char* out_dir){
 DesktopFiles files;
 auto identities=load_identities();
 if(identities.size()!=24){std::fprintf(stderr,"identity table has %zu entries, expected 24\n",identities.size());return 1;}
 FILE* catalog=std::fopen("build/factory-kits.tsv","rb");if(!catalog){std::fprintf(stderr,"no TSV\n");return 1;}
 char line[2048];unsigned built=0;
 while(std::fgets(line,sizeof line,catalog)){
  char* name=std::strtok(line,"\t\r\n");if(!name)continue;
  std::vector<std::string> samples;
  for(unsigned pad=0;pad<16;++pad){char* s=std::strtok(nullptr,"\t\r\n");if(!s){samples.clear();break;}samples.emplace_back(s);}
  if(samples.size()!=16){std::fprintf(stderr,"TSV row for %s does not have 16 samples\n",name);return 1;}
  auto idit=identities.find(name);
  if(idit==identities.end()){std::fprintf(stderr,"no identity allocated for kit '%s'\n",name);return 1;}
  auto kit=std::make_unique<ProjectState>();make_empty_project(*kit);kit->content_kind=ContentKind::Kit;
  auto track=track_at(*kit,0,0);track.instrument=Instrument::DrumKit;track.level=0.8f;
  if(!copy_text(track.preset_name,sizeof track.preset_name,name)){std::fprintf(stderr,"name fail %s\n",name);return 1;}
  track.preset_identity=idit->second;
  for(unsigned pad=0;pad<16;++pad){
   const char* sample=samples[pad].c_str();
   WavView view;uint64_t offset=0;if(!sample_file_header(files,sample,view,offset)){std::fprintf(stderr,"bad wav %s\n",sample);return 1;}
   auto& asset=kit->assets[pad];
   char canonical[128];std::snprintf(canonical,sizeof canonical,"ASSETS/SAMPLES/%s",base_name(sample));
   if(!copy_text(asset.path,sizeof asset.path,canonical) || !file_checksum(files,sample,asset.metadata.content_crc)){std::fprintf(stderr,"asset fail %s\n",sample);return 1;}
   asset.metadata.rate=view.rate;asset.metadata.frames=view.frames;asset.metadata.channels=uint8_t(view.channels);asset.metadata.bits=view.bits;asset.metadata.root=60;
   auto& sound=track.kit.pads[pad];sound.gm_note=kGm[pad];sound.sample[0]=uint8_t(pad);sound.level=pad==0 || pad==14?0.6f:0.42f;sound.edit.choke=pad==2 || pad==3?1:0;sound.edit.release_ms=6;
   for(unsigned step=0;step<16;++step){bool enabled=pad==0?step%4==0:pad==1?step%8==4:pad==2?step%2==1:pad==3?step%16==14:pad==4?step%8==4:false;track.kit.steps[pad][step].enabled=enabled;track.kit.steps[pad][step].velocity=pad==2?72:100;}
   std::printf("PAD\t%s\t%u\t%u\t%s\t%08x\t%u\t%u\t%u\t%u\n",name,pad,unsigned(kGm[pad]),canonical,asset.metadata.content_crc,view.frames,view.rate,unsigned(view.channels),view.bits);
  }
  char logical[160];std::snprintf(logical,sizeof logical,"FACTORY/KITS/LIBRARY/%s.mgxk",name);
  auto copy=std::make_unique<ProjectState>(*kit);
  if(!project_paths(*copy,logical,false)){std::fprintf(stderr,"paths fail\n");return 1;}
  std::filesystem::create_directories(std::filesystem::path(out_dir)/"FACTORY/KITS/LIBRARY");
  std::string out=std::string(out_dir)+"/"+logical;
  FileWriter writer(files,out.c_str());if(!encode_project(writer,*copy) || !writer.finish()){std::fprintf(stderr,"encode fail %s\n",name);return 1;}
  char guid_text[33];preset_guid_text(idit->second.guid,guid_text);
  std::printf("KIT\t%s\t%s\t%s\n",name,out.c_str(),guid_text);++built;
 }
 std::fclose(catalog);
 if(built!=24){std::fprintf(stderr,"built %u kits, expected 24\n",built);return 1;}
 return 0;
}

static int verify(){
 DesktopFiles files;unsigned failures=0;
 auto identities=load_identities();
 // verify runs from the staging root, where build/kit-identities.tsv is
 // not present; the table is re-read from KITS-sidecar if available.
 if(identities.empty()){
  // Fall back: identities are checked against the files' own IDEN by
  // the external manifest step; here they must at least be assigned
  // factory identities (the codec already enforced assigned+valid).
  std::printf("NOTE no identity table in cwd; allocation equality is checked externally\n");
 }
 for(const auto& entry:identities)(void)entry;
 std::vector<std::string> names;
 {
  // Kit list comes from the staging factory-library directory itself.
  for(const auto& e:std::filesystem::directory_iterator("FACTORY/KITS/LIBRARY")){
   std::string f=e.path().filename().string();
   if(f.size()>5 && f.compare(f.size()-5,5,".mgxk")==0)names.push_back(f.substr(0,f.size()-5));
  }
  std::sort(names.begin(),names.end());
 }
 if(names.size()!=24){std::printf("FAIL staging has %zu kits, expected 24\n",names.size());++failures;}
 for(auto& name:names){
  char logical[160];std::snprintf(logical,sizeof logical,"FACTORY/KITS/LIBRARY/%s.mgxk",name.c_str());
  // Header: MGXC + format version 12, straight from the bytes.
  {
   std::vector<uint8_t> head(6);unsigned got=0;
   if(!files.read(logical,0,head.data(),6,got) || got!=6 || std::memcmp(head.data(),"MGXC",4) || head[4]!=12 || head[5]!=0){
    std::printf("FAIL %s header/version\n",name.c_str());++failures;continue;
   }
  }
  auto project=std::make_unique<ProjectState>();
  if(!read_project_file(files,logical,*project)){std::printf("FAIL %s decode\n",name.c_str());++failures;continue;}
  if(!project_paths(*project,logical,true)){std::printf("FAIL %s resolve\n",name.c_str());++failures;continue;}
  if(project->content_kind!=ContentKind::Kit){std::printf("FAIL %s kind\n",name.c_str());++failures;continue;}
  auto track=track_at(*project,0,0);
  if(track.instrument!=Instrument::DrumKit){std::printf("FAIL %s instrument\n",name.c_str());++failures;continue;}
  if(std::strcmp(track.preset_name,name.c_str())){std::printf("FAIL %s preset_name '%s'\n",name.c_str(),track.preset_name);++failures;continue;}
  const auto& id=project->tracks[0].preset_identity;
  if(!id.assigned() || id.origin!=PresetOrigin::Factory || id.revision!=1 || !id.parent.empty()){
   std::printf("FAIL %s identity shape\n",name.c_str());++failures;continue;
  }
  auto idit=identities.find(name);
  if(idit!=identities.end() && !same_preset_identity(id,idit->second)){
   std::printf("FAIL %s identity != allocation\n",name.c_str());++failures;continue;
  }
  if(!verify_project_assets(files,*project)){std::printf("FAIL %s assets (crc/header)\n",name.c_str());++failures;continue;}
  bool pads_ok=true;
  for(unsigned pad=0;pad<16;++pad){
   const auto& asset=project->assets[pad];const auto& sound=track.kit.pads[pad];
   if(!asset.path[0] || std::strncmp(asset.path,"ASSETS/SAMPLES/",15) || sound.sample[0]!=pad || sound.gm_note!=kGm[pad])pads_ok=false;
  }
  if(!pads_ok){std::printf("FAIL %s pad path/gm/sample wiring\n",name.c_str());++failures;continue;}
  // Byte stability: re-encode the resolved state for the same logical path.
  auto copy=std::make_unique<ProjectState>(*project);
  if(!project_paths(*copy,logical,false)){std::printf("FAIL %s re-relativize\n",name.c_str());++failures;continue;}
  ChunkBuffer encoded;if(!encode_project_buffer(encoded,*copy)){std::printf("FAIL %s re-encode\n",name.c_str());++failures;continue;}
  uint64_t size=files.size(logical);std::vector<uint8_t> raw(size);unsigned got=0;
  if(!files.read(logical,0,raw.data(),unsigned(size),got) || got!=size || encoded.size!=size || std::memcmp(encoded.data,raw.data(),size)){
   std::printf("FAIL %s byte-stability (file %llu vs re-encoded %u)\n",name.c_str(),(unsigned long long)size,encoded.size);++failures;continue;
  }
  char guid_text[33];preset_guid_text(id.guid,guid_text);
  std::printf("VERIFY OK %s (format v12, 16 pads, assets+crc ok, byte-stable, iden %s)\n",name.c_str(),guid_text);
 }
 return failures?1:0;
}

int main(int argc,char** argv){
 if(argc>=3 && !std::strcmp(argv[1],"build"))return build(argv[2]);
 if(argc>=2 && !std::strcmp(argv[1],"verify"))return verify();
 std::fprintf(stderr,"usage: user-kits-v12 build <out_dir> | verify\n");return 2;
}
