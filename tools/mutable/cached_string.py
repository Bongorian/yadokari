#!/usr/bin/env python3
"""Generate a Rings string coefficient cache; preserve upstream and arithmetic.
Only frequency/brightness/damping-dependent coefficients are memoized. Delay,
phase, filter state and per-sample interpolators continue on every block.
"""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
manifest=json.loads((a.root/'resonator-texture-sources.json').read_text())
for name in ('rings/dsp/string.h','rings/dsp/string.cc'):
 raw=(a.root/name).read_bytes();assert hashlib.sha256(raw).hexdigest()==manifest['files'][name]
 s=raw.decode()
 if name.endswith('.h'):
  old='  float frequency_;';assert s.count(old)==1
  s=s.replace(old,'''  bool coefficients_valid_;
  float cached_frequency_,cached_brightness_,cached_damping_;
  float cached_coefficient_,cached_bright_,cached_noise_,cached_cutoff_,cached_f_;
'''+old)
 else:
  s=s.replace('  enable_dispersion_ = enable_dispersion;','  enable_dispersion_ = enable_dispersion;\n  coefficients_valid_ = false;')
  start=s.index('  float lf_damping =');end=s.index('  fir_damping_filter_.Configure',start)
  block=s[start:end]
  s=s[:start]+'''  bool changed = !coefficients_valid_ || cached_frequency_ != frequency_
      || cached_brightness_ != brightness_ || cached_damping_ != damping_;
  if (changed) {
'''+block+'''    cached_frequency_ = frequency_; cached_brightness_ = brightness_; cached_damping_ = damping_;
    cached_coefficient_ = damping_coefficient; cached_bright_ = brightness;
    cached_noise_ = noise_filter; cached_cutoff_ = damping_cutoff; cached_f_ = damping_f;
    coefficients_valid_ = true;
  }
  float damping_coefficient = cached_coefficient_, brightness = cached_bright_;
  float noise_filter = cached_noise_, damping_cutoff = cached_cutoff_, damping_f = cached_f_;
'''+s[end:]
  s=s.replace('  iir_damping_filter_.set_f_q<FREQUENCY_ACCURATE>(damping_f, 0.5f);','  if (changed) iir_damping_filter_.set_f_q<FREQUENCY_ACCURATE>(damping_f, 0.5f);')
 out=a.output/name;out.parent.mkdir(parents=True,exist_ok=True)
 if not out.exists() or out.read_text()!=s:out.write_text(s)
