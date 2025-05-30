#!/bin/sh

set -e

cmake \
	-S /vanilla \
	-B /build \
	-DVANILLA_BUILD_VENDORED=ON \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_TOOLCHAIN_FILE=/vanilla/docker/macos/toolchain.cmake

cmake --build /build --parallel
mv /build/bin/*.app /install/
