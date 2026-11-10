#pragma once
#include <stdint.h>
namespace groove { namespace ui {
// Actual conditional page advice is collected by the renderer, never laid out
// in the document body. The bounded list is reused by the temporary hint layer.
struct PageAdvice {
 char lines[24][160]{};unsigned count=0;
 void clear(){count=0;}
 void add(const char* text){if(!text || !*text || count==24)return;char line[160]{};unsigned n=0;for(;*text && n<155;++text){if(*text==':'){line[n++]=' ';line[n++]='>';line[n++]=' ';}else line[n++]=*text;}for(unsigned i=0;i<count;++i){unsigned p=0;while(lines[i][p] && lines[i][p]==line[p])++p;if(lines[i][p]==line[p])return;}for(unsigned i=0;i<=n;++i)lines[count][i]=line[i];++count;}
};
}}
