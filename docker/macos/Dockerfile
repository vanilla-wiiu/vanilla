FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt -y update

RUN apt -y install build-essential git clang patch python3 cmake patch libssl-dev libxml2-dev liblzma-dev cpio libbz2-dev zlib1g-dev bash wget nasm

RUN git clone https://github.com/tpoechtrager/osxcross

RUN wget -c https://archive.org/download/xcode-clt/Command_Line_Tools_for_Xcode_16.4.dmg

RUN /osxcross/tools/gen_sdk_package_tools_dmg.sh /Command_Line_Tools_for_Xcode_*.dmg

RUN rm /Command_Line_Tools_for_Xcode_*.dmg

RUN mv /osxcross/*.sdk.tar.xz /osxcross/tarballs/

RUN SDK_VERSION=14 UNATTENDED=1 /osxcross/build.sh
RUN /osxcross/build_compiler_rt.sh

# Fix linker in osxcross cmake (https://github.com/tpoechtrager/osxcross/issues/454)
RUN echo 'set(CMAKE_LINKER "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-ld" CACHE FILEPATH "ld")' >> /osxcross/target/toolchain.cmake
RUN echo 'set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=${CMAKE_LINKER}")' >> /osxcross/target/toolchain.cmake
RUN echo 'set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=${CMAKE_LINKER}")' >> /osxcross/target/toolchain.cmake
RUN echo 'set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=${CMAKE_LINKER}")' >> /osxcross/target/toolchain.cmake

ENV PATH=/osxcross/target/bin:$PATH

ENTRYPOINT [ "/vanilla/docker/macos/build.sh" ]
