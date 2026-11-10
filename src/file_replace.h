#pragma once
#include "data_layout.h"
#include "update/sha256.h"
namespace groove {
// File-service owner only. The plan is durable before either name changes.
// No historical generations: all three reserved files disappear after recovery.
class FileReplace {
 IFileAccess& fs_;char root_[40]{},plan_[96]{},draft_[96]{},old_[96]{},next_[96]{};
 struct Record {char destination[192]{},source[192]{};uint64_t old_size=0,new_size=0;uint8_t old_hash[32]{},new_hash[32]{};bool had_old=false;};
 bool paths(const char* root){return copy_text(root_,sizeof root_,root) && join_path(plan_,sizeof plan_,root,".MGX-REPLACE.plan") && join_path(draft_,sizeof draft_,root,".MGX-REPLACE.plan.tmp") && join_path(old_,sizeof old_,root,".MGX-REPLACE.old") && join_path(next_,sizeof next_,root,".MGX-REPLACE.new");}
 static bool source_path(const char* path){for(const char* root:kUserShelves){char expected[96];if(join_path(expected,sizeof expected,root,".MGX-TRANSFER.tmp") && !__builtin_strcmp(path,expected))return true;}return false;}
 static bool target_path(const char* root,const char* path){unsigned n=unsigned(__builtin_strlen(root));if(__builtin_strncmp(root,path,n) || path[n]!='/')return false;const char* name=path+n+1;if(!*name || *name=='.')return false;for(const char* p=name;*p;++p)if(*p=='/' || *p=='\\' || *p==':' || uint8_t(*p)<32)return false;return data_suffix(name,".mgxs") || data_suffix(name,".mgxk") || data_suffix(name,".mgxp") || data_suffix(name,".wav") || data_suffix(name,".sf2") || data_suffix(name,".mid");}
 static void put64(uint8_t* b,uint64_t n){for(unsigned i=0;i<8;++i)b[i]=uint8_t(n>>(8*i));}
 static uint64_t get64(const uint8_t* b){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=uint64_t(b[i])<<(8*i);return n;}
 bool digest(const char* path,uint64_t size,uint8_t out[32],bool (*proceed)(void*)=nullptr,void* context=nullptr){if(!fs_.exists(path) || fs_.size(path)!=size)return false;FileScanBuffer block;if(!block.data())return false;update::Sha256 hash;for(uint64_t at=0;at<size;){if(proceed && !proceed(context))return false;unsigned wanted=unsigned(size-at>kFileScanBufferBytes?kFileScanBufferBytes:size-at),got=0;if(!fs_.read(path,at,block.data(),wanted,got) || got!=wanted)return false;hash.add(block.data(),got);at+=got;}hash.finish(out);return true;}
 bool matches(const char* path,uint64_t size,const uint8_t hash[32]){uint8_t actual[32];return digest(path,size,actual) && update::equal(actual,hash,32);}
 bool decode(const char* path,Record& r){uint8_t bytes[512],sum[32];unsigned got=0;if(fs_.size(path)!=sizeof bytes || !fs_.read(path,0,bytes,sizeof bytes,got) || got!=sizeof bytes || __builtin_memcmp(bytes,"MRP1",4) || bytes[4]>1 || bytes[5]!=1 || bytes[6] || bytes[7] || bytes[199] || bytes[391])return false;update::Sha256::hash(bytes,472,sum);if(!update::equal(sum,bytes+472,32))return false;for(unsigned i=504;i<512;++i)if(bytes[i])return false;__builtin_memcpy(r.destination,bytes+8,192);__builtin_memcpy(r.source,bytes+200,192);r.had_old=bytes[4];r.old_size=get64(bytes+392);r.new_size=get64(bytes+400);__builtin_memcpy(r.old_hash,bytes+408,32);__builtin_memcpy(r.new_hash,bytes+440,32);return target_path(root_,r.destination) && source_path(r.source) && r.new_size && r.new_size<=1024ull*1024*1024;}
 bool write(const Record& r){uint8_t bytes[512]{};__builtin_memcpy(bytes,"MRP1",4);bytes[4]=r.had_old;bytes[5]=1;__builtin_memcpy(bytes+8,r.destination,192);__builtin_memcpy(bytes+200,r.source,192);put64(bytes+392,r.old_size);put64(bytes+400,r.new_size);__builtin_memcpy(bytes+408,r.old_hash,32);__builtin_memcpy(bytes+440,r.new_hash,32);update::Sha256::hash(bytes,472,bytes+472);if(!fs_.write(draft_,0,bytes,sizeof bytes,true))return false;Record checked;if(!decode(draft_,checked))return false;return fs_.move(draft_,plan_);}
 bool drop(const char* path){return !fs_.exists(path) || fs_.remove(path);}
 bool cleanup(const Record& r){return drop(old_) && drop(next_) && drop(r.source) && drop(draft_) && drop(plan_);}
 bool recover(bool& new_won,bool verified_source=false){new_won=false;Record r;if(!decode(plan_,r))return false;
  if(matches(r.destination,r.new_size,r.new_hash)){new_won=true;return cleanup(r);}
  const char* candidate=verified_source && fs_.exists(r.source) && fs_.size(r.source)==r.new_size?r.source:matches(next_,r.new_size,r.new_hash)?next_:matches(r.source,r.new_size,r.new_hash)?r.source:nullptr;
  if(candidate){
   if(fs_.exists(r.destination)){if(!r.had_old || !matches(r.destination,r.old_size,r.old_hash))return false;if(fs_.exists(old_)){if(!matches(old_,r.old_size,r.old_hash) || !fs_.remove(r.destination))return false;}else if(!fs_.move(r.destination,old_))return false;}
   if(candidate!=next_){if(fs_.exists(next_) || !fs_.move(candidate,next_))return false;}
   if(!fs_.move(next_,r.destination) || (!verified_source && !matches(r.destination,r.new_size,r.new_hash)))return false;
   new_won=true;return cleanup(r);
  }
  if(r.had_old && matches(r.destination,r.old_size,r.old_hash))return cleanup(r);
  if(r.had_old && !fs_.exists(r.destination) && matches(old_,r.old_size,r.old_hash)){if(!fs_.move(old_,r.destination) || !matches(r.destination,r.old_size,r.old_hash))return false;return cleanup(r);}
  return false;
 }
public:
 explicit FileReplace(IFileAccess& fs):fs_(fs){}
 bool replace(const char* source,const char* destination,uint64_t bytes,const uint8_t hash[32],bool (*proceed)(void*)=nullptr,void* context=nullptr){char folder[192];if(!copy_text(folder,sizeof folder,destination))return false;parent_path(folder);if(!paths(folder) || !target_path(folder,destination) || !source_path(source) || fs_.exists(plan_) || fs_.exists(draft_) || fs_.exists(old_) || fs_.exists(next_))return false;Record r;if(!copy_text(r.destination,sizeof r.destination,destination) || !copy_text(r.source,sizeof r.source,source))return false;r.new_size=bytes;__builtin_memcpy(r.new_hash,hash,32);r.had_old=fs_.exists(destination);if(r.had_old){r.old_size=fs_.size(destination);if(!digest(destination,r.old_size,r.old_hash,proceed,context))return false;}if(proceed && !proceed(context))return false;if(!write(r)){if(fs_.exists(plan_)){bool won=false;recover(won);}else fs_.remove(draft_);return false;}bool won=false;if(recover(won,true))return won;bool recovered=false;recover(recovered);return false;}
 static bool pending(IFileAccess& fs){FileReplace x(fs);for(const char* root:kUserShelves){if(!x.paths(root) || fs.exists(x.plan_) || fs.exists(x.draft_) || fs.exists(x.old_) || fs.exists(x.next_))return true;}return false;}
 static bool recover_all(IFileAccess& fs){FileReplace transaction(fs);for(const char* root:kUserShelves){if(!transaction.paths(root))return false;if(fs.exists(transaction.plan_)){bool won=false;if(!transaction.recover(won))return false;}else if(fs.exists(transaction.draft_)){Record record;if(transaction.decode(transaction.draft_,record)){if(!fs.move(transaction.draft_,transaction.plan_))return false;bool won=false;if(!transaction.recover(won))return false;}else if(!fs.remove(transaction.draft_))return false;}else if(fs.exists(transaction.old_) || fs.exists(transaction.next_))return false;}
  // These fixed receive names are reserved by this service, never user names.
  constexpr const char* receives[]={"PROJECTS","PRESETS","KITS","ASSETS/SAMPLES","ASSETS/SOUNDFONTS","ASSETS/MIDI","RECORDINGS","PRESETS/DX7"};for(const char* root:receives){char source[96];if(!join_path(source,sizeof source,root,".MGX-TRANSFER.tmp") || (fs.exists(source) && !fs.remove(source)))return false;}return true;}
};
}
