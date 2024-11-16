FROM almalinux:8

# Supports 'x86_64' and 'aarch64'
ARG PLATFORM=x86_64
ENV PREFIX=/devel
ENV RELEASEVER=8
ENV PLATFORM=${PLATFORM}

# Set up environment
RUN dnf -y update
RUN dnf -y install 'dnf-command(config-manager)'
RUN dnf -y config-manager --set-enabled powertools
RUN dnf -y install epel-release
RUN dnf -y install gcc-${PLATFORM}-linux-gnu gcc-c++-${PLATFORM}-linux-gnu
RUN dnf -y install git
RUN dnf -y install gcc-toolset-13
RUN dnf -y install pkg-config

# Install CMake
RUN dnf -y install openssl-devel
RUN source /opt/rh/gcc-toolset-13/enable \
        && git clone --depth 1 --branch v3.29.3 https://gitlab.kitware.com/cmake/cmake.git \
        && cd cmake \
        && mkdir build \
        && cd build \
        && ../configure \
        && make -j$(nproc) install \
        && cd ../.. \
        && rm -rf cmake

# Install Ninja
RUN source /opt/rh/gcc-toolset-13/enable \
        && git clone --depth 1 --branch v1.12.1 https://github.com/ninja-build/ninja.git \
        && cd ninja \
        && mkdir build \
        && cd build \
        && cmake .. \
        && cmake --build . --parallel \
        && cmake --install . \
        && cd ../.. \
        && rm -rf cmake

# Build Qt 6
RUN dnf -y install perl

# Get Qt 6
RUN git clone --branch 6.8.1 --depth 1 git://code.qt.io/qt/qt5.git qt6 \
	&& cd qt6 \
	&& perl init-repository --module-subset=qtbase,qtmultimedia,qtshadertools,qtsvg

RUN dnf -y install pulseaudio-libs-devel mesa-libGL-devel \
        xcb-util-renderutil-devel xcb-util-cursor-devel xcb-util-image-devel \
        libX11-devel xcb-util-wm-devel xcb-util-keysyms-devel libxkbcommon-x11-devel \
        wayland-devel fontconfig-devel gcc-toolset-12-libstdc++-devel \
        glibc-devel pcre2-devel \
        --forcearch ${PLATFORM} --releasever ${RELEASEVER} --installroot ${PREFIX}

ENV CC=${PLATFORM}-linux-gnu-gcc
ENV CXX=${PLATFORM}-linux-gnu-g++
ENV LD=${PLATFORM}-linux-gnu-ld
ENV CFLAGS="-I${PREFIX}/usr/include -I${PREFIX}/usr/local/include -I/usr/lib/gcc/${PLATFORM}-linux-gnu/12/include --sysroot ${PREFIX}"
ENV CXXFLAGS="-I${PREFIX}/usr/include -I${PREFIX}/usr/local/include -I${PREFIX}/opt/rh/gcc-toolset-12/root/usr/include/c++/12 -I${PREFIX}/opt/rh/gcc-toolset-12/root/usr/include/c++/12/${PLATFORM}-redhat-linux --sysroot ${PREFIX}"
ENV LDFLAGS="-L${PREFIX}/opt/rh/gcc-toolset-12/root/usr/lib/gcc/${PLATFORM}-redhat-linux/12 -L${PREFIX}/usr/lib64 -L${PREFIX}/usr/local/lib64 --sysroot ${PREFIX}"
ENV PKG_CONFIG_PATH="${PREFIX}/usr/lib64/pkgconfig:${PREFIX}/usr/share/pkgconfig:${PREFIX}/opt/rh/gcc-toolset-12/root/usr/lib64/pkgconfig"
ENV QEMU_LD_PREFIX=${PREFIX}
ENV PKG_CONFIG_LIBDIR="${PREFIX}/usr/lib64/pkgconfig"
ENV PKG_CONFIG_SYSROOT_DIR=${PREFIX}

# HACK: Helps prevent subsequent builds from trying to link with the system libraries
RUN dnf -y install pkg-config

ENV LD_LIBRARY_PATH=${PREFIX}/usr/lib64
RUN cd qt6 \
        && mkdir qt6-build \
        && cd qt6-build \
        && ../configure -xcb -feature-wayland -verbose -no-feature-gssapi -no-feature-ffmpeg -no-feature-system-zlib -no-feature-forkfd_pidfd -prefix ${PREFIX}/usr -sysroot ${PREFIX} \
        && cmake --build . --parallel \
        && cmake --install . \
        && cd ../.. \
        && rm -rf qt6

# Build SDL2
RUN dnf -y install libXext-devel \
        --forcearch ${PLATFORM} --releasever ${RELEASEVER} --installroot ${PREFIX}
RUN git clone --branch release-2.30.3 --depth 1 https://github.com/libsdl-org/SDL.git \
        && cd SDL \
        && mkdir build \
        && cd build \
        && cmake .. -DCMAKE_PREFIX_PATH=/devel/usr -DCMAKE_INSTALL_PREFIX=/devel/usr \
        && cmake --build . --parallel \
        && cmake --install . \
        && cd ../.. \
        && rm -rf SDL

# Build FFmpeg
RUN dnf -y install nasm
RUN git clone --branch n7.0.1 --depth 1 https://github.com/FFmpeg/FFmpeg.git \
        && cd FFmpeg \
        && ln -s /usr/bin/$CC /usr/bin/gcc \
        && sed -i "s/#include <linux\\/videodev2.h>/#include <linux\\/videodev2.h>\n#include <linux\\/limits.h>/g" libavcodec/v4l2_m2m.h \
        && ./configure \
                --arch=${PLATFORM} \
                --cc=${CC} \
                --cxx=${CXX} \
                --strip=${PLATFORM}-linux-gnu-strip \
                --host-cflags="${CFLAGS}" \
                --host-cppflags="${CXXFLAGS}" \
                --enable-cross-compile \
                --prefix=/devel/usr \
                --enable-gpl \
                --enable-shared \
                --disable-static \
                --disable-programs \
                --disable-avdevice \
                --disable-doc \
                --disable-debug \
        && make -j$(nproc) install \
        && rm /usr/bin/gcc \
        && cd ../.. \
        && rm -rf FFmpeg

# Other dependencies for Vanilla
RUN dnf -y install libnl3-devel dhcp-client openssl-devel git \
        --forcearch ${PLATFORM} --releasever ${RELEASEVER} --installroot ${PREFIX}

# HACK: This may have been removed by our earlier removal of glibc, but we want it back now
RUN dnf -y install pkg-config wget

# Acquire linuxdeployqt
RUN wget -c https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage \
        && chmod +x linuxdeployqt-continuous-x86_64.AppImage \
        && ./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract \
        && cp -Rav squashfs-root/usr/* /usr \
        && rm -rf squashfs-root

# Hack up the environment for deployment
RUN rm -f /usr/bin/ldd \
        && ln -s ${PREFIX}/usr/bin/ldd /usr/bin/ldd \
        && rm -f /usr/bin/strip \
        && ln -s /usr/bin/${PLATFORM}-linux-gnu-strip /usr/bin/strip \
        && sed -i "s/RTLDLIST=\\/lib\\/ld-linux-${PLATFORM}.so.1/RTLDLIST=\\${PREFIX}\\/usr\\/lib\\/ld-linux-${PLATFORM}.so.1/" ${PREFIX}/usr/bin/ldd

ENV LD_LIBRARY_PATH=${PREFIX}/usr/lib64:${PREFIX}/usr/lib64/pulseaudio:${PREFIX}/usr/lib64/bind9-export:/AppDir/usr/lib

# Build Vanilla and deploy
ENTRYPOINT ["/vanilla/docker/run.sh"]
