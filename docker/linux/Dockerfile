FROM debian:bookworm

ARG ARCH=x86_64
ENV ARCH=${ARCH}

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN if [ "${ARCH}" = "aarch64" ]; then export DEBARCH="arm64"; export DEBGCC="gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"; \
    elif [ "${ARCH}" = "x86_64" ]; then export DEBARCH="amd64"; export DEBGCC="gcc g++"; \
    else export DEBARCH="${ARCH}"; export DEBGCC="gcc g++"; fi \
    && dpkg --add-architecture $DEBARCH \
    && apt -y update \
    && apt -y install $DEBGCC ninja-build nasm git wget pkg-config make autoconf libtool bison flex \
        libpulse-dev:$DEBARCH \
        libasound2-dev:$DEBARCH \
        libx11-dev:$DEBARCH \
        libdrm-dev:$DEBARCH \
        libwayland-dev:$DEBARCH \
        libxext-dev:$DEBARCH \
        libxcb-xinput-dev:$DEBARCH \
        libgl1-mesa-dev:$DEBARCH \
        libegl-dev:$DEBARCH \
        libopengl-dev:$DEBARCH \
        libnm-dev:$DEBARCH \
        libdrm-dev:$DEBARCH \
        libxi-dev:$DEBARCH \
        libxcb-xinput-dev:$DEBARCH \
        libdbus-1-dev:$DEBARCH \
        libibus-1.0-dev:$DEBARCH \
        libsndio-dev:$DEBARCH \
        libxrandr-dev:$DEBARCH \
        libsamplerate0-dev:$DEBARCH \
        libpipewire-0.3-dev:$DEBARCH \
        libjack-dev:$DEBARCH \
        libxcursor-dev:$DEBARCH \
        libxss-dev:$DEBARCH \
        libdecor-0-dev:$DEBARCH \
        libudev-dev:$DEBARCH \
        libxinerama-dev:$DEBARCH \
        libxv-dev:$DEBARCH \
        libxxf86vm-dev:$DEBARCH \
        libxt-dev:$DEBARCH \
        libsm-dev:$DEBARCH \
        libice-dev:$DEBARCH \
        libgbm-dev:$DEBARCH \
        libva-dev:$DEBARCH \
        libxkbcommon-dev:$DEBARCH \
        libpolkit-agent-1-dev:$DEBARCH

RUN if [ "${ARCH}" = "x86_64" ]; \
        then sed -i 's/ main$/ main contrib non-free non-free-firmware/' /etc/apt/sources.list.d/debian.sources; \
        apt -y update; \
        apt -y install nvidia-cuda-toolkit; \
    fi

# Install newer version of CMake
ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && chmod +x cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && ./cmake-$CMAKE_VER-linux-$(uname -m).sh --prefix=/usr --skip-license

ENTRYPOINT [ "/vanilla/docker/linux/build.sh" ]
