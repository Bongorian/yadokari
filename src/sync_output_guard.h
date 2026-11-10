#pragma once
#include <stdint.h>
namespace groove {
// Pi 4 Circle xHCI blocking transfers use a three-second fallback without a
// confirmed endpoint cancellation. Do not reuse that endpoint/buffer after a
// timeout-looking failure until the USB function has been re-enumerated.
class SyncOutputGuard {
 public:
  bool ready() const {return !blocked_;}
  void reset(){blocked_=false;}
  void completed(bool success,uint32_t elapsed_us){if(!success && elapsed_us>=2900000u)blocked_=true;}
 private: bool blocked_=false;
};
}
