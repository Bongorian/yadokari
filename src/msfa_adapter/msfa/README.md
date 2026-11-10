# Include anchor

The compiler searches this directory for msfa's `../debugger/Logger.h` include.
The sibling project-owned header has no logging implementation: the vendored
Env translation unit does not use one. Vendor bytes stay unchanged.
