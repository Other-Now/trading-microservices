#!/usr/bin/env bash
# Configure + build. Uses ~/tg-deps/root (Boost unpacked without sudo) when it
# exists, otherwise the system Boost (CI / Docker: apt install libboost-dev).
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=${BUILD_DIR:-build}
EXTRA=()
if [ -d "$HOME/tg-deps/root/usr/include/boost" ]; then
  EXTRA+=(-DCMAKE_INCLUDE_PATH="$HOME/tg-deps/root/usr/include")
fi
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo} "${EXTRA[@]}" ${CMAKE_ARGS:-} >/dev/null
cmake --build "$BUILD" -j"$(nproc)"
