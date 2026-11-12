#include "../src/update/project_snapshot.h"
#include "../src/content_shelf.h"
#include <circle/bcmrandom.h>
#include <circle/sched/scheduler.h>
#include "../src/project_location.h"
#include "project_storage.h"
#include "../src/obsolete_content.h"
#include "boot_trace_config.h"
#if GROOVE_BOOT_TRACE
#define BOOT_STORAGE(op,name) do {if(m_InstallingFactory)CLogger::Get()->Write("factory-io",LogNotice,"%s %s",op,name);} while(0)
#else
#define BOOT_STORAGE(op,name) do {} while(0)
#endif
#include "storage_ui_wait.h"
extern "C" void GrooveStorageUiWait(){StorageUiWait::poll();}
#include "../src/factory_content.h"
#include "../src/factory_boot_gate.h"
#include "update_runtime.h"
#include "../build/embedded_demo_sample.h"
#include "../src/alloc_policy.h"
#include "../src/sample_cache_promotion.h"
#include <circle/memory.h>
#include "../src/session_files.h"
#include "../src/benchmark_cleanup_policy.h"
#include "../src/data_directories.h"
#include "../src/file_replace.h"
#include "../src/benchmark_project.h"
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>
CProjectStorage::~CProjectStorage(){delete[] m_CpuIo;end_read_session();CloseRecordingWrite();cancel_incremental_load();while(m_SoundFontWorker && m_SoundFontWorker->busy())ServiceSoundFont();CloseSoundFontRead();delete m_ChunkedLoad;for(auto& c:m_Cache){delete c.stream;if(c.high)CMemorySystem::HeapFree(c.bytes);else delete[] c.bytes;}}
bool CProjectStorage::Mount(){m_Drive=CDeviceNameService::Get()->GetDevice("emmc1-4",TRUE)?"DATA:":"SD:";m_Mounted=f_mount(&m_Fs,m_Drive,1)==FR_OK;return m_Mounted;}
bool CProjectStorage::Absolute(const char* path,char* out,unsigned capacity)const{char clean[192];if(!m_Mounted || *path=='/' || !groove::canonical_path(clean,sizeof clean,"",path))return false;return groove::join_path(out,capacity,m_Drive,clean);}
unsigned CProjectStorage::list(const char* name,groove::FileEntry* result,unsigned capacity){char path[208];if(!Absolute(name,path,sizeof path))return 0;DIR dir;if(f_opendir(&dir,path)!=FR_OK)return 0;FILINFO info;unsigned n=0;while(n<capacity && f_readdir(&dir,&info)==FR_OK && info.fname[0]){if(info.fname[0]=='.' || (info.fattrib&AM_HID))continue;if(!groove::data_browser_entry(name,info.fname,(info.fattrib&AM_DIR)!=0))continue;auto& e=result[n];if(!groove::copy_text(e.name,sizeof e.name,info.fname))continue;e.directory=(info.fattrib&AM_DIR)!=0;e.bytes=info.fsize;++n;}f_closedir(&dir);for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j)if((result[j].directory && !result[i].directory) || (result[i].directory==result[j].directory && strcmp(result[j].name,result[i].name)<0)){auto tmp=result[i];result[i]=result[j];result[j]=tmp;}return n;}
bool CProjectStorage::list_checked(const char* name,groove::FileEntry* out,unsigned capacity,unsigned& count){
 count=0;char path[208];if(!Absolute(name,path,sizeof path))return false;DIR dir;if(f_opendir(&dir,path)!=FR_OK)return false;bool ok=true;FILINFO info;
 for(;;){if(f_readdir(&dir,&info)!=FR_OK){ok=false;break;}if(!info.fname[0])break;if(!strcmp(info.fname,".") || !strcmp(info.fname,".."))continue;
  if(count==capacity || !groove::copy_text(out[count].name,sizeof out[count].name,info.fname)){ok=false;break;}out[count].directory=(info.fattrib&AM_DIR)!=0;out[count++].bytes=info.fsize;
 }
 return f_closedir(&dir)==FR_OK && ok;
}
bool CProjectStorage::begin_read_session(const char* name){
 if(m_ReadOpen)return false;char path[208];if(!Absolute(name,path,sizeof path) || !CloseRecordingWrite(path) || !groove::copy_text(m_ReadPath,sizeof m_ReadPath,name) || f_open(&m_ReadFile,path,FA_READ)!=FR_OK)return false;m_ReadOpen=true;return true;
}
void CProjectStorage::end_read_session(){if(m_ReadOpen){f_close(&m_ReadFile);m_ReadOpen=false;m_ReadPath[0]=0;}}
bool CProjectStorage::read(const char* name,uint64_t offset,void* data,unsigned wanted,unsigned& got){
#ifdef GROOVE_CPU_HEAP
 // Files may be encoded from CPU-owned objects. DMA must only see LOW buffers.
 if(CMemorySystem::IsCpuHeapPointer(data)){
  got=0;if(!m_CpuIo)m_CpuIo=groove::alloc_array<u8>(65536);if(!m_CpuIo)return false;
  while(got<wanted){unsigned n=wanted-got;if(n>65536)n=65536;unsigned read_count=0;if(!read(name,offset+got,m_CpuIo,n,read_count))return false;memcpy(static_cast<u8*>(data)+got,m_CpuIo,read_count);got+=read_count;if(read_count<n)break;}return true;
 }
#endif
if(m_ReadOpen && !strcmp(name,m_ReadPath)){if(m_Watchdog)m_Watchdog->Start();got=0;bool ok=(f_tell(&m_ReadFile)==offset || f_lseek(&m_ReadFile,offset)==FR_OK) && f_read(&m_ReadFile,data,wanted,&got)==FR_OK;if(ok && got){if(m_Watchdog)m_Watchdog->Start();CScheduler::Get()->Yield();}return ok;}BOOT_STORAGE("read",name);if(m_Watchdog)m_Watchdog->Start();got=0;char path[208];if(!Absolute(name,path,sizeof path) || !CloseRecordingWrite(path))return false;FIL file;if(f_open(&file,path,FA_READ)!=FR_OK)return false;bool ok=f_lseek(&file,offset)==FR_OK && f_read(&file,data,wanted,&got)==FR_OK;return f_close(&file)==FR_OK && ok;}
bool CProjectStorage::CloseRecordingWrite(const char* path){
 if(!m_RecordOpen || (path && strcmp(path,m_RecordPath)))return true;
 StorageUiWait::Scope responsive;bool ok=true;if(m_RecordReserved)ok=f_lseek(&m_RecordFile,m_RecordEnd)==FR_OK && f_truncate(&m_RecordFile)==FR_OK;ok=f_close(&m_RecordFile)==FR_OK && ok;m_RecordReserved=false;m_RecordOpen=false;m_RecordPath[0]=0;return ok;
}
bool CProjectStorage::begin_recording(const char* name,const void* header,unsigned bytes,uint64_t reserve){
 char path[208];if(reserve<bytes || reserve>=0x100000000ull || !Absolute(name,path,sizeof path) || !groove::data_suffix(name,".part") || !CloseRecordingWrite())return false;
 if(f_open(&m_RecordFile,path,FA_WRITE|FA_CREATE_NEW)!=FR_OK)return false;
 strcpy(m_RecordPath,path);m_RecordOpen=true;m_RecordReserved=true;m_RecordEnd=0;m_RecordLastStartUs=m_RecordSyncUs=0;m_RecordWriteMaxUs=m_RecordGapMaxUs=m_RecordSyncMaxUs=m_RecordSeekMaxUs=m_RecordCalls=0;
 StorageUiWait::Scope responsive;uint64_t started=CTimer::GetClockTicks64();bool contiguous=f_expand(&m_RecordFile,reserve,1)==FR_OK;
 bool ok=contiguous || (f_lseek(&m_RecordFile,reserve)==FR_OK && f_tell(&m_RecordFile)==reserve);
 CLogger::Get()->Write("record",LogNotice,"Record reserve bytes %u contiguous %u elapsed-us %u ok %u",unsigned(reserve),unsigned(contiguous),unsigned(CTimer::GetClockTicks64()-started),unsigned(ok));
 if(ok)ok=write(name,0,header,bytes,false);if(!ok){CloseRecordingWrite();f_unlink(path);}return ok;
}
bool CProjectStorage::finish_recording(const char* name,const void* header,unsigned bytes,uint64_t length){
 if(!write(name,0,header,bytes,false))return false;m_RecordEnd=length;return CloseRecordingWrite();
}
bool CProjectStorage::write(const char* name,uint64_t offset,const void* data,unsigned count,bool truncate){
#ifdef GROOVE_CPU_HEAP
 if(CMemorySystem::IsCpuHeapPointer(data)){
  if(!m_CpuIo)m_CpuIo=groove::alloc_array<u8>(65536);if(!m_CpuIo)return false;
  unsigned at=0;do{unsigned n=count-at;if(n>65536)n=65536;memcpy(m_CpuIo,static_cast<const u8*>(data)+at,n);if(!write(name,offset+at,m_CpuIo,n,truncate && !at))return false;at+=n;}while(at<count);return true;
 }
#endif
end_read_session();BOOT_STORAGE("write",name);
 if(!m_InstallingFactory && groove::factory_path(name))return false;
 if(m_Watchdog)m_Watchdog->Start();char path[208];if(!Absolute(name,path,sizeof path))return false;
 if(groove::data_suffix(name,".part")){
  StorageUiWait::Scope responsive;
  if(m_RecordOpen && (truncate || strcmp(path,m_RecordPath)) && !CloseRecordingWrite())return false;
  if(!m_RecordOpen){if(f_open(&m_RecordFile,path,FA_WRITE|(truncate?FA_CREATE_ALWAYS:FA_OPEN_EXISTING))!=FR_OK)return false;strcpy(m_RecordPath,path);m_RecordOpen=true;}
  uint64_t start=CTimer::GetClockTicks64();if(truncate){m_RecordLastStartUs=0;m_RecordWriteMaxUs=m_RecordGapMaxUs=m_RecordSyncMaxUs=m_RecordSeekMaxUs=m_RecordCalls=0;}
  unsigned gap=m_RecordLastStartUs?unsigned(start-m_RecordLastStartUs):0;m_RecordLastStartUs=start;if(gap>m_RecordGapMaxUs)m_RecordGapMaxUs=gap;
  unsigned written=0;bool ok=f_tell(&m_RecordFile)==offset || f_lseek(&m_RecordFile,offset)==FR_OK;uint64_t sought=CTimer::GetClockTicks64();if(ok)ok=f_write(&m_RecordFile,data,count,&written)==FR_OK && written==count;uint64_t wrote=CTimer::GetClockTicks64();if(ok && (!offset || wrote-m_RecordSyncUs>=1000000)){ok=f_sync(&m_RecordFile)==FR_OK;m_RecordSyncUs=CTimer::GetClockTicks64();}uint64_t synced=CTimer::GetClockTicks64();
  unsigned seek_us=unsigned(sought-start),write_us=unsigned(wrote-sought),sync_us=unsigned(synced-wrote);bool peak=write_us>m_RecordWriteMaxUs+4000 || sync_us>m_RecordSyncMaxUs+4000;
  if(seek_us>m_RecordSeekMaxUs)m_RecordSeekMaxUs=seek_us;if(write_us>m_RecordWriteMaxUs)m_RecordWriteMaxUs=write_us;if(sync_us>m_RecordSyncMaxUs)m_RecordSyncMaxUs=sync_us;++m_RecordCalls;
  if(peak || offset==0)CLogger::Get()->Write("record",LogNotice,"Record IO calls %u bytes %u offset %u seek-max-us %u write-max-us %u sync-max-us %u gap-max-us %u ok %u",m_RecordCalls,count,unsigned(offset),m_RecordSeekMaxUs,m_RecordWriteMaxUs,m_RecordSyncMaxUs,m_RecordGapMaxUs,unsigned(ok));
  if(ok && offset+count>m_RecordEnd)m_RecordEnd=offset+count;m_RecordWriteMs=CTimer::GetClockTicks64()/1000;if(!ok)CloseRecordingWrite();return ok;
 }
 FIL file;if(f_open(&file,path,FA_WRITE|(truncate?FA_CREATE_ALWAYS:FA_OPEN_EXISTING))!=FR_OK)return false;unsigned written=0;bool ok=f_lseek(&file,offset)==FR_OK && f_write(&file,data,count,&written)==FR_OK && written==count && f_sync(&file)==FR_OK;return f_close(&file)==FR_OK && ok;
}
bool CProjectStorage::move(const char* from,const char* to){end_read_session();BOOT_STORAGE("rename",from);if(!m_InstallingFactory && (groove::factory_path(from) || groove::factory_path(to)))return false;char a[208],b[208];return Absolute(from,a,sizeof a) && Absolute(to,b,sizeof b) && CloseRecordingWrite(a) && CloseRecordingWrite(b) && f_rename(a,b)==FR_OK;}
bool CProjectStorage::remove(const char* name){end_read_session();BOOT_STORAGE("remove",name);if(!m_InstallingFactory && groove::factory_path(name))return false;char path[208];if(!Absolute(name,path,sizeof path) || !CloseRecordingWrite(path))return false;FRESULT r=f_unlink(path);return r==FR_OK || r==FR_NO_FILE;}
bool CProjectStorage::directory(const char* name){BOOT_STORAGE("directory",name);char path[208];if(!Absolute(name,path,sizeof path))return false;FILINFO info;if(f_stat(path,&info)==FR_OK)return (info.fattrib&AM_DIR)!=0;return f_mkdir(path)==FR_OK;}
bool CProjectStorage::exists(const char* name){BOOT_STORAGE("exists",name);char path[208];FILINFO info;return Absolute(name,path,sizeof path) && f_stat(path,&info)==FR_OK;}
uint64_t CProjectStorage::size(const char* name){BOOT_STORAGE("size",name);char path[208];FILINFO info;return Absolute(name,path,sizeof path) && f_stat(path,&info)==FR_OK?info.fsize:0;}
bool CProjectStorage::same_reference(const char* first,const char* second){
 if(m_Watchdog)m_Watchdog->Start();
 struct Key {LBA_t sector;unsigned offset;};
 auto identify=[&](const char* name,Key& key){char path[208];if(!Absolute(name,path,sizeof path))return 0;FIL file;FRESULT result=f_open(&file,path,FA_READ);if(result==FR_NO_FILE || result==FR_NO_PATH)return -1;if(result!=FR_OK)return 0;
  uintptr_t pointer=reinterpret_cast<uintptr_t>(file.dir_ptr),window=reinterpret_cast<uintptr_t>(file.obj.fs->win);bool valid=pointer>=window && pointer-window<sizeof file.obj.fs->win;key={file.dir_sect,unsigned(pointer-window)};bool closed=f_close(&file)==FR_OK;return valid && closed?1:0;
 };
 Key a{},b{};int left=identify(first,a);if(left<0)return false;if(!left)return true;int right=identify(second,b);if(right<0)return false;return !right || (a.sector==b.sector && a.offset==b.offset);
}
bool CProjectStorage::random_bytes(uint8_t* out,unsigned count){CBcmRandomNumberGenerator random;for(unsigned at=0;at<count;){uint32_t value=random.GetNumber();for(unsigned b=0;b<4 && at<count;++b)out[at++]=uint8_t(value>>(b*8));}return true;}
bool CProjectStorage::revision(const char* name,groove::FileRevision& r){BOOT_STORAGE("stat",name);char path[208];FILINFO info;if(!Absolute(name,path,sizeof path) || f_stat(path,&info)!=FR_OK || (info.fattrib&AM_DIR))return false;r.size=info.fsize;r.identity=0;r.modified=info.fdate;r.detail=info.ftime;return true;}
bool CProjectStorage::load(const char* name,groove::WavView& out){return load_impl(name,out,false);}
bool CProjectStorage::load_twin(const char* name,groove::WavView& out){return load_impl(name,out,true);}
bool CProjectStorage::load_impl(const char* name,groove::WavView& out,bool resident){if(m_SoundFontWorker && m_SoundFontWorker->busy())return false;if(groove::soundfont_file_header(*this,name)){SoundFontFiles input(*this);bool ok=m_SoundFonts.load(input,name,out);CloseSoundFontRead();return ok;}uint64_t bytes=size(name);if(bytes<44 || bytes>1024u*1024u*1024u)return false;for(unsigned i=m_Count;i>0;--i){auto& cached=m_Cache[i-1];if(!strcmp(cached.path,name) && ((!resident && cached.stream && cached.stream->current()) || (!cached.stream && groove::cached_file_matches(*this,name,cached.bytes,cached.length)))){out=cached.view;return true;}}if(m_Count==128)return false;auto& c=m_Cache[m_Count];if(!groove::copy_text(c.path,sizeof c.path,name))return false;if(!resident && bytes>8*1024*1024){if(m_Streams==64)return false;c.stream=groove::alloc_new<groove::SampleStream>(*this);if(!c.stream || !c.stream->open(name,c.view)){delete c.stream;c.stream=nullptr;return false;}++m_Streams;}else{if(m_Bytes+bytes>256*1024*1024)return false;c.bytes=groove::alloc_array<u8>(unsigned(bytes));unsigned got=0;bool ok=c.bytes && read(name,0,c.bytes,unsigned(bytes),got) && got==bytes && groove::parse_wav(c.bytes,unsigned(bytes),c.view);if(!ok){delete[] c.bytes;c.bytes=nullptr;return false;}
 // FatFs reads into DMA-safe low memory; publish the immutable sample from
 // high memory only after a complete copy and a second parse.
 c.high=groove::promote_sample_cache(c.bytes,unsigned(bytes),c.view,[](unsigned n)->void*{void* p=CMemorySystem::HeapTryAllocate(n,HEAP_CPU);return p?p:CMemorySystem::HeapTryAllocate(n,HEAP_HIGH);},[](u8* p){delete[] p;},[](u8* p){CMemorySystem::HeapFree(p);});
 m_Bytes+=unsigned(bytes);c.length=unsigned(bytes);}out=c.view;++m_Count;return true;}
bool CProjectStorage::PendingRevisionCurrent(){groove::FileRevision now;return !m_PendingLoad.has_revision || (revision(m_PendingLoad.path,now) && groove::same_file_revision(m_PendingLoad.revision,now));}
void CProjectStorage::cancel_incremental_load(){if(m_PendingLoad.phase==LoadPhase::SoundFont && m_SoundFontWorker)m_SoundFontWorker->cancel();if(m_ChunkedLoad)m_ChunkedLoad->clear();delete[] m_PendingLoad.low;if(m_PendingLoad.high)CMemorySystem::HeapFree(m_PendingLoad.high);m_PendingLoad=PendingLoad{};}
bool CProjectStorage::begin_incremental_load(const char* name,uint32_t expected_crc){
 if(m_PendingLoad.phase!=LoadPhase::Idle || (m_SoundFontWorker && m_SoundFontWorker->busy()) || !name){return false;}uint64_t bytes=size(name);if(bytes<44 || bytes>1024u*1024u*1024u || !groove::copy_text(m_PendingLoad.path,sizeof m_PendingLoad.path,name))return false;
 if(groove::soundfont_file_header(*this,name)){m_PendingLoad.has_revision=revision(name,m_PendingLoad.revision);if(m_SoundFontWorker){if(!m_SoundFontWorker->submit(m_SoundFonts,name,bytes,expected_crc)){cancel_incremental_load();return false;}m_PendingLoad.phase=LoadPhase::SoundFont;return true;}SoundFontFiles input(*this);bool ok=m_SoundFonts.load(input,name,m_PendingLoad.view);CloseSoundFontRead();if(ok && expected_crc)ok=groove::soundfont_crc(m_PendingLoad.view.soundfont)==expected_crc;if(!ok){cancel_incremental_load();return false;}m_PendingLoad.phase=LoadPhase::Ready;return true;}
 m_PendingLoad.size=unsigned(bytes);m_PendingLoad.has_revision=revision(name,m_PendingLoad.revision);
 const u8* cached=nullptr;unsigned cached_size=0;for(unsigned i=m_Count;i>0;--i){auto& c=m_Cache[i-1];if(strcmp(c.path,name))continue;if(c.stream && bytes>8u*1024u*1024u && c.stream->current()){m_PendingLoad.view=c.view;m_PendingLoad.phase=LoadPhase::Ready;return true;}if(!c.stream && c.length==bytes){cached=c.bytes;cached_size=c.length;m_PendingLoad.cache_index=i-1;break;}}
 if(bytes>8*1024*1024){m_PendingLoad.phase=LoadPhase::Stream;return true;}
 if(!m_ChunkedLoad){m_ChunkedLoad=groove::alloc_new<groove::ChunkedSampleRead>(*this);}bool can_add=m_Count<128 && uint64_t(m_Bytes)+bytes<=256u*1024u*1024u;
 if(!m_ChunkedLoad || !m_ChunkedLoad->start(name,cached,cached_size,can_add,expected_crc)){cancel_incremental_load();return false;}m_PendingLoad.phase=LoadPhase::Reading;return true;
}
groove::SampleLoadProgress CProjectStorage::FinishPendingLoad(groove::WavView& out){
 if(!PendingRevisionCurrent() || m_Count>=128 || uint64_t(m_Bytes)+m_PendingLoad.size>256u*1024u*1024u){cancel_incremental_load();return groove::SampleLoadProgress::Failed;}
 auto& c=m_Cache[m_Count];if(!groove::copy_text(c.path,sizeof c.path,m_PendingLoad.path)){cancel_incremental_load();return groove::SampleLoadProgress::Failed;}c.bytes=m_PendingLoad.high?m_PendingLoad.high:m_PendingLoad.low;c.high=m_PendingLoad.high!=nullptr;c.length=m_PendingLoad.size;c.view=m_PendingLoad.view;
 if(m_PendingLoad.high){delete[] m_PendingLoad.low;m_PendingLoad.low=nullptr;m_PendingLoad.high=nullptr;}else m_PendingLoad.low=nullptr;
 m_Bytes+=c.length;++m_Count;out=c.view;cancel_incremental_load();return groove::SampleLoadProgress::Ready;
}
groove::SampleLoadProgress CProjectStorage::poll_incremental_load(groove::WavView& out){
 using Progress=groove::SampleLoadProgress;if(m_PendingLoad.phase==LoadPhase::Idle)return Progress::Failed;
 if(m_PendingLoad.phase==LoadPhase::SoundFont){ServiceSoundFont();bool ok=false;if(!m_SoundFontWorker->collect(m_PendingLoad.view,ok))return Progress::Working;CloseSoundFontRead();ok=ok && PendingRevisionCurrent();if(ok)out=m_PendingLoad.view;m_PendingLoad.phase=LoadPhase::Ready;cancel_incremental_load();return ok?Progress::Ready:Progress::Failed;}
 if(m_PendingLoad.phase==LoadPhase::Ready){if(!PendingRevisionCurrent()){cancel_incremental_load();return Progress::Failed;}out=m_PendingLoad.view;cancel_incremental_load();return Progress::Ready;}
 if(m_PendingLoad.phase==LoadPhase::Stream){bool ok=PendingRevisionCurrent() && load(m_PendingLoad.path,out);cancel_incremental_load();return ok?Progress::Ready:Progress::Failed;}
 if(m_PendingLoad.phase==LoadPhase::Reading){auto result=m_ChunkedLoad->service();if(result==groove::ChunkedSampleRead::Result::Working)return Progress::Working;if(result==groove::ChunkedSampleRead::Result::Matched){if(!PendingRevisionCurrent()){cancel_incremental_load();return Progress::Failed;}out=m_Cache[m_PendingLoad.cache_index].view;cancel_incremental_load();return Progress::Ready;}if(result!=groove::ChunkedSampleRead::Result::Loaded || !m_ChunkedLoad->take(m_PendingLoad.low,m_PendingLoad.size,m_PendingLoad.view)){cancel_incremental_load();return Progress::Failed;}
  m_PendingLoad.high=static_cast<u8*>(CMemorySystem::HeapTryAllocate(m_PendingLoad.size,HEAP_CPU));if(!m_PendingLoad.high)m_PendingLoad.high=static_cast<u8*>(CMemorySystem::HeapTryAllocate(m_PendingLoad.size,HEAP_HIGH));if(!m_PendingLoad.high)return FinishPendingLoad(out);m_PendingLoad.offset=0;m_PendingLoad.phase=LoadPhase::Promote;return Progress::Working;
 }
 unsigned left=m_PendingLoad.size-m_PendingLoad.offset,count=left>groove::kFileScanBufferBytes?groove::kFileScanBufferBytes:left;__builtin_memcpy(m_PendingLoad.high+m_PendingLoad.offset,m_PendingLoad.low+m_PendingLoad.offset,count);m_PendingLoad.offset+=count;if(m_PendingLoad.offset<m_PendingLoad.size)return Progress::Working;
 groove::WavView promoted{};if(groove::parse_wav(m_PendingLoad.high,m_PendingLoad.size,promoted))m_PendingLoad.view=promoted;else{CMemorySystem::HeapFree(m_PendingLoad.high);m_PendingLoad.high=nullptr;}return FinishPendingLoad(out);
}
groove::WavView CProjectStorage::lookup(const char* name)const{auto sf=m_SoundFontWorker && m_SoundFontWorker->busy()?groove::WavView{}:m_SoundFonts.lookup(name);if(sf.soundfont)return sf;for(unsigned i=m_Count;i>0;--i)if(!strcmp(m_Cache[i-1].path,name))return m_Cache[i-1].view;return {};}
bool CProjectStorage::prepare(const groove::ProjectState& project,groove::WavView* views){for(unsigned i=0;i<groove::kSampleAssets;++i){views[i]={};if(project.assets[i].path[0] && !(groove::twin_material(project,i)?load_twin(project.assets[i].path,views[i]):load(project.assets[i].path,views[i])))return false;}return true;}
void CProjectStorage::CloseSoundFontRead(){if(m_SoundFontOpen){CLogger::Get()->Write("groove",LogNotice,"SF2 read cursor opens 1 reads %u seeks %u",m_SoundFontReads,m_SoundFontSeeks);f_close(&m_SoundFontFile);m_SoundFontOpen=false;m_SoundFontPath[0]=0;}}
bool CProjectStorage::ReadSoundFont(const char* name,uint64_t offset,void* data,unsigned wanted,unsigned& got){
 if(m_Watchdog)m_Watchdog->Start();
 got=0;if(m_SoundFontOpen && strcmp(name,m_SoundFontPath))CloseSoundFontRead();
 if(!m_SoundFontOpen){char path[208];if(!Absolute(name,path,sizeof path) || !groove::copy_text(m_SoundFontPath,sizeof m_SoundFontPath,name) || f_open(&m_SoundFontFile,path,FA_READ)!=FR_OK)return false;m_SoundFontOpen=true;m_SoundFontReads=m_SoundFontSeeks=0;}
 // Preserve FatFs' FAT-chain cursor across the sequential CRC/sample passes.
 // Reopening and seeking from the start on every32/64KiB read was quadratic.
 ++m_SoundFontReads;if(f_tell(&m_SoundFontFile)!=offset)++m_SoundFontSeeks;
 bool ok=(f_tell(&m_SoundFontFile)==offset || f_lseek(&m_SoundFontFile,offset)==FR_OK) && f_read(&m_SoundFontFile,data,wanted,&got)==FR_OK;
 // This path also serves synchronous validated project loads. Progressing
 // 148 MiB SF2 reads must not outlive the main-loop watchdog or starve ARP/TCP.
 // Yield only after FatFs returns; no other task owns this file cursor.
 if(ok && got){if(m_Watchdog)m_Watchdog->Start();CScheduler::Get()->Yield();}
 return ok;
}
void CProjectStorage::ServiceSoundFont(){if(!m_SoundFontWorker)return;StorageUiWait::Scope responsive;SoundFontFiles input(*this);m_SoundFontWorker->service(input);if(m_PendingLoad.phase!=LoadPhase::SoundFont){groove::WavView discarded;bool ok=false;if(m_SoundFontWorker->collect(discarded,ok))CloseSoundFontRead();}}
void CProjectStorage::service(){ServiceSoundFont();if(m_RecordOpen && !m_RecordReserved && CTimer::GetClockTicks64()/1000-m_RecordWriteMs>1250)CloseRecordingWrite();StorageUiWait::Scope responsive;for(unsigned i=0;i<m_Count;++i)if(m_Cache[i].stream)m_Cache[i].stream->service();}
void CProjectStorage::TraceStreams(){for(unsigned i=0;i<m_Count;++i)if(m_Cache[i].stream)CLogger::Get()->Write("stream",LogNotice,"Stream path %s misses %u current %u",m_Cache[i].path,m_Cache[i].stream->underruns(),unsigned(m_Cache[i].stream->current()));}
bool CProjectStorage::Load(groove::ProjectState& p,groove::RecoverySource* source){groove::read_project_location(*this,"GROOVE/CURRENT.TXT",m_CurrentProject,sizeof m_CurrentProject);unsigned version=0;groove::RecoverySource found;groove::AssetCatalog assets;groove::PresetLibrary presets;char error[96]{};bool indexed=presets.refresh(*this);auto* candidate=groove::alloc_new<groove::ProjectState>();auto validate=[&](groove::ProjectState& attempt,unsigned,const groove::PreservedStore& store){return !store.dropped_unknown && indexed && attempt.content_kind==groove::ContentKind::Song && groove::project_paths(attempt,m_CurrentProject,true,true) && assets.resolve_project(*this,attempt,error,sizeof error) && groove::resolve_pinned_content(*this,presets,assets,attempt,error,sizeof error,&store) && groove::verify_project_assets(*this,attempt);};if(!candidate)return false;bool ok=groove::load_recoverable_checked(*this,m_CurrentProject,*candidate,&version,&found,validate,&m_Preserved);if(!ok && strcmp(m_CurrentProject,groove::kFactoryStartupProject)){strcpy(m_CurrentProject,groove::kFactoryStartupProject);ok=groove::load_recoverable_checked(*this,m_CurrentProject,*candidate,&version,&found,validate,&m_Preserved);}if(ok){p=*candidate;if(source)*source=found;}else if(source)*source={};delete candidate;return ok;}
bool CProjectStorage::SaveUpdateSnapshot(const groove::ProjectState& p,const groove::Engine& engine){return groove::update::save_update_snapshot(*this,p,engine,&m_Preserved);}

// DATA only. Paths are generated from FatFs directory entries, never supplied
// by the network. Keep the fixed score, runtime config and known sound library.
static bool CleanBenchmarkTree(const char* full,const char* relative,unsigned depth,bool dry_run,CRecoveryWatchdog* watchdog,unsigned& files,unsigned& dirs){
 if(depth>16)return false;
 DIR dir;if(f_opendir(&dir,full)!=FR_OK)return false;
 bool ok=true;FILINFO info;
 while(ok){
  if(watchdog)watchdog->Start();
  if(f_readdir(&dir,&info)!=FR_OK){ok=false;break;}
  if(!info.fname[0])break;
  if(!strcmp(info.fname,".") || !strcmp(info.fname,".."))continue;
  char child[768],rel[768];
  if(!groove::join_path(child,sizeof child,full,info.fname) || !groove::join_path(rel,sizeof rel,relative,info.fname)){ok=false;break;}
  if(info.fattrib&AM_DIR){
   if(groove::benchmark_keep_subtree(rel))continue;
   if(!CleanBenchmarkTree(child,rel,depth+1,dry_run,watchdog,files,dirs)){ok=false;break;}
   if(!groove::benchmark_keep_directory(rel)){if(!dry_run && ((info.fattrib&AM_RDO && f_chmod(child,0,AM_RDO)!=FR_OK) || f_unlink(child)!=FR_OK)){ok=false;break;}++dirs;}
  }else if(!groove::benchmark_keep_file(rel)){if(!dry_run && ((info.fattrib&AM_RDO && f_chmod(child,0,AM_RDO)!=FR_OK) || f_unlink(child)!=FR_OK)){ok=false;break;}++files;}
 }
 if(f_closedir(&dir)!=FR_OK)ok=false;
 return ok;
}
bool CProjectStorage::CleanBenchmarkData(unsigned& files,unsigned& dirs){
 files=dirs=0;
 if(!CloseRecordingWrite())return false;
 if(!m_Mounted || strcmp(m_Drive,"DATA:") || !exists("REMOTE.ON") || !groove::benchmark_file_matches(*this,"BENCH.MGXS"))return false;
 uint32_t crc=0;if(!groove::file_checksum(*this,"FACTORY/DEBUG/demo-slices.wav",crc) || crc!=0x7c7e7b61u)return false;
 unsigned planned_files=0,planned_dirs=0;
 if(!CleanBenchmarkTree("DATA:","",0,true,m_Watchdog,planned_files,planned_dirs))return false;
 CLogger::Get()->Write("groove",LogNotice,"Benchmark DATA cleanup planned files %u dirs %u",planned_files,planned_dirs);
 return CleanBenchmarkTree("DATA:","",0,false,m_Watchdog,files,dirs) && files==planned_files && dirs==planned_dirs;
}

// A FACTORY-only filesystem walk, never a raw partition write. The one-off
// all-DATA retirement finished in v555; later library updates leave user data
// outside FACTORY untouched. Keep current containers/materials/settings.
static bool CleanContentReports(groove::IFileAccess& access,const char* full,const char* relative,unsigned depth,CRecoveryWatchdog* watchdog,unsigned& files,unsigned& dirs){
 if(depth>16)return false;
 DIR dir;if(f_opendir(&dir,full)!=FR_OK)return false;FILINFO info;bool ok=true;
 while(ok){
  if(watchdog)watchdog->Start();
  if(f_readdir(&dir,&info)!=FR_OK){ok=false;break;}if(!info.fname[0])break;
  if(!strcmp(info.fname,".") || !strcmp(info.fname,".."))continue;
  char path[768],rel[768];if(!groove::join_path(path,sizeof path,full,info.fname) || !groove::join_path(rel,sizeof rel,relative,info.fname)){ok=false;break;}
  if(info.fattrib&AM_DIR){if(groove::data_under(rel,"firmware"))continue;ok=CleanContentReports(access,path,rel,depth+1,watchdog,files,dirs);
   if(ok && groove::data_equal(rel,"assets/demo-projects")){FRESULT r=f_unlink(path);if(r==FR_OK)++dirs;else if(r!=FR_DENIED)ok=false;}
  }else {
   const auto retired=groove::retire_obsolete_container(access,rel);
   if(retired==groove::RetirementResult::ProtectedContainer)continue;
   if(retired==groove::RetirementResult::Failed){ok=false;break;}
   if(retired==groove::RetirementResult::Removed){++files;CLogger::Get()->Write("groove",LogNotice,"DATA retired old container %s",rel);continue;}
   // Preserve current/future containers and unknown .mgx family files even
   // when a renamed alias would match the historical disposable list.
   if(groove::data_mgx_file(rel) || groove::data_mgx_temporary(rel) || groove::data_suffix(rel,".mgp") || groove::data_suffix(rel,".mgk"))continue;
   if(!groove::data_disposable_file(rel))continue;
   if((info.fattrib&AM_RDO && f_chmod(path,0,AM_RDO)!=FR_OK) || f_unlink(path)!=FR_OK){ok=false;break;}
   ++files;CLogger::Get()->Write("groove",LogNotice,"DATA removed %s",rel);
  }
 }
 return f_closedir(&dir)==FR_OK && ok;
}
bool CProjectStorage::PrepareContent(){
 if(!m_Mounted)return false;
 // An SD-root development boot gets factory files too, but cleanup is DATA only.
 m_InstallingFactory=true;groove::DataDirectoryReport directories;bool normalized=groove::normalize_data_directories(*this,directories);
 CLogger::Get()->Write("groove",normalized?LogNotice:LogWarning,"DATA directory spelling renamed %u complete %u",directories.renamed,unsigned(normalized));
 BOOT_STORAGE("phase","replacement recovery begin");
 if(!groove::FileReplace::recover_all(*this)){m_InstallingFactory=false;CLogger::Get()->Write("groove",LogError,"DATA replacement recovery incomplete; preserving candidates");return false;}
 BOOT_STORAGE("phase","replacement recovery done / factory gate begin");
 groove::FactoryBootReport gate;unsigned started=CTimer::GetClockTicks();
 bool ok=groove::prepare_factory_content(*this,kEmbeddedDemoSample,sizeof kEmbeddedDemoSample,GrooveFirmwareVersion(),gate);m_InstallingFactory=false;
 CLogger::Get()->Write("groove",ok?LogNotice:LogWarning,"Factory gate build %u revision %u skipped %u stat %u marker %u elapsed-us %u",GrooveFirmwareVersion(),groove::kFactoryCatalogRevision,unsigned(gate.skipped),gate.stat_checked,unsigned(gate.marker_saved),unsigned(CTimer::GetClockTicks()-started));
 const auto& report=gate.install;
 CLogger::Get()->Write("groove",ok?LogNotice:LogWarning,"Factory catalog checked %u repaired %u demos %u ok %u retired %u",report.checked,report.repaired,groove::kFactoryDemoCount,unsigned(ok),report.retired);
 if(ok){char previous[160]{};if(!groove::read_project_location(*this,"GROOVE/CURRENT.TXT",previous,sizeof previous) || !groove::data_content_kind(*this,previous) || groove::data_equal(previous,"BENCH.MGXS") || groove::retired_factory_demo(previous))ok=groove::write_project_location(*this,"GROOVE/CURRENT.TXT",groove::kFactoryStartupProject);}
 unsigned files=0,dirs=0;m_InstallingFactory=true;if(ok && !strcmp(m_Drive,"DATA:"))ok=CleanContentReports(*this,"DATA:/FACTORY","FACTORY",0,m_Watchdog,files,dirs);m_InstallingFactory=false;
 CLogger::Get()->Write("groove",ok?LogNotice:LogWarning,"DATA content cleanup files %u dirs %u ok %u",files,dirs,unsigned(ok));
 return ok;
}

bool CProjectStorage::space(uint64_t& free,uint64_t& total){if(!m_Mounted)return false;DWORD clusters=0;FATFS* fs=nullptr;if(m_Watchdog)m_Watchdog->Start();if(f_getfree(m_Drive,&clusters,&fs)!=FR_OK || !fs)return false;free=uint64_t(clusters)*fs->csize*512;total=uint64_t(fs->n_fatent-2)*fs->csize*512;if(m_Watchdog)m_Watchdog->Start();return true;}
