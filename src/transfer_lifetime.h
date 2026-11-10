#pragma once
#include <stdint.h>
namespace groove {
class TransferLifetime {
 uint64_t started_=0;bool active_=false;
public:
 static constexpr uint64_t duration_us=600000000;
 void open(uint64_t now){started_=now;active_=true;}
 void close(){active_=false;}
 bool active()const{return active_;}
 bool expired(uint64_t now)const{return active_ && now-started_>=duration_us;}
 unsigned remaining(uint64_t now)const{return !active_ || expired(now)?0:unsigned((duration_us-(now-started_)+999999)/1000000);}
};
}
