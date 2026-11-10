#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
make docs-check
make test session-demo
if pkg-config --exists sdl2; then
 make simulator-test
elif [ -f .deps/sdl/usr/include/SDL2/SDL.h ]; then
 sdl_triplet=$(c++ -print-multiarch)
 export LD_LIBRARY_PATH="$root/.deps/sdl/usr/lib/$sdl_triplet${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
 make simulator-test SDL_CFLAGS="-I.deps/sdl/usr/include/SDL2 -I.deps/sdl/usr/include/$sdl_triplet" SDL_LIBS="-L.deps/sdl/usr/lib/$sdl_triplet -lSDL2"
else
 echo 'SDL2 missing; run setup-local-sdl.sh or install SDL2 development files.' >&2
 exit 2
fi
prefix=${PREFIX64:-aarch64-none-elf-}
if ! command -v "${prefix}g++" >/dev/null; then
 echo 'AArch64 bare-metal compiler missing; Circle check has not run.' >&2
 exit 2
fi
make firmware PREFIX64="$prefix"
