#pragma once
#include "../project_files.h"
#include "../engine.h"
namespace groove { namespace update {
// Operational update snapshot, outside every user content shelf. Never replace
// a user-authored PROJECTS/Recovery.mgxs or change the current-song locator.
inline constexpr const char* kUpdateSnapshot="GROOVE/Update-Recovery.mgxs";
inline bool save_update_snapshot(IFileAccess& fs,const ProjectState& project,const Engine& engine,const PreservedStore* preserved=nullptr){
 if(!fs.directory("GROOVE"))return false;
 auto* copy=alloc_new<ProjectState>(project);
 bool ok=copy && project_paths(*copy,kUpdateSnapshot,false) && save_recoverable_checked(fs,kUpdateSnapshot,*copy,[&](){return engine.project_snapshot_safe();},preserved);
 delete copy;return ok;
}
}}
