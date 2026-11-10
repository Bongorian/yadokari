#include "project.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <pthread.h>
#ifdef GROOVE_RESET_BASELINE
#include "project_reset_before.inc"
#endif
__attribute__((noinline)) static void* reset_under_caller(void* opaque){
 // Reserve the actual Circle Run caller's ~56 KiB while resetting on a
 // 128 KiB thread stack. All source/default objects belong on the heap.
 volatile unsigned char parent[56*1024];for(unsigned i=0;i<sizeof parent;++i)parent[i]=static_cast<unsigned char>(i*17);
 auto& p=*static_cast<groove::ProjectState*>(opaque);
#ifdef GROOVE_RESET_BASELINE
 groove::legacy_make_empty_project(p);
#else
 groove::make_empty_project(p);
#endif
 for(unsigned i=0;i<sizeof parent;++i)assert(parent[i]==static_cast<unsigned char>(i*17));
 return nullptr;
}
int main(){using namespace groove;auto p=std::make_unique<ProjectState>();p->song.enabled=true;p->morph_set=3;p->tempo=271;p->tracks[15].instrument=Instrument::VirtualAnalog;p->sounds[200].instrument=Instrument::Fm6;p->patterns[0].tracks[4].steps[0].enabled=true;p->assets[0].path[0]='X';
 pthread_attr_t attributes;assert(!pthread_attr_init(&attributes));assert(!pthread_attr_setstacksize(&attributes,128*1024));pthread_t thread;assert(!pthread_create(&thread,&attributes,reset_under_caller,p.get()));assert(!pthread_join(thread,nullptr));pthread_attr_destroy(&attributes);
 assert(!p->song.enabled && p->morph_set==0 && p->tempo==120 && !p->assets[0].path[0]);for(auto& s:p->sounds)assert(s.instrument==Instrument::Empty);for(unsigned i=0;i<16;++i)assert(p->tracks[i].instrument==Instrument::Empty && p->tracks[i].midi_channel==i);assert(!p->patterns[0].tracks[4].steps[0].enabled && p->patterns[0].tracks[4].steps[0].sound==kNoPreset);std::puts("PASS project reset on 128 KiB stack with live 56 KiB caller, canonical defaults and empty pool");
}
