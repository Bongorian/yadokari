#include "group_routing_cache.h"
#include <cassert>
int main(){groove::GroupBus groups[8]{};groove::GroupRoutingCache cache;
 const auto* a=cache.resolve(groups);for(unsigned i=0;i<16;++i)assert(a[i]==8);
 groups[3].members=uint16_t((1u<<0)|(1u<<15));a=cache.resolve(groups);assert(a[0]==3 && a[15]==3 && a[1]==8);
 groups[1].members=1;a=cache.resolve(groups);assert(a[0]==1 && a[15]==3);
 groups[1].members=0;groups[3].members=0;a=cache.resolve(groups);assert(a[0]==8 && a[15]==8);
}
