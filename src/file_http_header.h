#pragma once
#include "file_web.h"
namespace groove {
struct FileHttpHeader {
 char method[8]{},url[1024]{},code[16]{};uint64_t length=0;
 bool parse(char* text){char* end=__builtin_strstr(text,"\r\n");if(!end)return false;*end=0;char* first=__builtin_strchr(text,' ');if(!first)return false;*first++=0;char* second=__builtin_strchr(first,' ');if(!second)return false;*second++=0;if(__builtin_strcmp(second,"HTTP/1.1") || !copy_text(method,sizeof method,text) || !copy_text(url,sizeof url,first) || url[0]!='/')return false;
  bool saw_length=false,saw_host=false,saw_code=false;char cookie[16]{};bool saw_cookie=false;for(char* at=end+2;*at;){char* next=__builtin_strstr(at,"\r\n");if(!next)return false;*next=0;if(!*at)break;char* colon=__builtin_strchr(at,':');if(!colon)return false;*colon++=0;while(*colon==' ' || *colon=='\t')++colon;char* tail=colon+__builtin_strlen(colon);while(tail>colon && (tail[-1]==' ' || tail[-1]=='\t'))*--tail=0;
   if(data_equal(at,"Content-Length")){uint64_t n;if(saw_length || !FileWeb::integer(colon,n) || n>1024ull*1024*1024 || (__builtin_strcmp(method,"PUT") && n>32768))return false;length=n;saw_length=true;}
   else if(data_equal(at,"Transfer-Encoding"))return false;
   else if(data_equal(at,"Host")){if(saw_host || !*colon)return false;saw_host=true;}
   else if(data_equal(at,"Cookie")){for(char* item=colon;*item;){while(*item==' ' || *item==';')++item;char* last=__builtin_strchr(item,';');if(!last)last=item+__builtin_strlen(item);if(last-item>=4 && !__builtin_memcmp(item,"MGX=",4)){unsigned bytes=unsigned(last-item-4);if(saw_cookie || bytes>=sizeof cookie)return false;__builtin_memcpy(cookie,item+4,bytes);cookie[bytes]=0;saw_cookie=true;}item=*last?last+1:last;}}
   else if(data_equal(at,"X-Groove-Code")){if(saw_code || !copy_text(code,sizeof code,colon))return false;saw_code=true;}
   at=next+2;
  }if(!saw_code && saw_cookie)copy_text(code,sizeof code,cookie);return saw_host;
 }
};
}
