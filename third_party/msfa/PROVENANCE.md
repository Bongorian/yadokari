# msfa

Unmodified per-file Apache-2.0 subset of [Dexed](https://github.com/asb2m10/dexed/tree/fea2f0f0f2c397799f59b27eb11c038cd34b22c5/source/msfa), commit `fea2f0f0f2c397799f59b27eb11c038cd34b22c5`.
Every included file has an Apache-2.0 notice; hashes and the exact list are in `audit.json`.
Dexed's GPL application, JUCE, MTS client, tuning wrappers and LGPL code are excluded.

The adapter uses the unchanged six-operator core, operator kernels, logarithmic
lookup tables, amplitude/pitch envelopes and LFO. It supplies note/patch ownership,
velocity/key scaling, pitch mapping and macro application. `Dx7Note` itself is not
compiled because its current wrapper depends on tuning/MTS/application services.
The unused `controllers.h` inclusion is suppressed by its include guard in the
adapter translation unit; its original bytes are retained. Env's unused logger
include resolves to a project-owned empty shim outside this directory.
Upstream fixed-point wrapping arithmetic is compiled with `-fwrapv` and without
UBSan shift instrumentation for the vendor translation unit; adapter bounds and
host integration remain sanitized. No new external runtime dependency is added.
