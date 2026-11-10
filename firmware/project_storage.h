#pragma once
#include <fatfs/ff.h>
#include <circle/types.h>
#include <circle/util.h>
#include "../src/file_access.h"
#include "../src/preserved_payload.h"
#include "../src/recovery_source.h"
#include "../src/sample_stream.h"
#include "../src/chunked_sample_read.h"
#include "recovery_watchdog.h"
#include "../src/soundfont.h"
#include "../src/soundfont_load_slot.h"
namespace groove { class Engine; }
// Task-owned DATA filesystem and immutable sample cache. Old views stay alive
// until shutdown, including across asynchronous project snapshot handoffs.
class CProjectStorage:public groove::IFileAccess,public groove::ISampleLibrary {
public:
 ~CProjectStorage();bool Mount();
 bool begin_read_session(const char*)override;void end_read_session()override;
 bool space(uint64_t& free,uint64_t& total)override;
 // Transfer guard: FAT short names and OEM case variants can name one entry.
 bool same_reference(const char*,const char*);
 bool CleanBenchmarkData(unsigned& files,unsigned& dirs);
 bool PrepareContent();
 void SetWatchdog(CRecoveryWatchdog* watchdog){m_Watchdog=watchdog;}
 const char* CurrentProject()const{return m_CurrentProject;}
 bool Load(groove::ProjectState&,groove::RecoverySource* source=nullptr);bool SaveUpdateSnapshot(const groove::ProjectState&,const groove::Engine&);
 // MGXC fallback preservation for the boot session file; the kernel
 // syncs it with SessionFiles' store after boot (see kernel.cpp).
 groove::PreservedStore& preserved(){return m_Preserved;}
 void SetSoundFontWorker(groove::SoundFontLoadSlot* worker){m_SoundFontWorker=worker;}
 const char* failure_reason()const override{if(m_SoundFontWorker && m_SoundFontWorker->busy())return "SF2 loading / sound kept";return m_SoundFonts.failure()[0]?m_SoundFonts.failure():"Assets unavailable / sound kept";}
 bool load(const char*,groove::WavView&)override;
 bool load_twin(const char*,groove::WavView&)override;
 bool load_impl(const char*,groove::WavView&,bool);
 groove::WavView lookup(const char*)const override;
 bool prepare(const groove::ProjectState&,groove::WavView*)override;
 bool supports_incremental_load()const override{return true;}
 bool incremental_validates_crc(const char* path,uint64_t bytes)override{return (bytes>=44 && bytes<=8u*1024u*1024u) || groove::soundfont_file_header(*this,path);}
 bool begin_incremental_load(const char*,uint32_t)override;
 groove::SampleLoadProgress poll_incremental_load(groove::WavView&)override;
 void cancel_incremental_load()override;
 void service()override;
 void TraceStreams();
 unsigned list(const char*,groove::FileEntry*,unsigned)override;
 bool browser_file_management_allowed()const override{return m_Mounted && !strcmp(m_Drive,"DATA:");}
 bool list_checked(const char*,groove::FileEntry*,unsigned,unsigned&)override;
 bool read(const char*,uint64_t,void*,unsigned,unsigned&)override;
 bool write(const char*,uint64_t,const void*,unsigned,bool)override;
 bool begin_recording(const char*,const void*,unsigned,uint64_t)override;
 bool finish_recording(const char*,const void*,unsigned,uint64_t)override;
 bool random_bytes(uint8_t*,unsigned)override;
 bool move(const char*,const char*)override;bool remove(const char*)override;bool directory(const char*)override;uint64_t size(const char*)override;bool exists(const char*)override;bool revision(const char*,groove::FileRevision&)override;
 struct StreamCounters{unsigned readers=0;uint64_t reads=0,bytes=0,failures=0,misses=0;};
 StreamCounters stream_counters()const{StreamCounters out;for(const auto& c:m_Cache)if(c.stream){auto n=c.stream->counters();++out.readers;out.reads+=n.reads;out.bytes+=n.bytes;out.failures+=n.failures;out.misses+=n.misses;}return out;}
private:
 groove::SoundFontCache m_SoundFonts;groove::SoundFontLoadSlot* m_SoundFontWorker=nullptr;
 void ServiceSoundFont();
 u8* m_CpuIo=nullptr; // LOW bounce for explicit CPU-only storage spans
 FIL m_ReadFile{};bool m_ReadOpen=false;char m_ReadPath[160]{};
 FIL m_SoundFontFile{};bool m_SoundFontOpen=false;char m_SoundFontPath[96]{};unsigned m_SoundFontReads=0,m_SoundFontSeeks=0;
 bool ReadSoundFont(const char*,uint64_t,void*,unsigned,unsigned&);void CloseSoundFontRead();
 class SoundFontFiles:public groove::IFileAccess {
  CProjectStorage& owner_;
 public:
  explicit SoundFontFiles(CProjectStorage& owner):owner_(owner){}
  unsigned list(const char*,groove::FileEntry*,unsigned)override{return 0;}
  bool read(const char* path,uint64_t at,void* data,unsigned n,unsigned& got)override{return owner_.ReadSoundFont(path,at,data,n,got);}
  uint64_t size(const char* path)override{return owner_.size(path);}bool exists(const char* path)override{return owner_.exists(path);}
  bool write(const char*,uint64_t,const void*,unsigned,bool)override{return false;}
   bool move(const char*,const char*)override{return false;}bool remove(const char*)override{return false;}bool directory(const char*)override{return false;}
 };
 struct Cached {char path[96]{};u8* bytes=nullptr;bool high=false;unsigned length=0;groove::SampleStream* stream=nullptr;groove::WavView view{};};
 Cached m_Cache[128]{};unsigned m_Count=0,m_Streams=0,m_Bytes=0;
 enum class LoadPhase:u8 {Idle,Reading,Promote,Stream,Ready,SoundFont};
 struct PendingLoad {LoadPhase phase=LoadPhase::Idle;char path[96]{};u8* low=nullptr;u8* high=nullptr;unsigned size=0,offset=0,cache_index=0;bool has_revision=false;groove::FileRevision revision{};groove::WavView view{};};
 PendingLoad m_PendingLoad{};groove::ChunkedSampleRead* m_ChunkedLoad=nullptr;
 bool PendingRevisionCurrent();groove::SampleLoadProgress FinishPendingLoad(groove::WavView&);

 bool Absolute(const char*,char*,unsigned)const;
 // Main-loop-owned recording cursor: keep the FAT chain position across appends.
 bool CloseRecordingWrite(const char* path=nullptr);
 FIL m_RecordFile{};bool m_RecordOpen=false;char m_RecordPath[208]{};uint64_t m_RecordWriteMs=0,m_RecordLastStartUs=0,m_RecordSyncUs=0,m_RecordEnd=0;bool m_RecordReserved=false;unsigned m_RecordWriteMaxUs=0,m_RecordGapMaxUs=0,m_RecordSyncMaxUs=0,m_RecordSeekMaxUs=0,m_RecordCalls=0;
 groove::PreservedStore m_Preserved;
 CRecoveryWatchdog* m_Watchdog=nullptr;
 bool m_InstallingFactory=false;
 char m_CurrentProject[160]="FACTORY/DEBUG/Benchmark.mgxs";
 const char* m_Drive="SD:";FATFS m_Fs{};bool m_Mounted=false;
};
