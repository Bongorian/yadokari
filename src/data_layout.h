#pragma once
#include "project_format_version.h"
#include "file_access.h"
namespace groove {
inline char data_lower(char c){return c>='A' && c<='Z'?char(c+32):c;}
inline bool data_equal(const char* a,const char* b){while(*a && *b)if(data_lower(*a++)!=data_lower(*b++))return false;return *a==*b;}
inline bool data_under(const char* path,const char* root){while(*root)if(data_lower(*path++)!=data_lower(*root++))return false;return !*path || *path=='/';}
inline bool data_suffix(const char* path,const char* suffix){unsigned a=0,b=0;while(path[a])++a;while(suffix[b])++b;return a>=b && data_equal(path+a-b,suffix);}
// Suffixes are discovery/display hints only. TYPE remains authoritative.
inline unsigned data_mgx_suffix_length(const char* path){
 if(data_suffix(path,".mgx"))return 4;
 return data_suffix(path,".mgxs") || data_suffix(path,".mgxk") || data_suffix(path,".mgxp")?5:0;
}
inline bool data_mgx_file(const char* path){return data_mgx_suffix_length(path)!=0;}
inline bool data_mgx_temporary(const char* path){return data_suffix(path,".mgx.tmp") || data_suffix(path,".mgxs.tmp") || data_suffix(path,".mgxk.tmp") || data_suffix(path,".mgxp.tmp");}
// Values match ContentKind; keeping this helper independent avoids a project/UI include cycle.
inline const char* data_content_extension(unsigned kind){return kind==1?".mgxs":kind==2?".mgxk":kind==3?".mgxp":nullptr;}
inline bool data_content_name(char* out,unsigned capacity,const char* name,unsigned kind){
 const char* extension=data_content_extension(kind);if(!extension)return false;
 unsigned n=0;while(name[n])++n;n-=data_mgx_suffix_length(name);
 if(!n || n+6>capacity)return false;
 __builtin_memmove(out,name,n);return copy_text(out+n,capacity-n,extension);
}
inline void data_strip_mgx_suffix(char* name){unsigned n=0;while(name[n])++n;name[n-data_mgx_suffix_length(name)]=0;}
// Templates are edited in RAM, saved as user projects and restored on open.
inline bool factory_path(const char* path){char clean[192];return canonical_path(clean,sizeof clean,"",path) && (data_under(clean,"FACTORY") || (data_equal(clean,"BENCH.MGXS") || data_equal(clean,"BENCH.MGX")));}
inline bool data_playable_file(const char* name){if(data_mgx_file(name))return true;const char* extensions[]={".wav",".mid",".midi",".syx",".sf2",".part"};for(const char* ext:extensions)if(data_suffix(name,ext))return true;return false;}
inline bool data_browser_entry(const char* directory,const char* name,bool folder){
 char path[192];if(name[0]=='.' || !join_path(path,sizeof path,directory,name))return false;
 if(data_under(path,"GROOVE") || data_under(path,"firmware") || (data_equal(path,"BENCH.MGXS") || data_equal(path,"BENCH.MGX")))return false;
 return folder || data_playable_file(name);
}
inline constexpr const char* kRetiredFactoryDemos[]={"01-Neon-House.mgx","02-Foundry.mgx","03-Paper-Moon.mgx","04-Night-Transit.mgx","05-Saltwater.mgx","06-Glass-Orbit.mgx","07-Afterimage.mgx","08-Ion-Relay.mgx","09-Sun-Circuit.mgx","10-Seven-Lanterns.mgx","01-Velvet-Platform.mgx","02-Pressure-Lock.mgx","03-Last-Train-Home.mgx","04-Broken-Signal.mgx","05-Tidal-Dub.mgx","06-Floating-Gardens.mgx","07-Chrome-Sunset.mgx","08-Vector-Funk.mgx","09-Terracotta.mgx","10-Sevenfold.mgx"};
inline bool retired_factory_demo(const char* path){
 for(const char* file:kRetiredFactoryDemos){char full[160];if(join_path(full,sizeof full,"FACTORY/DEMOS",file) && data_equal(path,full))return true;}return false;
}
// Narrow cleanup: never infer that an unfamiliar song/audio file is unused.
// Configuration and license/provenance files are intentionally not on this list.
inline bool data_disposable_file(const char* path){
 if(data_under(path,"firmware"))return false;
 if(retired_factory_demo(path))return true;
 // Unsupported/corrupt content must remain available for explicit load errors.
 // A version bump is not authorization to delete user content or recovery temps.
 if(data_mgx_file(path) || data_mgx_temporary(path))return false;
 if(data_suffix(path,".mgk") || data_suffix(path,".mgp"))return true;
 // Retired generations are not retained as backups on this instrument.
 for(const char* p=path;*p;++p)if(*p=='.'){
  char base[192];unsigned n=unsigned(p-path);if(n>=sizeof base)continue;__builtin_memcpy(base,path,n);base[n]=0;
  if(data_mgx_file(base) || data_suffix(base,".mgk") || data_suffix(base,".mgp"))return true;
 }
 const char* extensions[]={".csv",".tsv",".log",".png",".bmp",".html",".pdf"};for(const char* ext:extensions)if(data_suffix(path,ext))return true;
 return data_equal(path,"assets/demo-projects/factory-session.mgx") || data_equal(path,"assets/demo-projects/factory-live.mgx") || data_equal(path,"assets/demo-projects/factory-slices.mgx") || data_equal(path,"assets/demo-projects/showcase.mgx");
}
// TYPE is the first frame in current MGXC, allowing bounded Browser classification.
inline unsigned data_content_header(const uint8_t* h,unsigned n){
 return n>=23 && !__builtin_memcmp(h,"MGXC",4) && h[4]==uint8_t(kFormatVersion) && h[5]==uint8_t(kFormatVersion>>8) && !__builtin_memcmp(h+10,"TYPE",4) && h[14]==1 && !h[15] && !h[16] && !h[17] && h[18]==1 && !h[19] && !h[20] && !h[21] && h[22]>=1 && h[22]<=3?h[22]:0;
}
inline unsigned data_content_kind(IFileAccess& fs,const char* path){uint8_t header[23];unsigned got=0;return fs.read(path,0,header,sizeof header,got)?data_content_header(header,got):0;}
inline constexpr const char* kUserShelves[]={"PROJECTS","PRESETS","KITS","ASSETS/SAMPLES","ASSETS/SOUNDFONTS","ASSETS/MIDI","RECORDINGS"};
inline constexpr const char* kCanonicalDataDirectories[]={"GROOVE","PROJECTS","PRESETS","PRESETS/DX7","RECORDINGS","KITS","ASSETS","ASSETS/SAMPLES","ASSETS/SOUNDFONTS","ASSETS/MIDI","FACTORY","FACTORY/DEMOS","FACTORY/DEBUG","FACTORY/KITS","FACTORY/DRUMS","FACTORY/PRESETS","FACTORY/PRESETS/SAMPLES"};
inline const char* data_shelf_for_content(unsigned kind){return kind==1?kUserShelves[0]:kind==2?kUserShelves[2]:kind==3?kUserShelves[1]:nullptr;}
constexpr const char* kUserProject="PROJECTS/Untitled.mgxs";
constexpr const char* kFactoryStartupProject="FACTORY/DEBUG/Benchmark.mgxs";
}
