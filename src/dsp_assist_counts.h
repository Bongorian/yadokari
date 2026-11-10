#pragma once
#include <stdint.h>
namespace groove {
struct DspAssistCounts {
 uint64_t submitted=0,used=0,late=0,busy=0,checked=0,mismatch=0,cloned=0,reused=0;
 DspAssistCounts operator-(const DspAssistCounts& b)const{return {submitted-b.submitted,used-b.used,late-b.late,busy-b.busy,checked-b.checked,mismatch-b.mismatch,cloned-b.cloned,reused-b.reused};}
 void add(const DspAssistCounts& b){submitted+=b.submitted;used+=b.used;late+=b.late;busy+=b.busy;checked+=b.checked;mismatch+=b.mismatch;cloned+=b.cloned;reused+=b.reused;}
};
}
