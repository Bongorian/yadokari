#pragma once
#include <stdint.h>
namespace groove {
// Task-owned HTTP connection deadline. Synchronous validation is not idle
// reception; the response gets a fresh deadline after the commit completes.
class FileHttpDeadline {
 enum class Phase {Receive,Commit,Response};Phase phase_=Phase::Receive;uint64_t activity_=0;
public:
 void receive(uint64_t now){phase_=Phase::Receive;activity_=now;}
 void commit(){phase_=Phase::Commit;}
 void response(uint64_t now){phase_=Phase::Response;activity_=now;}
 void progress(uint64_t now){activity_=now;}
 bool expired(uint64_t now)const{return phase_!=Phase::Commit && now>activity_ && now-activity_>5000000;}
};
}
