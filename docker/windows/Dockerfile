FROM ubuntu:24.04

RUN apt -y update
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt -y install gcc-mingw-w64 git wget make nasm

# Install newer version of CMake
ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && chmod +x cmake-$CMAKE_VER-linux-$(uname -m).sh \
    && ./cmake-$CMAKE_VER-linux-$(uname -m).sh --prefix=/usr --skip-license

COPY "mingw-w64-x86_64.cmake" "/"

ENTRYPOINT [ "/vanilla/docker/windows/build.sh" ]
