FROM ubuntu:20.04

# Update apt
RUN apt -y update

# Install latest CMake
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC \
        apt -y install build-essential ninja-build libssl-dev git tzdata
RUN git clone --depth 1 --branch v3.29.3 https://gitlab.kitware.com/cmake/cmake.git \
        && cd cmake \
        && mkdir build \
        && cd build \
        && ../configure \
        && make install \
        && cd ../.. \
        && rm -rf cmake

# Build Qt 6
RUN apt -y install libgles2-mesa-dev libegl1-mesa-dev libfontconfig-dev \
        libxkbcommon-x11-dev libx11-xcb-dev libpulse-dev \
        libxcb-glx0-dev libxcb-cursor-dev libxcb-icccm4-dev \
        libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
        libxcb-render0-dev libxcb-render-util0-dev libxcb-shape0-dev \
        libxcb-shm0-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-xkb-dev
RUN git clone --branch 6.7.1 --depth 1 git://code.qt.io/qt/qt5.git qt6 \
    && cd qt6 \
    && perl init-repository --module-subset=qtbase,qtmultimedia,qtshadertools \
    && mkdir qt6-build \
    && cd qt6-build \
    && ../configure -xcb -verbose -no-feature-ffmpeg -no-feature-forkfd_pidfd -prefix /usr/local \
    && cmake --build . --parallel \
    && cmake --install . \
    && cd ../.. \
    && rm -rf qt6

# Build linuxdeployqt
RUN apt -y install patchelf wget
RUN git clone https://github.com/probonopd/linuxdeployqt.git \
        && cd linuxdeployqt \
        && sed -i "s/find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)/find_package(Qt6 REQUIRED COMPONENTS Core)/g" tools/linuxdeployqt/CMakeLists.txt \
        && mkdir build \
        && cd build \
        && cmake .. \
        && cmake --build . --parallel \
        && cp tools/linuxdeployqt/linuxdeployqt /usr/local/bin \
        && cd ../.. \
        && rm -rf linuxdeployqt

# Other dependencies for Vanilla
RUN apt -y install libavcodec-dev libavfilter-dev libavutil-dev \
        libnl-genl-3-dev libsdl2-dev \
        isc-dhcp-client

# Build Vanilla and deploy
CMD git config --global --add safe.directory /vanilla/lib/hostap \
        && cp /vanilla/lib/hostap/conf/wpa_supplicant.config /vanilla/lib/hostap/wpa_supplicant/.config \
        && /usr/local/bin/cmake /vanilla -B/build -G Ninja -DCMAKE_INSTALL_PREFIX=/AppDir/usr \
        && cmake --build /build --parallel \
        && cmake --install /build \
        && cp $(which dhclient) /AppDir/usr/bin \
        && wget https://raw.githubusercontent.com/isc-projects/dhcp/master/client/scripts/linux -O /AppDir/usr/sbin/dhclient-script \
        && chmod +x /AppDir/usr/sbin/dhclient-script \
        && linuxdeployqt /AppDir/usr/bin/vanilla-gui -executable-dir=/AppDir/usr/bin -bundle-non-qt-libs \
        && tar czf /vanilla/Vanilla_U-$(uname -m).tar.gz -C /AppDir .
