FROM almalinux:8

# RUN apt -y update
# RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt -y install build-essential ninja-build libnl-genl-3-dev libssl-dev nasm git wget pkg-config
RUN dnf -y update
RUN dnf -y install 'dnf-command(config-manager)'
RUN dnf -y config-manager --set-enabled powertools
RUN dnf -y install gcc-toolset-14
RUN dnf -y install wget
RUN dnf -y install git
RUN dnf -y install nasm
RUN dnf -y install libnl3-devel
RUN dnf -y install openssl-devel
RUN dnf -y install pulseaudio-libs-devel
RUN dnf -y install alsa-lib-devel
RUN dnf -y install libX11-devel
RUN dnf -y install libXext-devel
RUN dnf -y install wayland-devel
RUN dnf -y install libdrm-devel

ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && chmod +x cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && ./cmake-$CMAKE_VER-linux-$(uname -m).sh --prefix=/usr --skip-license

COPY "build.sh" "/"

CMD [ "/build.sh" ]
