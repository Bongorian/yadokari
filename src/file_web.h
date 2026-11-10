#pragma once
#include "user_files.h"
namespace groove {
class FileWeb {
 UserFiles& files_;char code_[16]{};char* text_=nullptr;unsigned used_=0;
 bool append(const char* s){unsigned n=unsigned(__builtin_strlen(s));if(!text_ || used_+n>=131072)return false;__builtin_memcpy(text_+used_,s,n+1);used_+=n;return true;}
 void number(uint64_t n){char s[24];unsigned at=23;s[at]=0;do{s[--at]=char('0'+n%10);n/=10;}while(n);append(s+at);}
 void quoted(const char* s){append("\"");for(;*s;++s){char v[3]={*s,0,0};if(*s=='"' || *s=='\\'){v[0]='\\';v[1]=*s;}if(uint8_t(*s)<32)v[0]='?';append(v);}append("\"");}
 static int hex(char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;}
public:
 unsigned status=200;const char* content_type="application/json";char file[192]{};uint64_t file_bytes=0;
 explicit FileWeb(UserFiles& files,const char* code):files_(files),text_(alloc_array<char>(131072)){copy_text(code_,sizeof code_,code);}
 ~FileWeb(){delete[] text_;}
 const char* body()const{return text_?text_:"{\"error\":\"MEMORY\"}";}unsigned bytes()const{return text_?used_:18;}
 static bool parameter(const char* url,const char* key,char* out,unsigned capacity){const char* at=url;while(*at && *at!='?')++at;if(!*at)return false;++at;unsigned k=unsigned(__builtin_strlen(key));while(*at){const char* end=at;while(*end && *end!='&')++end;if(unsigned(end-at)>k && !__builtin_memcmp(at,key,k) && at[k]=='='){unsigned n=0;for(const char* p=at+k+1;p<end;++p){unsigned c=uint8_t(*p);if(c=='%'){if(end-p<3)return false;int a=hex(p[1]),b=hex(p[2]);if(a<0 || b<0)return false;c=unsigned(a*16+b);p+=2;}else if(c=='+')c=' ';if(!c || n+1>=capacity)return false;out[n++]=char(c);}out[n]=0;return true;}at=*end?end+1:end;}return false;}
 static bool integer(const char* s,uint64_t& n){n=0;if(!*s)return false;for(;*s;++s){if(*s<'0' || *s>'9' || n>(0xffffffffffffffffull-unsigned(*s-'0'))/10)return false;n=n*10+unsigned(*s-'0');}return true;}
 static bool digest(const char* s,uint8_t out[32]){if(__builtin_strlen(s)!=64)return false;for(unsigned i=0;i<32;++i){int a=hex(s[i*2]),b=hex(s[i*2+1]);if(a<0 || b<0)return false;out[i]=uint8_t(a*16+b);}return true;}
 bool authorized(const char* code)const{return code && code_[0] && __builtin_strlen(code)==__builtin_strlen(code_) && update::equal(reinterpret_cast<const uint8_t*>(code),reinterpret_cast<const uint8_t*>(code_),__builtin_strlen(code_));}
 bool begin_put(const char* url,const char* code,uint64_t bytes){
  Reset();if(!Authorize(code))return false;char name[96]{},sha[80]{};uint8_t sum[32];
  if(__builtin_strncmp(url,"/api/v1/file?",13) || !parameter(url,"name",name,sizeof name) || !parameter(url,"sha256",sha,sizeof sha) || !digest(sha,sum))return Error(UserFileError::Type);
  return files_.begin(name,bytes,sum) || Error(files_.error());
 }
 bool put_chunk(uint64_t at,const uint8_t* bytes,unsigned n){return files_.chunk(at,bytes,n) || Error(files_.error());}
 bool finish_put(){return files_.commit()?Success():Error(files_.error());}
 bool request(const char* method,const char* url,const char* code,const uint8_t* body,unsigned n){
  if(!__builtin_strcmp(method,"PUT")){if(!begin_put(url,code,n))return false;for(unsigned at=0;at<n;){unsigned count=n-at>32768?32768:n-at;if(!put_chunk(at,body+at,count))return false;at+=count;}return finish_put();}
  Reset();if(!Authorize(code))return false;char path[192]{},value[96]{};uint64_t number=0;
  char route[64];unsigned r=0;while(url[r] && url[r]!='?' && r+1<sizeof route){route[r]=url[r];++r;}route[r]=0;
  bool get=!__builtin_strcmp(method,"GET");
  if(get && !__builtin_strcmp(route,"/api/v1/status")){uint64_t free=0,total=0;if(!files_.space(free,total))return Error(files_.error());append("{\"free\":");this->number(free);append(",\"total\":");this->number(total);append("}");return true;}
  if(get && !__builtin_strcmp(route,"/api/v1/list") && parameter(url,"shelf",value,sizeof value) && integer(value,number) && number<7){auto* entries=alloc_array<UserFileEntry>(256);unsigned count=0;bool ok=entries && files_.list(UserShelf(number),entries,256,count);if(ok){append("{\"files\":[");for(unsigned i=0;i<count;++i){if(i)append(",");append("{\"path\":");quoted(entries[i].path);append(",\"bytes\":");this->number(entries[i].bytes);append("}");}append("]}");}delete[] entries;return ok || Error(files_.error());}
  if(get && !__builtin_strcmp(route,"/api/v1/file") && parameter(url,"path",path,sizeof path)){unsigned got=0;uint8_t probe;if(!files_.get(path,0,&probe,1,got))return Error(files_.error());copy_text(file,sizeof file,path);file_bytes=files_.size(path);content_type="application/octet-stream";return true;}
  if(!__builtin_strcmp(method,"DELETE") && !__builtin_strcmp(route,"/api/v1/file") && parameter(url,"path",path,sizeof path))return files_.erase(path)?Success():Error(files_.error());
  if(!__builtin_strcmp(method,"POST") && !__builtin_strcmp(route,"/api/v1/cancel")){files_.cancel();return Success();}
  return Error(UserFileError::Ok);
 }
private:
 void Reset(){status=200;content_type="application/json";file[0]=0;file_bytes=0;used_=0;if(text_)text_[0]=0;}
 bool Authorize(const char* code){if(!text_){status=503;return false;}if(!files_.active()){status=503;append("{\"error\":\"CLOSED\"}");return false;}if(!authorized(code)){status=403;append("{\"error\":\"CODE_REQUIRED\"}");return false;}return true;}
 bool Success(){used_=0;text_[0]=0;append("{\"ok\":true,\"path\":");quoted(files_.destination());append("}");return true;}
 bool Error(UserFileError e){status=e==UserFileError::Closed?503:e==UserFileError::Busy?409:e==UserFileError::Protected?423:e==UserFileError::Io?500:e==UserFileError::Exists?409:400;used_=0;if(!text_)return false;text_[0]=0;append("{\"error\":");quoted(e==UserFileError::Ok?"INVALID_REQUEST":user_file_error(e));append(",\"path\":");quoted(files_.destination());append("}");return false;}
};
}
