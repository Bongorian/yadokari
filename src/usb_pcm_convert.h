#pragma once
#include <stdint.h>
namespace groove {
// Convert USB left-justified 24-bit PCM to Circle's packed/right-justified
// 24-bit representation. The caller owns capacity and supplies complete samples.
// Expansion runs backward and compaction forward so source and destination may
// be the same buffer. No allocation or I/O is performed.
inline bool usb_pcm24_capture_inplace(uint8_t* data,unsigned bytes,unsigned wire,
                                      unsigned internal,unsigned capacity) {
 if (!data || (wire!=3 && wire!=4) || (internal!=3 && internal!=4) || bytes%wire) return false;
 unsigned count=bytes/wire;
 if (count>capacity/internal || bytes>capacity) return false;
 if (wire==3 && internal==3) return true;
 for (unsigned step=0;step<count;++step) {
  unsigned i=internal>wire ? count-1-step : step;
  const unsigned src=i*wire+(wire==4?1:0), dst=i*internal;
  uint8_t a=data[src],b=data[src+1],c=data[src+2];
  // Retain the discarded low wire byte in the unused internal byte. This
  // lets the hardware probe distinguish left- and right-justified capture.
  uint8_t low=wire==4?data[i*wire]:0;
  data[dst]=a;data[dst+1]=b;data[dst+2]=c;if(internal==4)data[dst+3]=low;
 }
 return true;
}
}
