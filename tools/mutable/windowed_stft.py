#!/usr/bin/env python3
"""Generate an exact batched analysis window; leave FFT/overlap/add untouched."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
name='clouds/dsp/pvoc/stft.cc';raw=(a.root/name).read_bytes();manifest=json.loads((a.root/'resonator-texture-sources.json').read_text());assert hashlib.sha256(raw).hexdigest()==manifest['files'][name]
s=raw.decode();s='#include "native_window.h"\n'+s
start=s.index('  for (size_t i = 0; i < fft_size_; ++i) {',s.index('void STFT::Buffer()'))
end=s.index('  // Compute FFT.',start)
s=s[:start]+'''  size_t copied = 0;
  while (copied < fft_size_) {
    size_t n = std::min(fft_size_ - copied, buffer_size_ - source_ptr);
    groove::native_analysis_window(fft_in_ + copied, analysis_ + source_ptr,
        w, window_stride_, n);
    copied += n; w += n * window_stride_; source_ptr += n;
    if (source_ptr == buffer_size_) source_ptr = 0;
  }

'''+s[end:]
out=a.output;out.parent.mkdir(parents=True,exist_ok=True)
if not out.exists() or out.read_text()!=s:out.write_text(s)
