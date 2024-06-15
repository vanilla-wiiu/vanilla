FROM almalinux:8

# Update dnf
RUN dnf -y update

# Set up environment
RUN dnf -y install gcc-toolset-13
ENV PATH="/opt/rh/gcc-toolset-13/root/usr/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/rh/gcc-toolset-13/root:/usr/lib64:/usr/lib:/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH}"
ENV PKG_CONFIG_PATH="/opt/rh/gcc-toolset-13/root/usr/lib64/pkgconfig:${PKG_CONFIG_PATH}"

# Install latest CMake
RUN dnf -y install openssl-devel git
RUN git clone --depth 1 --branch v3.29.3 https://gitlab.kitware.com/cmake/cmake.git \
        && cd cmake \
        && mkdir build \
        && cd build \
        && ../configure \
        && make -j$(nproc) install \
        && cd ../.. \
        && rm -rf cmake

# Build Qt 6
RUN dnf -y install 'dnf-command(config-manager)' && dnf config-manager --set-enabled powertools
RUN dnf install epel-release -y
RUN dnf -y install perl pulseaudio-libs-devel mesa-libGL-devel \
        xcb-util-renderutil-devel xcb-util-cursor-devel xcb-util-image-devel \
        libX11-devel xcb-util-wm-devel xcb-util-keysyms-devel libxkbcommon-x11-devel \
        wayland-devel fontconfig-devel
RUN git clone --branch 6.7.1 --depth 1 git://code.qt.io/qt/qt5.git qt6 \
        && cd qt6 \
        && perl init-repository --module-subset=qtbase,qtmultimedia,qtshadertools \
        && mkdir qt6-build \
        && cd qt6-build \
        && ../configure -xcb -feature-wayland -verbose -no-feature-ffmpeg -no-feature-forkfd_pidfd -prefix /usr/local \
        && cmake --build . --parallel \
        && cmake --install . \
        && cd ../.. \
        && rm -rf qt6

# Build linuxdeployqt
RUN dnf -y install patchelf wget
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

# Build SDL2
RUN dnf -y install libXext-devel
RUN git clone --branch release-2.30.3 --depth 1 https://github.com/libsdl-org/SDL.git \
        && cd SDL \
        && mkdir build \
        && cd build \
        && cmake .. \
        && cmake --build . --parallel \
        && cmake --install . \
        && cd ../.. \
        && rm -rf SDL

# Build FFmpeg
RUN dnf -y install nasm
RUN git clone --branch n7.0.1 --depth 1 https://github.com/FFmpeg/FFmpeg.git \
        && cd FFmpeg \
        && ./configure --enable-gpl --enable-shared --disable-static --disable-programs --disable-avdevice --disable-doc --disable-debug \
        && make -j$(nproc) install

# Other dependencies for Vanilla
RUN dnf -y install libnl3-devel dhcp-client which

# Build Vanilla and deploy
ENTRYPOINT ["/vanilla/docker/run.sh"]
