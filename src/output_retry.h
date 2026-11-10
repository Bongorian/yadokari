#pragma once
#include <stdint.h>
namespace groove {
// Task-side retry spacing; no timer, allocation, or work occurs in audio.
// The first attempt is immediate. Failed attempts back off from 100 ms to
// five seconds; success and reconnection restore immediate operation.
class OutputRetry {
public:
 bool ready(uint64_t now_ms) const { return !waiting_ || now_ms-failed_at_>=delay_ms_; }
 void failed(uint64_t now_ms) { failed_at_=now_ms;waiting_=true;delay_ms_=next_ms_;next_ms_=next_ms_<2500?next_ms_*2:5000; }
 void reset() { waiting_=false;delay_ms_=0;next_ms_=100; }
private:
 uint64_t failed_at_=0;
 unsigned delay_ms_=0,next_ms_=100;
 bool waiting_=false;
};
}
