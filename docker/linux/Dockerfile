FROM debian:bookworm

ARG ARCH=x86_64
ENV ARCH=${ARCH}

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN if [ "${ARCH}" = "aarch64" ]; then export DEBARCH="arm64"; export DEBGCC="gcc-aarch64-linux-gnu"; \
    elif [ "${ARCH}" = "x86_64" ]; then export DEBARCH="amd64"; export DEBGCC="gcc"; \
    else export DEBARCH="${ARCH}"; export DEBGCC="gcc"; fi \
    && dpkg --add-architecture $DEBARCH \
    && apt -y update \
    && apt -y install $DEBGCC ninja-build nasm git wget pkg-config make \
    && apt -y install libnl-genl-3-dev:$DEBARCH \
    && apt -y install libssl-dev:$DEBARCH \
    && apt -y install libpulse-dev:$DEBARCH \
    && apt -y install libasound2-dev:$DEBARCH \
    && apt -y install libx11-dev:$DEBARCH \
    && apt -y install libdrm-dev:$DEBARCH \
    && apt -y install libwayland-dev:$DEBARCH \
    && apt -y install libxext-dev:$DEBARCH \
    && apt -y install libnl-route-3-dev:$DEBARCH \
    && apt -y install libxcb-xinput-dev:$DEBARCH
# RUN apt -y install libpipewire-dev:$DEBARCH

# RUN dnf -y update
# RUN dnf -y install 'dnf-command(config-manager)'
# RUN dnf -y config-manager --set-enabled powertools
# RUN dnf -y install gcc-toolset-14
# RUN dnf -y install wget
# RUN dnf -y install git
# RUN dnf -y install nasm
# RUN dnf -y install libnl3-devel
# RUN dnf -y install openssl-devel
# RUN dnf -y install pulseaudio-libs-devel
# RUN dnf -y install alsa-lib-devel
# RUN dnf -y install libX11-devel
# RUN dnf -y install libXext-devel
# RUN dnf -y install wayland-devel
# RUN dnf -y install libdrm-devel

# Install newer version of CMake
ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && chmod +x cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && ./cmake-$CMAKE_VER-linux-$(uname -m).sh --prefix=/usr --skip-license

ENTRYPOINT [ "/vanilla/docker/linux/build.sh" ]
