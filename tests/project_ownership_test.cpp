#include "project_load_slot.h"
#include "project_save_slot.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
static void* projects[32]{};static unsigned allocations=0,live=0;
static void* allocate(size_t n){void* p=std::malloc(n?n:1);if(p && n==sizeof(groove::ProjectState)){++allocations;++live;for(auto& slot:projects)if(!slot){slot=p;return p;}std::abort();}return p;}
static void release(void* p){if(p){for(auto& slot:projects)if(slot==p){slot=nullptr;--live;break;}}std::free(p);}
[[gnu::noinline]] void* operator new(size_t n){if(auto* p=allocate(n))return p;throw std::bad_alloc();}
[[gnu::noinline]] void* operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void* operator new(size_t n,const std::nothrow_t&)noexcept{return allocate(n);}
[[gnu::noinline]] void* operator new[](size_t n,const std::nothrow_t&)noexcept{return allocate(n);}
[[gnu::noinline]] void operator delete(void* p)noexcept{release(p);}[[gnu::noinline]] void operator delete[](void* p)noexcept{release(p);}
[[gnu::noinline]] void operator delete(void* p,size_t)noexcept{release(p);}[[gnu::noinline]] void operator delete[](void* p,size_t)noexcept{release(p);}
[[gnu::noinline]] void operator delete(void* p,const std::nothrow_t&)noexcept{release(p);}[[gnu::noinline]] void operator delete[](void* p,const std::nothrow_t&)noexcept{release(p);}
static uint8_t* copy(const groove::ChunkBuffer& b){auto* p=new uint8_t[b.size];std::memcpy(p,b.data,b.size);return p;}
int main(){using namespace groove;
 auto original=std::make_unique<ProjectState>();make_empty_project(*original);original->tempo=143;ChunkBuffer bytes;assert(encode_project_buffer(bytes,*original));unsigned baseline=live;
 {ProjectEncodeSlot slot;auto* owned=new ProjectState(*original);assert(slot.submit(owned,"song.mgxs"));}assert(live==baseline);
 {ProjectEncodeSlot slot;assert(slot.submit_copy(*original,"song.mgxs") && slot.process_one());}assert(live==baseline);
 {ProjectDecodeSlot slot;assert(slot.submit(copy(bytes),bytes.size));}assert(live==baseline);
 {ProjectDecodeSlot slot;assert(slot.initialize(nullptr));assert(!slot.initialize(nullptr));assert(slot.submit(copy(bytes),bytes.size) && slot.process_one());}assert(live==baseline);
 {
  ProjectDecodeSlot slot;assert(slot.initialize(nullptr));unsigned before=allocations;
  for(unsigned i=0;i<8;++i){assert(slot.submit(copy(bytes),bytes.size));if(i==2)slot.cancel();assert(slot.process_one());ProjectDecodeSlot::Result result;assert(slot.collect(result));
   if(i==2){assert(!result.project && !result.version);continue;}
   assert(result.project && result.project->tempo==143 && result.project->tracks[0].sample_edit.speed==original->tracks[0].sample_edit.speed);
   assert(result.project->tracks[0].texture.values[9]==original->tracks[0].texture.values[9]);
   result.project->tracks[0].sample_edit.speed=.25f;result.project->tracks[0].texture.values[9]=.987f;
   assert(slot.recycle(result.project));
  }
  assert(allocations==before); // No second 45 MiB parser candidate per decode.
 }assert(live==baseline);
 std::puts("Project ownership: pending/ready cleanup, private pooled decode, fresh inactive defaults, cancellation and allocation count passed");
}
