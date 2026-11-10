#pragma once
#include <stdint.h>
namespace groove {
// A retryable queue conflict is different from a permanent transaction refusal.
enum class SequenceCommitResult:uint8_t {Applied,Busy,Invalid,NoMemory,SyncLost};
}
