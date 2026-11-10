#include "user_files.h"
#include "file_web.h"
#include "file_http_header.h"
#include "file_dhcp.h"
#include "data_directories.h"
#include "transfer_lifetime.h"
#include "file_transfer_fixtures.h"
#include "../platform/desktop/file_access.h"
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>
using namespace groove;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x);return false;}}while(false)
struct FaultFiles:DesktopFiles {bool corrupt=false,partial_create=false;unsigned max_write=0;bool write(const char* path,uint64_t at,const void* bytes,unsigned n,bool truncate)override{if(n>max_write)max_write=n;if(partial_create && truncate){DesktopFiles::write(path,at,bytes,n,truncate);return false;}if(corrupt && n){std::vector<uint8_t> changed(static_cast<const uint8_t*>(bytes),static_cast<const uint8_t*>(bytes)+n);changed[n/2]^=1;return DesktopFiles::write(path,at,changed.data(),n,truncate);}return DesktopFiles::write(path,at,bytes,n,truncate);}};
struct Fixture {std::filesystem::path previous=std::filesystem::current_path();char root[64]="/tmp/mgx13-files-XXXXXX";std::unique_ptr<ProjectState> project=std::make_unique<ProjectState>();FaultFiles fs;UserFiles files;
 Fixture():files(fs,*project){if(!mkdtemp(root) || chdir(root))std::abort();make_empty_project(*project);files.open("PROJECTS/Open.mgxs");}
 ~Fixture(){files.close();std::filesystem::current_path(previous);std::filesystem::remove_all(root);}
};
static bool put(UserFiles& files,const char* name,const std::vector<uint8_t>& bytes){uint8_t sum[32];update::Sha256::hash(bytes.data(),bytes.size(),sum);if(!files.begin(name,bytes.size(),sum))return false;for(unsigned at=0;at<bytes.size();){unsigned n=bytes.size()-at>32768?32768:unsigned(bytes.size()-at);if(!files.chunk(at,bytes.data()+at,n))return false;at+=n;}return files.commit();}
static bool bytes_equal(IFileAccess& fs,const char* path,const std::vector<uint8_t>& bytes){std::vector<uint8_t> actual(bytes.size());unsigned got=0;return fs.size(path)==bytes.size() && fs.read(path,0,actual.data(),actual.size(),got) && got==actual.size() && actual==bytes;}
static bool containers(){Fixture f;auto* entries=new UserFileEntry[256];const ContentKind kinds[]={ContentKind::Song,ContentKind::Kit,ContentKind::Preset};const char* path[]={"PROJECTS/Wave13.mgxs","KITS/Wave13.mgxk","PRESETS/Wave13.mgxp"};const UserShelf shelves[]={UserShelf::Projects,UserShelf::Kits,UserShelf::Presets};
 for(unsigned i=0;i<3;++i){auto bytes=file_fixture::project(kinds[i]);CHECK(!bytes.empty());CHECK(put(f.files,i==0?"Wave13.mgxp":i==1?"Wave13.mgxs":"Wave13.mgxk",bytes));CHECK(!std::strcmp(f.files.destination(),path[i]));CHECK(bytes_equal(f.fs,path[i],bytes));unsigned count=0;CHECK(f.files.list(shelves[i],entries,256,count) && count==1 && !std::strcmp(entries[0].path,path[i]));std::vector<uint8_t> back(bytes.size());unsigned got=0;CHECK(f.files.get(path[i],0,back.data(),back.size(),got) && got==back.size() && back==bytes);CHECK(!f.fs.exists((std::string(path[i])+".tmp").c_str()));}
 delete[] entries;return true;
}
static bool media(){Fixture f;auto w=file_fixture::wav(),m=file_fixture::midi(),bank=file_fixture::syx();std::vector<uint8_t> sf(kSoundFontFixture,kSoundFontFixture+sizeof kSoundFontFixture);unsigned old=sf.size();sf.resize(sf.size()+8+2*1024*1024);std::memcpy(sf.data()+old,"JUNK",4);update::put32(sf.data()+old+4,2*1024*1024);update::put32(sf.data()+4,sf.size()-8);
 CHECK(put(f.files,"Tone.wav",w) && bytes_equal(f.fs,"ASSETS/SAMPLES/Tone.wav",w));CHECK(put(f.files,"Pattern.mid",m) && bytes_equal(f.fs,"ASSETS/MIDI/Pattern.mid",m));CHECK(put(f.files,"Large.sf2",sf) && bytes_equal(f.fs,"ASSETS/SOUNDFONTS/Large.sf2",sf));CHECK(f.fs.max_write<=32768);
 CHECK(put(f.files,"My Bank.syx",bank));CHECK(!std::strcmp(f.files.destination(),"PRESETS/DX7/MY BANK"));FileEntry entries[40];CHECK(f.fs.list(f.files.destination(),entries,40)==32);for(unsigned i=0;i<32;++i){char path[192];CHECK(join_path(path,sizeof path,f.files.destination(),entries[i].name));CHECK(data_content_kind(f.fs,path)==unsigned(ContentKind::Preset));}CHECK(!f.fs.exists("PRESETS/DX7/.MGX-TRANSFER.tmp") && !f.fs.exists("My Bank.syx"));return true;
}
static bool reject_and_keep(){Fixture f;auto w=file_fixture::wav();CHECK(put(f.files,"Good.wav",w));uint8_t sum[32];update::Sha256::hash(w.data(),w.size(),sum);CHECK(put(f.files,"Good.wav",w) && f.files.error()==UserFileError::Ok && bytes_equal(f.fs,"ASSETS/SAMPLES/Good.wav",w));auto broken=w;broken[0]='X';CHECK(!put(f.files,"Good.wav",broken) && f.files.error()==UserFileError::Invalid && bytes_equal(f.fs,"ASSETS/SAMPLES/Good.wav",w));
 CHECK(f.files.begin("Good.wav",w.size(),sum));broken=w;broken.back()^=1;CHECK(f.files.chunk(0,broken.data(),broken.size()));CHECK(!f.files.commit() && f.files.error()==UserFileError::Hash && bytes_equal(f.fs,"ASSETS/SAMPLES/Good.wav",w));
 auto old=file_fixture::project(ContentKind::Song);old[4]=8;CHECK(!put(f.files,"Old.mgxs",old) && f.files.error()==UserFileError::Invalid && !f.fs.exists("PROJECTS/Old.mgxs"));
 auto badbank=file_fixture::syx();badbank[4102]=1;CHECK(!put(f.files,"Broken.syx",badbank) && f.files.error()==UserFileError::Invalid);auto badm=file_fixture::midi();badm[21]=127;CHECK(!put(f.files,"Broken.mid",badm));auto badsf=std::vector<uint8_t>(kSoundFontFixture,kSoundFontFixture+sizeof kSoundFontFixture);badsf[4]^=1;CHECK(!put(f.files,"Broken.sf2",badsf));return true;
}
static bool protection(){Fixture f;CHECK(file_fixture::files(f.fs));copy_text(f.project->assets[0].path,sizeof f.project->assets[0].path,"ASSETS/SAMPLES/Locked.wav");auto w=file_fixture::wav();CHECK(!put(f.files,"Locked.wav",w) && f.files.error()==UserFileError::Protected);CHECK(!f.files.erase("ASSETS/SAMPLES/locked.wav") && f.files.error()==UserFileError::Protected);auto song=file_fixture::project(ContentKind::Song);CHECK(!put(f.files,"open.mgxk",song) && f.files.error()==UserFileError::Protected);CHECK(!f.files.erase("PROJECTS/Open.mgxs") && f.files.error()==UserFileError::Protected);
 std::filesystem::create_hard_link("ASSETS/SAMPLES/Locked.wav","ASSETS/SAMPLES/LOCKED~1.wav");CHECK(!f.files.erase("ASSETS/SAMPLES/LOCKED~1.wav") && f.files.error()==UserFileError::Protected);std::filesystem::create_hard_link("PROJECTS/Open.mgxs","PROJECTS/OPEN~1.mgxs");CHECK(!f.files.erase("PROJECTS/OPEN~1.mgxs") && f.files.error()==UserFileError::Protected);
 const char* invalid[]={"../escape.wav","/absolute.wav","foo/../bad.wav","bad\\name.wav","C:bad.wav",".hidden.wav","name.txt","name.mgx","GROOVE/DEVICE.CFG","FACTORY/DEBUG/Sine.sf2"};uint8_t sum[32]{};for(const char* name:invalid)CHECK(!f.files.begin(name,10,sum));for(const char* path:{"../PROJECTS/Open.mgxs","/PROJECTS/Open.mgxs","PROJECTS/../GROOVE/a.mgxs","ASSETS/SAMPLES/../../wpa_supplicant.conf","FACTORY/DEBUG/Sine.sf2","GROOVE/DEVICE.CFG"}){CHECK(!f.files.erase(path));unsigned got=0;uint8_t byte;CHECK(!f.files.get(path,0,&byte,1,got));}
 CHECK(put(f.files,"demo-slices.wav",w) && bytes_equal(f.fs,"ASSETS/SAMPLES/demo-slices.wav",w));return true;
}
static bool cleanup(){Fixture f;auto w=file_fixture::wav();uint8_t sum[32];update::Sha256::hash(w.data(),w.size(),sum);f.fs.partial_create=true;CHECK(!f.files.begin("Partial.wav",w.size(),sum) && f.files.error()==UserFileError::Io && !f.fs.exists("ASSETS/SAMPLES/.MGX-TRANSFER.tmp"));f.fs.partial_create=false;f.files.busy(true);CHECK(!f.files.begin("Busy.wav",w.size(),sum) && f.files.error()==UserFileError::Busy);f.files.busy(false);CHECK(f.files.begin("Abort.wav",w.size(),sum));CHECK(f.files.chunk(0,w.data(),32));f.files.close();CHECK(!f.fs.exists("ASSETS/SAMPLES/.MGX-TRANSFER.tmp"));CHECK(!f.files.begin("Closed.wav",w.size(),sum) && f.files.error()==UserFileError::Closed);f.files.open("PROJECTS/Open.mgxs");
 CHECK(f.files.begin("Offset.wav",w.size(),sum));CHECK(!f.files.chunk(1,w.data(),32) && f.files.error()==UserFileError::Length);CHECK(!f.fs.exists("ASSETS/SAMPLES/.MGX-TRANSFER.tmp"));
 CHECK(f.files.begin("Short.wav",w.size(),sum));CHECK(f.files.chunk(0,w.data(),20));CHECK(!f.files.commit() && f.files.error()==UserFileError::Length);CHECK(!f.fs.exists("ASSETS/SAMPLES/.MGX-TRANSFER.tmp"));
 f.fs.corrupt=true;CHECK(!put(f.files,"Corrupt.wav",w) && f.files.error()==UserFileError::Hash);f.fs.corrupt=false;CHECK(!f.fs.exists("ASSETS/SAMPLES/Corrupt.wav"));
 TransferLifetime lifetime;CHECK(!lifetime.expired(1000));lifetime.open(1000);CHECK(lifetime.remaining(1000)==600 && !lifetime.expired(600000999) && lifetime.expired(600001000));uint64_t now=1000;struct Guard{TransferLifetime* life;uint64_t* now;} guard{&lifetime,&now};f.files.continuation([](void* p){auto& g=*static_cast<Guard*>(p);return !g.life->expired(*g.now);},&guard);CHECK(f.files.begin("Expires.wav",w.size(),sum));now=600001000;CHECK(!f.files.chunk(0,w.data(),32) && !f.fs.exists("ASSETS/SAMPLES/.MGX-TRANSFER.tmp"));lifetime.close();CHECK(lifetime.remaining(now)==0 && !lifetime.active());f.files.continuation(nullptr,nullptr);return true;
}
static bool protocol(){Fixture f;FileWeb web(f.files,"TESTCODE");CHECK(!web.request("GET","/api/v1/list?shelf=0","wrong",nullptr,0) && web.status==403);auto w=file_fixture::wav();uint8_t sum[32];update::Sha256::hash(w.data(),w.size(),sum);char hex[65];for(unsigned i=0;i<32;++i)std::sprintf(hex+i*2,"%02x",sum[i]);std::string url=std::string("/api/v1/file?name=Hello%20World.wav&sha256=")+hex;CHECK(web.request("PUT",url.c_str(),"TESTCODE",w.data(),w.size()) && web.status==200 && f.fs.exists("ASSETS/SAMPLES/Hello World.wav"));CHECK(web.request("GET","/api/v1/file?path=ASSETS%2FSAMPLES%2FHello%20World.wav","TESTCODE",nullptr,0) && web.file_bytes==w.size());CHECK(web.request("DELETE","/api/v1/file?path=ASSETS%2FSAMPLES%2FHello%20World.wav","TESTCODE",nullptr,0) && !f.fs.exists("ASSETS/SAMPLES/Hello World.wav"));f.files.close();CHECK(!web.request("GET","/api/v1/list?shelf=0","TESTCODE",nullptr,0) && web.status==503);
 FileHttpHeader h;char request[]="PUT /api/v1/file?name=x.sf2 HTTP/1.1\r\nHost: groovebox.local\r\nCookie: MGX=TESTCODE\r\nContent-Length: 104857600\r\n\r\n";CHECK(h.parse(request) && h.length==104857600 && !std::strcmp(h.code,"TESTCODE"));
 char duplicate[]="POST /api/v1/cancel HTTP/1.1\r\nHost: local\r\nContent-Length: 0\r\nContent-Length: 1\r\n\r\n";h={};CHECK(!h.parse(duplicate));char chunked[]="PUT / HTTP/1.1\r\nHost: local\r\nTransfer-Encoding: chunked\r\n\r\n";h={};CHECK(!h.parse(chunked));return true;
}
// Fault model: each successful filesystem mutation is durable and atomic. It does
// not model torn FAT metadata/SD controller caches; physical cuts remain separate.
struct TransactionFiles:DesktopFiles {
 unsigned at=0,cut=0;bool crash=false;
 bool before(){return !cut || ++at!=cut || crash;}
 bool after(bool ok){if(ok && crash && cut && at==cut)throw std::runtime_error("power cut");return ok;}
 bool write(const char* p,uint64_t a,const void* b,unsigned n,bool t)override{return before() && after(DesktopFiles::write(p,a,b,n,t));}
 bool move(const char* a,const char* b)override{return before() && after(DesktopFiles::move(a,b));}
 bool remove(const char* p)override{return before() && after(DesktopFiles::remove(p));}
};
static bool syx_receive_recovery(){Fixture f;CHECK(f.fs.directory("PRESETS") && f.fs.directory("PRESETS/DX7"));const char bytes[]="interrupted bank";CHECK(f.fs.write("PRESETS/DX7/.MGX-TRANSFER.tmp",0,bytes,sizeof bytes,true));CHECK(f.fs.write("PRESETS/DX7/Keep.syx",0,bytes,sizeof bytes,true));CHECK(FileReplace::recover_all(f.fs));CHECK(!f.fs.exists("PRESETS/DX7/.MGX-TRANSFER.tmp") && f.fs.exists("PRESETS/DX7/Keep.syx"));return true;}
static bool replacement_recovery(){
 for(bool had_old:{false,true})for(bool crash:{false,true})for(unsigned cut=1;cut<=12;++cut){Fixture f;TransactionFiles fs;CHECK(fs.directory("ASSETS") && fs.directory("ASSETS/SAMPLES"));auto old=file_fixture::wav(),fresh=old;fresh.back()^=0x22;
  if(had_old){CHECK(fs.DesktopFiles::write("ASSETS/SAMPLES/Replace.wav",0,old.data(),old.size(),true));}CHECK(fs.DesktopFiles::write("ASSETS/SAMPLES/.MGX-TRANSFER.tmp",0,fresh.data(),fresh.size(),true));
  uint8_t sum[32];update::Sha256::hash(fresh.data(),fresh.size(),sum);fs.cut=cut;fs.crash=crash;FileReplace tx(fs);try{tx.replace("ASSETS/SAMPLES/.MGX-TRANSFER.tmp","ASSETS/SAMPLES/Replace.wav",fresh.size(),sum);}catch(const std::runtime_error&){}
  fs.cut=0;CHECK(FileReplace::recover_all(fs));CHECK(!FileReplace::pending(fs));
  CHECK(bytes_equal(fs,"ASSETS/SAMPLES/Replace.wav",fresh) || (had_old?bytes_equal(fs,"ASSETS/SAMPLES/Replace.wav",old):!fs.exists("ASSETS/SAMPLES/Replace.wav")));
  for(const char* name:{".MGX-TRANSFER.tmp",".MGX-REPLACE.plan",".MGX-REPLACE.plan.tmp",".MGX-REPLACE.old",".MGX-REPLACE.new"}){char path[96];CHECK(join_path(path,sizeof path,"ASSETS/SAMPLES",name) && !fs.exists(path));}
 }
 return true;
}
static bool dhcp_and_directories(){uint8_t input[300]{},out[320]{};input[0]=input[1]=1;input[2]=6;input[4]=42;input[28]=0xaa;std::memcpy(input+236,"\x63\x82\x53\x63",4);input[240]=53;input[241]=1;input[242]=1;input[243]=255;FileDhcp dhcp;CHECK(dhcp.reply(input,sizeof input,out,sizeof out,0)==300 && out[0]==2 && out[4]==42 && out[16]==192 && out[19]==2 && out[242]==2);input[242]=3;CHECK(dhcp.reply(input,sizeof input,out,sizeof out,0)==300 && out[242]==5);input[239]=0;CHECK(!dhcp.reply(input,sizeof input,out,sizeof out,0));
 Fixture f;CHECK(f.fs.directory("projects") && f.fs.directory("projects/mixed") && f.fs.write("projects/mixed/KeepName.wav",0,"abc",3,true));DataDirectoryReport report;CHECK(normalize_data_directories(f.fs,report) && report.complete && report.renamed==2 && f.fs.exists("PROJECTS/MIXED/KeepName.wav"));return true;
}
int main(){struct Test{const char* name;bool(*run)();};const Test tests[]={{"MGXC TYPE routing and byte-identical round trips",containers},{"WAV MIDI large SF2 and DX7 split",media},{"invalid input and good-file preservation",reject_and_keep},{"current references and path confinement",protection},{"busy disconnect storage corruption and expiry",cleanup},{"HTTP authentication cookies streaming headers and close",protocol},{"reserved SYX receive crash recovery",syx_receive_recovery},{"durable replacement crash and I/O boundary recovery",replacement_recovery},{"AP DHCP and directory-only normalization",dhcp_and_directories}};unsigned failed=0;for(const auto& test:tests){std::printf("START %s\n",test.name);std::fflush(stdout);bool ok=test.run();std::printf("%s %s\n",ok?"PASS":"FAIL",test.name);failed+=!ok;}return failed?1:0;}
