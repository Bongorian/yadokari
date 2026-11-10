#pragma once
#include "chunk_io.h"
#include "project_format_version.h"
namespace groove {
// Reject obsolete/truncated/oversized containers before reserving a full read
// buffer. ContentKind is still decoded from the body, never from a suffix.
inline bool current_project_header(const uint8_t* h,uint64_t size){
 if(!h || size<14 || h[0]!='M' || h[1]!='G' || h[2]!='X' || h[3]!='C' || (unsigned(h[4])|(unsigned(h[5])<<8))!=kFormatVersion)return false;
 uint32_t payload=uint32_t(h[6])|(uint32_t(h[7])<<8)|(uint32_t(h[8])<<16)|(uint32_t(h[9])<<24);
 return payload<=kMaxFormatBytes && size==uint64_t(payload)+14;
}
}
