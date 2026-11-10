#!/bin/sh
set -eu
multiarch=$("${CXX:-c++}" -dumpmachine)
mkdir -p .deps/sdl-packages .deps/sdl
(cd .deps/sdl-packages && apt download libsdl2-dev libsdl2-2.0-0 && for package in ./*.deb; do dpkg-deb -x "$package" ../sdl; done)
cat <<MSG
Build and test:
  export LD_LIBRARY_PATH="$PWD/.deps/sdl/usr/lib/$multiarch\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
  make simulator-test SDL_CFLAGS='-I.deps/sdl/usr/include/SDL2 -I.deps/sdl/usr/include/$multiarch' SDL_LIBS='-L.deps/sdl/usr/lib/$multiarch -lSDL2'
  ./build/virtual-mk3
MSG
