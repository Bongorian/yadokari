#pragma once
#include <stdint.h>
namespace groove {
enum class RecoveryKind : uint8_t { None, Main, Temporary };
struct RecoverySource { RecoveryKind kind=RecoveryKind::None; };
}
