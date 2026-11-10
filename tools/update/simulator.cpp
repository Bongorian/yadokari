#include "update/fat_storage.h"
#include "update/receiver.h"
#include "update/view.h"
#include "update/preparation.h"
#include "update/pi4_policy.h"
#include "update/input.h"
#include "software_canvas.h"
#include <memory>
#include <diskio.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
using namespace groove::update;
static int disk_fd=-1;
extern "C" {
PARTITION VolToPart[4]={{0,1},{0,2},{0,3},{0,4}};
DSTATUS disk_status(BYTE p){return p?STA_NOINIT:0;}
DSTATUS disk_initialize(BYTE p){return disk_status(p);}
DRESULT disk_read(BYTE p,BYTE* out,LBA_t sector,UINT count){return !p&&pread(disk_fd,out,size_t(count)*512,off_t(sector)*512)==ssize_t(count)*512?RES_OK:RES_ERROR;}
DRESULT disk_write(BYTE p,const BYTE* in,LBA_t sector,UINT count){return !p&&pwrite(disk_fd,in,size_t(count)*512,off_t(sector)*512)==ssize_t(count)*512?RES_OK:RES_ERROR;}
DRESULT disk_ioctl(BYTE p,BYTE cmd,void* buffer){if(p)return RES_PARERR;if(cmd==CTRL_SYNC)return fsync(disk_fd)==0?RES_OK:RES_ERROR;if(cmd==GET_SECTOR_COUNT){struct stat s{};if(fstat(disk_fd,&s))return RES_ERROR;*static_cast<LBA_t*>(buffer)=LBA_t(s.st_size/512);return RES_OK;}if(cmd==GET_BLOCK_SIZE){*static_cast<DWORD*>(buffer)=1;return RES_OK;}if(cmd==GET_SECTOR_SIZE){*static_cast<WORD*>(buffer)=512;return RES_OK;}return RES_PARERR;}
}
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
class Disk:public BlockDevice {
public:
 explicit Disk(const char* path,bool create=false){disk_fd=open(path,O_RDWR|(create?(O_CREAT|O_EXCL):0),0600);check(disk_fd>=0,"Cannot open SD image (init never overwrites)");struct stat info{};check(!fstat(disk_fd,&info)&&S_ISREG(info.st_mode),"Only regular virtual SD image files are accepted");}
 ~Disk(){close(disk_fd);disk_fd=-1;}
 bool sector(unsigned lba,uint8_t* b,bool write)override {if(write)return pwrite(disk_fd,b,512,off_t(lba)*512)==512&&fsync(disk_fd)==0;return pread(disk_fd,b,512,off_t(lba)*512)==512;}
};
class MockReboot:public Rebooter {public:bool allowed=true;unsigned partition=0;bool tryboot=false;bool supported()const override{return allowed;}bool restart(unsigned p,bool trial)override{partition=p;tryboot=trial;return true;}};
static std::vector<uint8_t> read_file(const char* path){FILE* f=fopen(path,"rb");check(f,"Cannot read file");check(!fseek(f,0,SEEK_END),"File seek");long n=ftell(f);check(n>=0&&n<=16*1024*1024,"Oversized host file");rewind(f);std::vector<uint8_t> b(static_cast<size_t>(n));check(fread(b.data(),1,b.size(),f)==b.size(),"Short host read");fclose(f);return b;}
static void fat_write(const char* path,const std::vector<uint8_t>& bytes){
 std::string name(path);for(size_t i=3;i<name.size();++i)if(name[i]=='/'){std::string directory=name.substr(0,i);FRESULT result=f_mkdir(directory.c_str());check(result==FR_OK||result==FR_EXIST,"FAT directory");}
 FIL f;check(f_open(&f,path,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"FAT open write");UINT count=0;check(f_write(&f,bytes.data(),UINT(bytes.size()),&count)==FR_OK&&count==bytes.size(),"FAT write");check(f_sync(&f)==FR_OK,"FAT sync");check(f_close(&f)==FR_OK,"FAT close");}
static std::vector<uint8_t> fat_read(const char* path){FIL f;check(f_open(&f,path,FA_READ)==FR_OK,"FAT open read");std::vector<uint8_t> b(f_size(&f));UINT n=0;check(f_read(&f,b.data(),UINT(b.size()),&n)==FR_OK&&n==b.size(),"FAT read");check(f_close(&f)==FR_OK,"FAT close read");return b;}
static void initialize(Disk& disk,const std::vector<uint8_t>& image,uint32_t version,unsigned data_mib=64){
 check(data_mib>=64&&data_mib<=32768,"DATA size must be 64..32768 MiB");
 check(!image.empty()&&image.size()<=MaxImage&&version,"Invalid factory image");
 check(!ftruncate(disk_fd,off_t(2048+3*131072+uint64_t(data_mib)*2048)*512),"Create sparse image");uint8_t b[512]{};
 for(unsigned i=0;i<4;++i){auto* p=b+446+16*i;p[4]=0x0c;put32(p+8,2048+i*131072);put32(p+12,i==3?data_mib*2048:131072);}b[510]=0x55;b[511]=0xaa;check(disk.sector(0,b,true),"MBR write");
 uint8_t marker[512]={'G','S','D','1'};Sha256::hash(b+446,64,marker+4);check(disk.sector(8,marker,true),"Layout marker");
 uint8_t work[65536];FATFS fs[4]{};MKFS_PARM options{FM_FAT32,0,0,0,0};
 for(unsigned i=0;i<4;++i){char path[3]={char('0'+i),':',0};check(f_mkfs(path,&options,work,sizeof work)==FR_OK,"Format virtual FAT32");check(f_mount(fs+i,path,1)==FR_OK,"Mount virtual FAT32");}
 fat_write("0:/RECOVER.DAT",{2,4,6,8});fat_write("1:/KERNEL.IMG",image);fat_write("3:/PROTECT.DAT",{1,3,3,7,9});
 State state;state.sequence=1;state.highest=version;state.slots[0].version=version;state.slots[0].size=uint32_t(image.size());Sha256::hash(image.data(),image.size(),state.slots[0].hash);encode_state(state,b);check(disk.sector(32,b,true),"Initial journal");state.sequence=2;encode_state(state,b);check(disk.sector(16,b,true),"Redundant journal");
 for(unsigned i=0;i<4;++i){char path[3]={char('0'+i),':',0};check(f_mount(nullptr,path,0)==FR_OK,"Unmount format");}
}
static bool install(Manager& m,const std::vector<uint8_t>& data,unsigned cut=0xffffffff){Receiver receiver(m);if(data.size()<HeaderSize)return false;int result=receiver.feed(data.data(),HeaderSize);if(result!=1)return false;unsigned offset=HeaderSize;while(offset<data.size()&&offset<cut){unsigned n=receiver.wanted();if(n>data.size()-offset)n=unsigned(data.size()-offset);result=receiver.feed(data.data()+offset,n);offset+=n;if(result<0)return false;}if(offset!=data.size()){receiver.reset();return false;}return result==3;}
class FaultStorage:public Storage {
 Storage& base_;
public:
 bool fail_write=false,corrupt_read=false,torn_journal=false;
 explicit FaultStorage(Storage& base):base_(base){}
 bool read_journal(unsigned c,uint8_t b[512])override{return base_.read_journal(c,b);}
 bool write_journal(unsigned c,const uint8_t b[512])override{if(!torn_journal)return base_.write_journal(c,b);uint8_t old[512];if(!base_.read_journal(c,old))return false;for(unsigned i=0;i<128;++i)old[i]=b[i];base_.write_journal(c,old);return false;}
 bool open_image(unsigned s,bool w)override{return base_.open_image(s,w);}
 bool write_image(const uint8_t* p,unsigned n)override{return !fail_write&&base_.write_image(p,n);}
 bool read_image(uint8_t* p,unsigned n,unsigned& got)override{bool ok=base_.read_image(p,n,got);if(corrupt_read&&got)p[0]^=1;return ok;}
 bool close_image()override{return base_.close_image();}
};
static void exercise(Disk& disk,FatStorage& storage,const uint8_t key[32],const std::vector<uint8_t>& package){
 PadLatch pads_down;pads_down.preset(1u<<9);check(!pads_down.press(9,0x10)&&!pads_down.press(9,0x20)&&pads_down.press(9,0x10)&&!pads_down.press(9,0x10)&&!pads_down.press(9,0x40)&&!pads_down.press(9,0x30)&&pads_down.press(9,0x10),"Update actions require fresh pad presses across mode handoff");
 check(!pi4_tryboot_allowed(0,true)&&!pi4_tryboot_allowed(0xa03110,false)&&!pi4_tryboot_allowed(0xa03111,false)&&pi4_tryboot_allowed(0xa03111,true)&&pi4_tryboot_allowed(0xa03112,false)&&!pi4_tryboot_allowed(0xa03130,true),"Pi4 revision and EEPROM acknowledgement gate");
 uint32_t flags[4],notify[4],query[4];reboot_flags_tag(flags,true);reboot_notify_tag(notify);reboot_flags_query_tag(query);check(flags[0]==0x38064&&flags[1]==4&&flags[2]==4&&flags[3]==1&&notify[0]==0x30048&&notify[1]==0&&notify[2]==0&&notify[3]==0&&query[0]==0x30064&&query[1]==4&&query[2]==0&&query[3]==0,"Mailbox request shape including query and zero-payload notify");
 check(!journal_trial_expired(true,true,119999999,0)&&journal_trial_expired(true,true,120000000,0)&&!journal_trial_expired(true,false,120000000,0)&&!journal_trial_expired(false,true,120000000,0)&&journal_trial_expired(true,true,119999984u,0xfffffff0u),"Only an unconfirmed trial times out, including timer wrap");
 struct Gate:Preparation {bool stop_ok=true,save_ok=true,entered=false;unsigned calls=0;bool stop_and_quiesce()override{check(++calls==1,"Stop must precede save");return stop_ok;}bool save_project()override{check(++calls==2,"Save ordering");return save_ok;}void enter_update_mode()override{check(++calls==3,"Arm ordering");entered=true;}};
 Gate failed_stop;failed_stop.stop_ok=false;check(!failed_stop.run()&&!failed_stop.entered&&failed_stop.calls==1,"Audio stop failure gate");Gate failed_save;failed_save.save_ok=false;check(!failed_save.run()&&!failed_save.entered&&failed_save.calls==2,"Project save failure gate");Gate accepted;check(accepted.run()&&accepted.entered,"Update mode entry");
 MockReboot boot;FaultStorage fault(storage);Manager m(fault,boot,key);check(m.load(),"Load factory journal");uint8_t baseline[2][512];for(unsigned c=0;c<2;++c)check(disk.sector(c?32:16,baseline[c],false),"Read baseline");auto restore=[&](){for(unsigned c=0;c<2;++c)check(disk.sector(c?32:16,baseline[c],true),"Restore test journal");fault.fail_write=fault.corrupt_read=fault.torn_journal=false;check(m.load(),"Reload baseline");m.arm(true);};
 auto original=fat_read("1:/KERNEL.IMG"),recovery=fat_read("0:/RECOVER.DAT");check(!install(m,package)&&m.error()==Error::NotArmed,"Local update gate");restore();
 uint8_t no_key[32]{};Manager unkeyed(storage,boot,no_key);check(unkeyed.load(),"Unkeyed load");unkeyed.arm(true);check(!unkeyed.begin(package.data())&&unkeyed.error()==Error::NoKey,"No default trust key");
 auto bad=package;bad[140]^=1;check(!install(m,bad)&&m.error()==Error::Signature,"Reject signature");bad=package;bad.back()^=1;check(!install(m,bad)&&m.error()==Error::Hash,"Reject payload");restore();
 check(!install(m,package,HeaderSize+4096),"Interrupted receive");check(m.load()&&m.on_boot(1,false,1)&&m.state().active==0&&m.state().phase==Phase::Stable,"Power loss rollback");restore();
 fault.fail_write=true;check(!install(m,package)&&m.error()==Error::Storage,"Disk full/write fault");restore();fault.corrupt_read=true;check(!install(m,package)&&m.error()==Error::Hash,"Readback corruption");restore();
 fault.torn_journal=true;check(!install(m,package)&&m.error()==Error::Storage,"Torn journal");restore();
 check(install(m,package),"Signed package installation");uint8_t pads[16],buttons[62];controller_leds(m,false,false,pads,buttons);check(pads[9]!=0&&buttons[41]==0&&buttons[43]==0x9e,"Update LEDs match Ready and stopped transport");controller_leds(m,true,false,pads,buttons);check(pads[9]==0&&pads[3]!=0,"Busy update leaves cancel available");check(m.cancel()&&m.state().phase==Phase::Stable&&m.state().active==0,"Cancel staged update without switching active");check(install(m,package),"Reinstall cancelled package");boot.allowed=false;check(!m.trial()&&m.error()==Error::BootUnsupported,"Unsafe board gate");boot.allowed=true;check(m.trial()&&boot.partition==3&&boot.tryboot,"Tryboot handoff");check(m.load()&&m.on_boot(1,false,1)&&m.state().active==0,"Crash rollback to previous active");
 check(install(m,package),"Retry failed version");check(m.trial(),"Trial requested");check(!m.on_boot(2,true,2)&&m.rejected_trial(2),"Wrong partition cannot be a journal trial");check(m.load()&&!m.on_boot(3,false,1)&&m.rejected_trial(3)&&!m.rejected_trial(1),"Wrong running version schedules recovery only from app slot");check(m.load()&&m.on_boot(3,false,2)&&!m.rejected_trial(3),"Journal trial identity without hardware flag");check(!m.confirm(2,true,true),"Wrong slot cannot commit");check(!m.confirm(3,true,false),"Health gate");check(m.confirm(3,true,true)&&m.state().active==1,"Healthy trial commit");check(!install(m,package)&&m.error()==Error::Version,"Anti-downgrade");check(m.rollback_confirmed()&&m.state().active==0&&m.state().highest==2,"Manual rollback retains anti-downgrade counter");check(m.rollback_confirmed()&&m.state().active==1,"Return to verified second slot");check(m.boot_active()&&boot.partition==3&&!boot.tryboot,"Confirmed boot");check(m.on_boot(1,false,1)&&m.state().phase==Phase::Recovery,"Bounded boot failure enters recovery");
 restore();check(install(m,package)&&m.trial(),"Prepare damaged-slot rollback");uint8_t boot_sector[512],destroyed[512]{};const unsigned b_start=2048+2*131072;check(disk.sector(b_start,boot_sector,false)&&disk.sector(b_start,destroyed,true),"Damage inactive FAT boot sector");check(storage.mount()&&m.load()&&m.on_boot(1,false,1)&&m.boot_active()&&boot.partition==2&&!boot.tryboot,"Unmountable candidate must not block rollback to A");check(disk.sector(b_start,boot_sector,true)&&storage.mount(),"Restore simulated candidate FAT");
 check(fat_read("1:/KERNEL.IMG")==original,"Active image preserved");check(fat_read("0:/RECOVER.DAT")==recovery,"Independent recovery preserved");FATFS data;check(f_mount(&data,"3:",1)==FR_OK,"Mount protected data");check(fat_read("3:/PROTECT.DAT")==std::vector<uint8_t>({1,3,3,7,9}),"User data preserved");f_mount(nullptr,"3:",0);
 uint8_t digest[32];Sha256::hash(reinterpret_cast<const uint8_t*>("abc"),3,digest);const uint8_t expected[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};check(equal(digest,expected,32),"SHA256 known answer");std::puts("update: real MBR/FAT image, Ed25519, receive/hash/readback, disk faults, torn journal, tryboot/rollback/retry/confirm, anti-downgrade and protected data PASS");
}
static void serve(Manager& manager,int channel){Receiver receiver(manager);uint8_t buffer[4096];while(!receiver.done()){pollfd wait{channel,POLLIN,0};check(poll(&wait,1,30000)>0,"Transport timeout");ssize_t n=read(channel,buffer,receiver.wanted());if(n<=0){receiver.reset();throw std::runtime_error("Transport disconnected");}int result=receiver.feed(buffer,unsigned(n));if(!result)continue;std::string response=result==1?"READY\n":result==2?"OK\n":result==3?"VERIFIED\n":std::string("ERROR ")+error_name(manager.error())+"\n";check(write(channel,response.data(),response.size())==ssize_t(response.size()),"Transport reply");if(result<0)throw std::runtime_error("Rejected package");}}
static void cycle(Manager& manager,MockReboot& reboot,const std::vector<uint8_t>& second,const std::vector<uint8_t>& third){
 check(install(manager,second)&&manager.trial()&&manager.on_boot(3,true,2)&&manager.confirm(3,true,true),"A to B update");
 auto protected_b=fat_read("2:/KERNEL.IMG");
 check(install(manager,third)&&manager.state().pending==0&&manager.state().active==1,"Write inactive A while B remains active");
 check(manager.trial()&&reboot.partition==2&&manager.on_boot(1,false,1)&&manager.state().active==1&&manager.state().highest==2,"Failed A rolls back to B");
 check(install(manager,third)&&manager.trial()&&manager.on_boot(2,true,3)&&manager.confirm(2,true,true)&&manager.state().active==0&&manager.state().highest==3,"Retry and confirm B to A update");
 check(fat_read("2:/KERNEL.IMG")==protected_b,"Active B preserved during A write and retry");
 std::puts("update cycle: A1 -> B2 -> failed A3 -> B2 -> retry A3 confirmed; active B bytes preserved PASS");
}
int main(int argc,char** argv){try{
 check(argc>=3,"Usage: update-sim init IMAGE FACTORY VERSION | put/get IMAGE FATPATH HOSTFILE | exercise/install/serve/serial IMAGE PUBLIC PACKAGE_OR_PORT_OR_FD | status IMAGE");std::string command=argv[1];Disk disk(argv[2],command=="init");
 if(command=="init"){check(argc==5||argc==6,"init arguments");initialize(disk,read_file(argv[3]),uint32_t(std::stoul(argv[4])),argc==6?unsigned(std::stoul(argv[5])):64);return 0;}
 FatStorage storage(disk,"0:","1:","2:");check(storage.mount(),"Invalid layout or unmountable FAT");MockReboot reboot;
 if(command=="put"||command=="get"){check(argc==5,"put/get arguments");FATFS data;bool extra=argv[3][0]=='3';if(extra)check(f_mount(&data,"3:",1)==FR_OK,"Mount data");if(command=="put")fat_write(argv[3],read_file(argv[4]));else{auto bytes=fat_read(argv[3]);FILE* f=fopen(argv[4],"wb");check(f,"Output file");check(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size(),"Output write");fclose(f);}return 0;}
 uint8_t key[32]{};if(command!="status"&&command!="screen"){check(argc==(command=="cycle"?6:5),"Command arguments");auto bytes=read_file(argv[3]);check(bytes.size()==32,"Public key length");memcpy(key,bytes.data(),32);}Manager manager(storage,reboot,key);check(manager.load(),"No valid update journal");manager.arm(true);
 if(command=="exercise")exercise(disk,storage,key,read_file(argv[4]));
 else if(command=="cycle")cycle(manager,reboot,read_file(argv[4]),read_file(argv[5]));
 else if(command=="install")check(install(manager,read_file(argv[4])),error_name(manager.error()));
 else if(command=="serial")serve(manager,std::stoi(argv[4]));
 else if(command=="serve"){int sock=socket(AF_INET,SOCK_STREAM,0);check(sock>=0,"Socket");sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(uint16_t(std::stoi(argv[4])));check(!bind(sock,reinterpret_cast<sockaddr*>(&address),sizeof address)&&!listen(sock,1),"Listen");socklen_t size=sizeof address;check(!getsockname(sock,reinterpret_cast<sockaddr*>(&address),&size),"Get port");printf("PORT %u\n",ntohs(address.sin_port));fflush(stdout);int client=accept(sock,nullptr,nullptr);check(client>=0,"Accept");serve(manager,client);close(client);close(sock);}
 else if(command=="screen"){check(argc==4,"screen arguments");auto canvas=std::make_unique<groove::SoftwareCanvas>();draw_view(*canvas,manager,"HOST UPDATE SIMULATION",1,false,0);FILE* f=fopen(argv[3],"wb");check(f,"Screen output");fprintf(f,"P6\n960 272\n255\n");for(unsigned y=0;y<272;++y)for(unsigned x=0;x<960;++x){uint32_t pixel=canvas->pixels[x/480][y*480+x%480];uint8_t rgb[3]={uint8_t(pixel>>16),uint8_t(pixel>>8),uint8_t(pixel)};check(fwrite(rgb,1,3,f)==3,"Screen write");}fclose(f);}
 else if(command!="status")throw std::runtime_error("Unknown command");
 check(manager.load(),"Reload status");
 printf("active=%u version=%u phase=%u sequence=%u history=%u\n",manager.state().active,manager.state().slots[manager.state().active].version,unsigned(manager.state().phase),manager.state().sequence,manager.state().count);return 0;
 }catch(const std::exception& e){fprintf(stderr,"update-sim: %s\n",e.what());return 1;}}
