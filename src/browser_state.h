#pragma once
#include "file_access.h"
#include "browser_file_ops.h"
#include "sample_analysis.h"
namespace groove {
enum class FileAction:uint8_t {None,FolderMove,LibraryOpen,LibraryMove,LibraryKind,LibraryOriginals,LibraryPrev,LibraryNext,SaveCurrentPreset,LibraryOpenKits,KitPrev,KitNext,MidiRecord,MidiRecordInternal,MidiImport,Capture,RecordToggle,SampleRecordToggle,AuditionSelectedSample,Overdub,Duplicate,ToggleFavorites,Refresh,Open,Up,Preview,StopPreview,Load,Favorite,Kit,SaveKit,Collect,AutoSlice,ImportWavLoop,MapPads,Factory,DiagnosticsTest,SavePreset,BeginStretch,CancelStretch,BeginFreeze,CancelFreeze,RenameFile,DeleteFile,NewFolder,Resample,CancelResample,SaveProject,AudioRecord,AudioOverdub,OverwritePreset,LibraryInspect,LibraryRestore,LibraryEngine,BeginTwin,CancelTwin,NewProjectMenu};
// Family recognition only; actual type comes from the container header.
inline unsigned browser_project_kind(const char* name){return data_mgx_file(name)?1:0;}
inline bool is_soundfont_file(const char* name){return data_suffix(name,".sf2");}
inline bool is_dx7_file(const char* name){return data_suffix(name,".syx");}
inline bool is_midi_file(const char* name){unsigned n=0;while(name[n])++n;auto lower=[](char c){return c>='A' && c<='Z'?char(c+('a'-'A')):c;};if(n>=4 && name[n-4]=='.' && lower(name[n-3])=='m' && lower(name[n-2])=='i' && lower(name[n-1])=='d')return true;return n>=5 && name[n-5]=='.' && lower(name[n-4])=='m' && lower(name[n-3])=='i' && lower(name[n-2])=='d' && lower(name[n-1])=='i';}
inline bool is_wav_file(const char* name){unsigned n=0;while(name[n])++n;auto lower=[](char c){return c>='A' && c<='Z'?char(c+('a'-'A')):c;};return n>=4 && name[n-4]=='.' && lower(name[n-3])=='w' && lower(name[n-2])=='a' && lower(name[n-1])=='v';}
inline bool is_recording_part(const char* name){unsigned n=0;while(name[n])++n;return n>=5 && name[n-5]=='.' && name[n-4]=='p' && name[n-3]=='a' && name[n-2]=='r' && name[n-1]=='t';}
struct MidiImportRequest {char path[160]{};uint32_t generation=0;uint8_t pattern=0,track=0,channel=0;};
struct BrowserState {bool new_project_list=false;unsigned folder_offset=0,folder_total=0;int folder_delta=0;bool library_resume=false,library_list=false,library_kits=false,library_originals=false;Instrument library_instrument=Instrument::Sampler;unsigned library_offset=0,library_total=0;int library_delta=0;uint8_t library_return=0;uint32_t library_revision[64]{};Instrument library_engine[64]{};uint16_t preview_kit_mask=0;Instrument preview_instrument=Instrument::Empty;char library_paths[64][160]{};bool library_factory[64]{};BrowserFileEdit edit{};char directory[160]="",preview_path[160]{},audition_path[160]{},selected_name[96]{},status[96]="Open a folder or WAV";FileEntry entries[64]{};unsigned count=0,cursor=0;SampleAnalysis analysis{};const void* analysis_source=nullptr;const void* analysis_pending_source=nullptr;WavView preview{};SampleMetadata metadata{};uint8_t audition_track=255,audition_pattern=255,audition_pad=255;bool audition_assigned=false,previewing=false,missing=false,favorites_only=false,analysis_pending=false,analysis_background=false;};
}
