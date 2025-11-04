#!/bin/sh

EXTRA_CMAKE_FLAGS=

if [ "$ARCH" = "aarch64" ]
then
    EXTRA_CMAKE_FLAGS="-DCMAKE_CROSSCOMPILING=ON -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_SYSTEM_NAME=Linux"
    export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
    export PATH=$PATH:/usr/lib/aarch64-linux-gnu
    export CC=aarch64-linux-gnu-gcc
    export AR=aarch64-linux-gnu-ar
elif [ "$ARCH" = "armhf" ]
then
    EXTRA_CMAKE_FLAGS="-DCMAKE_CROSSCOMPILING=ON -DCMAKE_SYSTEM_PROCESSOR=armhf -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_FLAGS='-march=armv6zk -mfpu=vfp -mfloat-abi=hard -marm'"
    export PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig
    export PATH=$PATH:/usr/lib/arm-linux-gnueabihf
    export CC=arm-linux-gnueabihf-gcc
    export AR=arm-linux-gnueabihf-ar
	export CFLAGS="-march=armv6zk -mfpu=vfp -mfloat-abi=hard -marm"
fi

mkdir -p /build
cmake -S /vanilla -B /build -DVANILLA_BUILD_VENDORED=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVANILLA_INSTALL_POLKIT_RULE=OFF $EXTRA_CMAKE_FLAGS
cmake --build /build --parallel
cmake --install /build --prefix "/install"
