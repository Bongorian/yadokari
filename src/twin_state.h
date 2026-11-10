#pragma once
#include <stdint.h>
namespace groove {
// Song association only. Sound definitions remain ordinary embedded pool entries.
struct TwinState {
 uint16_t source=65535,sampled=65535;uint8_t digest[32]{};bool active=false;
 bool prepared()const{return source<256 && sampled<256 && source!=sampled;}
};
enum class TwinStatus:uint8_t {Empty,Ready,Stale,Active};
inline const char* twin_status_name(TwinStatus status){static const char* names[]={"TWIN","TWIN READY","TWIN STALE","TWIN ACTIVE"};return names[unsigned(status)];}
struct TwinConfig {
 uint8_t low=36,high=84,interval=12,velocity=100;uint16_t hold_ms=500,max_ms=6000;
 unsigned count()const{return interval && high>=low?1u+(high-low)/interval:0;}
 bool valid()const{return low<=127 && high<=127 && low<=high && interval>=1 && interval<=127 && velocity>=1 && velocity<=127 && count()<=16 && hold_ms>=10 && hold_ms<=4000 && max_ms>=hold_ms && max_ms<=10000;}
 unsigned note(unsigned i)const{return low+i*interval;}
};
}
