#!/bin/sh

set -e

mkdir /build
cmake -S /vanilla -B /build -DVANILLA_BUILD_VENDORED=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=/mingw-w64-x86_64.cmake
cmake --build /build --parallel
cmake --install /build --prefix "/install"

# HACK: Manual fixup, would be nice to not need this eventually
cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll /install/bin/
