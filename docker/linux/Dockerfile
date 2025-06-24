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
    && apt -y install libpulse-dev:$DEBARCH \
    && apt -y install libasound2-dev:$DEBARCH \
    && apt -y install libx11-dev:$DEBARCH \
    && apt -y install libdrm-dev:$DEBARCH \
    && apt -y install libwayland-dev:$DEBARCH \
    && apt -y install libxext-dev:$DEBARCH \
    && apt -y install libxcb-xinput-dev:$DEBARCH \
    && apt -y install libgl1-mesa-dev:$DEBARCH \
    && apt -y install libopengl-dev:$DEBARCH \
    && apt -y install libnm-dev:$DEBARCH \
    && apt -y install libdrm-dev:$DEBARCH \
    && apt -y install libxi-dev:$DEBARCH \
    && apt -y install libxcb-xinput-dev:$DEBARCH \
    && apt -y install libdbus-1-dev:$DEBARCH \
    && apt -y install libibus-1.0-dev:$DEBARCH \
    && apt -y install libsndio-dev:$DEBARCH \
    && apt -y install libxrandr-dev:$DEBARCH \
    && apt -y install libsamplerate0-dev:$DEBARCH \
    && apt -y install libpipewire-0.3-dev:$DEBARCH \
    && apt -y install libjack-dev:$DEBARCH \
    && apt -y install libxcursor-dev:$DEBARCH \
    && apt -y install libxss-dev:$DEBARCH \
    && apt -y install libdecor-0-dev:$DEBARCH \
    && apt -y install libudev-dev:$DEBARCH \
    && apt -y install libxinerama-dev:$DEBARCH \
    && apt -y install libxv-dev:$DEBARCH \
    && apt -y install libxxf86vm-dev:$DEBARCH \
    && apt -y install libxt-dev:$DEBARCH \
    && apt -y install libsm-dev:$DEBARCH \
    && apt -y install libice-dev:$DEBARCH \
    && apt -y install libgbm-dev:$DEBARCH \
    && apt -y install libxkbcommon-dev:$DEBARCH

# Install newer version of CMake
ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && chmod +x cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && ./cmake-$CMAKE_VER-linux-$(uname -m).sh --prefix=/usr --skip-license

ENTRYPOINT [ "/vanilla/docker/linux/build.sh" ]
