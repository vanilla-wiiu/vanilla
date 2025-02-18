#!/bin/sh
source /opt/rh/gcc-toolset-14/enable
mkdir -p /build
cmake -S /vanilla -B /build -DVANILLA_BUILD_VENDORED=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /build --parallel
cmake --install /build --prefix "/install"
