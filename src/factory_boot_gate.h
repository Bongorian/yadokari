#pragma once
#include "factory_content.h"
#include "update/sha256.h"
namespace groove {
struct FactoryCatalogEntry {const char* path;uint64_t bytes;};
#include "generated/factory_catalog.inc"
inline constexpr const char* kFactoryBootMarker="GROOVE/FACTORY.CHK";
inline constexpr const char* kFactoryBootTemporary="GROOVE/FACTORY.CHK.tmp";
inline constexpr const char* kFactoryOptionalKit="FACTORY/KITS/Techno-01.mgxk";
struct FactoryBootReport {FactoryInstallReport install;unsigned stat_checked=0;bool skipped=false,marker_saved=false;};
// Fixed 96-byte FGT1: schema/build/catalog revision/count/optional-size,
// reserved zeros, catalog SHA256, then SHA256 of the first 64 bytes.
inline void factory_boot_record(uint8_t (&bytes)[96],uint32_t build,uint32_t optional){
 for(auto& b:bytes)b=0;
 __builtin_memcpy(bytes,"FGT1",4);update::put32(bytes+4,1);update::put32(bytes+8,build);update::put32(bytes+12,kFactoryCatalogRevision);
 update::put32(bytes+16,sizeof kFactoryCatalog/sizeof *kFactoryCatalog);update::put32(bytes+20,optional);
 __builtin_memcpy(bytes+32,kFactoryCatalogHash,32);update::Sha256::hash(bytes,64,bytes+64);
}
inline bool factory_boot_stats(IFileAccess& fs,uint32_t optional,FactoryBootReport& report){
 for(const auto& entry:kFactoryCatalog){FileRevision st;++report.stat_checked;if(!fs.revision(entry.path,st) || st.size!=entry.bytes)return false;}
 if(optional){FileRevision st;++report.stat_checked;if(!fs.revision(kFactoryOptionalKit,st) || st.size!=optional)return false;}
 return true;
}
inline bool prepare_factory_content(IFileAccess& fs,const uint8_t* sample,unsigned sample_bytes,uint32_t build,FactoryBootReport& report){
 uint8_t marker[96]{},expected[96]{};unsigned got=0;uint32_t optional=0;
 if(fs.size(kFactoryBootMarker)==sizeof marker && fs.read(kFactoryBootMarker,0,marker,sizeof marker,got) && got==sizeof marker){
  optional=update::get32(marker+20);factory_boot_record(expected,build,optional);
  if(update::equal(marker,expected,sizeof marker) && factory_boot_stats(fs,optional,report)){report.skipped=true;report.marker_saved=true;return true;}
 }
 // Invalidate before touching any factory file. A partial full install must
 // never be skipped merely because the old files still have matching sizes.
 if(!fs.directory("GROOVE") || !fs.remove(kFactoryBootMarker) || !fs.remove(kFactoryBootTemporary))return false;
 if(!install_factory_content(fs,sample,sample_bytes,report.install))return false;
 optional=0;FileRevision st;if(fs.revision(kFactoryOptionalKit,st)){if(st.size>0xffffffffu)return false;optional=uint32_t(st.size);}
 if(!factory_boot_stats(fs,optional,report))return false;
 factory_boot_record(expected,build,optional);
 {FileWriter out(fs,kFactoryBootTemporary);if(!out.write(expected,sizeof expected) || !out.finish())return false;}
 if(!cached_file_matches(fs,kFactoryBootTemporary,expected,sizeof expected) || !fs.move(kFactoryBootTemporary,kFactoryBootMarker))return false;
 report.marker_saved=true;return true;
}
}
