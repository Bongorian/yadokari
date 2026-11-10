#pragma once
#include "session_files.h"
#include "factory_library.h"
namespace groove {
inline constexpr const char* kFactoryRepresentative="FACTORY/PRESETS/LIBRARY/TX81z Lately Bass.mgxp";
struct FactoryLibraryAudit {unsigned presets=0,kits=0,old_formats=0;bool complete=false,representative=false,old_name=false;};
// Explicit main-loop diagnostic. Traverse the real Browser's engine/kind
// filters and 64-row viewport, without auditioning or writing shelf content.
inline FactoryLibraryAudit audit_factory_library(SessionFiles& files,IFileAccess& fs,Performance& ui){
 FactoryLibraryAudit report;auto* saved=alloc_new<BrowserState>(ui.browser());if(!saved)return report;
 unsigned cursor=files.library_cursor_;auto& b=ui.browser();b.library_originals=true;b.library_kits=false;
 bool ok=files.browse_library(FileAction::LibraryOpen);
 for(unsigned kind=0;kind<2 && ok;++kind){b.library_kits=kind!=0;
  for(unsigned engine=0;engine<(kind?1:instrument_count()) && ok;++engine){
   b.library_delta=int(engine);ok=files.browse_library(FileAction::LibraryEngine);unsigned visited=0;
   while(ok && visited<b.library_total){
    for(unsigned i=0;i<b.count;++i){const char* path=b.library_paths[i];
     if(!data_under(path,kind?"FACTORY/KITS/LIBRARY":"FACTORY/PRESETS/LIBRARY"))continue;
     if(kind)++report.kits;else ++report.presets;
     report.old_name|=data_equal(filename(path),"DX7 Lately Bass.mgxp") || !__builtin_strcmp(b.entries[i].name,"DX7 Lately Bass");
     if(data_equal(path,kFactoryRepresentative))report.representative=!__builtin_strcmp(b.entries[i].name,"TX81z Lately Bass") && b.library_engine[i]==Instrument::Fm6;
    }
    visited+=b.count;if(visited>=b.library_total)break;b.library_delta=64;ok=files.browse_library(FileAction::LibraryMove);
   }
  }
 }
 ShelfScan scan;auto inspect=[&](const char* path,const FileEntry& entry){if(!data_mgx_file(entry.name))return;
  uint8_t header[6]{};unsigned got=0;if(!fs.read(path,0,header,6,got) || got!=6 || __builtin_memcmp(header,"MGXC",4) || wav_u16(header+4)!=kFormatVersion)++report.old_formats;
  report.old_name|=data_equal(filename(path),"DX7 Lately Bass.mgxp");
 };walk_content_shelf(fs,"FACTORY",inspect,scan);ok=ok && scan.complete;
 b=*saved;delete saved;files.library_cursor_=cursor;ui.mark_dirty();report.complete=ok;return report;
}
}
