#include "../src/sample_cache_promotion.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
int main(){
 const uint8_t wav[]={ 'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0xbb,0,0,0,0x77,1,0,2,0,16,0,'d','a','t','a',2,0,0,0,0,0x40 };
 auto* low=static_cast<uint8_t*>(malloc(sizeof wav));memcpy(low,wav,sizeof wav);groove::WavView view{};assert(groove::parse_wav(low,sizeof wav,view));
 auto* original=low;bool freed=false;auto missing=[](unsigned)->void*{return nullptr;};auto release_low=[&](uint8_t* p){assert(p==original);freed=true;free(p);};auto release_high=[](uint8_t* p){free(p);};
 assert(!groove::promote_sample_cache(low,sizeof wav,view,missing,release_low,release_high));assert(low==original && !freed && groove::wav_sample(view,0,0)==0.5f);
 auto allocate=[](unsigned n)->void*{return malloc(n);};assert(groove::promote_sample_cache(low,sizeof wav,view,allocate,release_low,release_high));assert(freed && low!=original && groove::wav_sample(view,0,0)==0.5f);assert(view.data>=low && view.data<low+sizeof wav);free(low);
 auto* corrupt=static_cast<uint8_t*>(malloc(sizeof wav));memcpy(corrupt,wav,sizeof wav);corrupt[0]=0;auto* before=corrupt;groove::WavView unchanged{};unchanged.frames=123;bool high_freed=false;
 auto unexpected_low=[](uint8_t*){assert(false);};auto failed_high=[&](uint8_t* p){high_freed=true;free(p);};
 assert(!groove::promote_sample_cache(corrupt,sizeof wav,unchanged,allocate,unexpected_low,failed_high));assert(corrupt==before && unchanged.frames==123 && high_freed);free(corrupt);
}
