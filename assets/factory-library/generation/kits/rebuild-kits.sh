#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"; cd "$ROOT"
V(){ make -s --eval "print:;@echo \$($1)" print; }
HOST_FLAGS=$(V HOST_FLAGS); SAN=$(V SAN)
ETO=$(V ENGINE_TOOL_OBJECTS); HSO=$(V HOST_SUPPORT_OBJECTS); PLO=$(V PL_OBJECTS); UIA=$(V UI_BASE_ARCHIVE)
WARN="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs"
g++ $HOST_FLAGS -O2 $WARN -c tools/content/user_kits_v12.cpp -o build/user-kits-v12-tool.o
[ -f build/user-kits-v12-engine.o ] || ld -r $ETO -o build/user-kits-v12-engine.o
g++ $HOST_FLAGS -O2 $WARN $SAN build/user-kits-v12-tool.o build/user-kits-v12-engine.o $HSO $PLO -o build/user-kits-v12 $UIA
rm -rf build/factory-library-out
./build/user-kits-v12 build build/factory-library-out > build/user-kits-v12-build-factory.log 2>&1
grep -c "^KIT" build/user-kits-v12-build-factory.log
# staging for verify
rm -rf build/staging-v12-factory
mkdir -p build/staging-v12-factory/FACTORY/KITS
cp -r build/factory-library-out/FACTORY/KITS/LIBRARY build/staging-v12-factory/FACTORY/KITS/
cp -r build/staging-v12/ASSETS build/staging-v12-factory/
cd build/staging-v12-factory
/build/user-kits-v12 verify > /build/user-kits-v12-verify-factory.log 2>&1 || true
grep -c "VERIFY OK" /build/user-kits-v12-verify-factory.log
grep "FAIL\|NOTE" /build/user-kits-v12-verify-factory.log | head -5
