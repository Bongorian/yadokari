#pragma once
#include <stdint.h>
namespace groove { namespace ui {
struct FileToast {
 char text[128]{};uint32_t last_hash=0;uint64_t until=0;
 static bool instruction(const char* value){const char* starts[]={"PUSH:","SHIFT", "Shift", "TURN:","Top", "TOP", "Press ","TOUCH ","SELECT +","Open a folder or WAV"};for(auto p:starts){unsigned i=0;while(p[i] && p[i]==value[i])++i;if(!p[i])return true;}return false;}
 void observe(const char* value,uint64_t now){uint32_t hash=2166136261u;for(const char* p=value;*p;++p)hash=(hash^uint8_t(*p))*16777619u;if(hash==last_hash)return;last_hash=hash;if(!*value || instruction(value))return;unsigned n=0;while(value[n] && n<127){text[n]=value[n];++n;}text[n]=0;until=now+2000;}
 bool active(uint64_t now)const{return text[0] && now<until;}
};
}}
