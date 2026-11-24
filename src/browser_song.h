#pragma once
#include "data_layout.h"
#include "project_files.h"

namespace groove {
// Normalize a valid staged Song in place before folder sorting and pagination.
inline bool resolve_browser_entry(IFileAccess& fs,const char* directory,FileEntry& entry){
 if(entry.directory || !data_suffix(entry.name,".mgxs.tmp"))return data_browser_entry(directory,entry.name,entry.directory);
 char temp[192],main[192];
 if(!join_path(temp,sizeof temp,directory,entry.name))return false;
 entry.name[__builtin_strlen(entry.name)-4]=0;
 if(!data_browser_entry(directory,entry.name,false) || !join_path(main,sizeof main,directory,entry.name) ||
    fs.exists(main) || data_content_kind(fs,temp)!=unsigned(ContentKind::Song) || !valid_project_file(fs,temp))return false;
 entry.orphan_song=true;
 return true;
}
}
