FROM ubuntu:24.04

RUN apt -y update
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt -y install gcc-mingw-w64 git wget make nasm

ENV CMAKE_VER=3.31.5
RUN wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-linux-x86_64.sh \
    && chmod +x cmake-$CMAKE_VER-linux-x86_64.sh \
    && ./cmake-$CMAKE_VER-linux-x86_64.sh --prefix=/usr --skip-license

COPY "build.sh" "/"
COPY "mingw-w64-x86_64.cmake" "/"

CMD [ "/build.sh" ]
