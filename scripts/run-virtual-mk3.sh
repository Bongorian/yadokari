#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
multiarch=$("${CXX:-c++}" -dumpmachine)
local_sdl="$root/.deps/sdl/usr/lib/$multiarch"
if ! pkg-config --exists sdl2 2>/dev/null && [ -d "$local_sdl" ]; then
 export LD_LIBRARY_PATH="$local_sdl${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
 if [ ! -x build/virtual-mk3 ]; then
  make simulator SDL_CFLAGS="-I.deps/sdl/usr/include/SDL2 -I.deps/sdl/usr/include/$multiarch" SDL_LIBS="-L$local_sdl -lSDL2"
 fi
elif [ ! -x build/virtual-mk3 ]; then
 make simulator
fi
exec ./build/virtual-mk3 "$@"
