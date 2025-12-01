FROM debian:bookworm

ARG ARCH=x86_64
ENV ARCH=${ARCH}

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt -y update \
RUN apt -y install which \
    sed \
    make \
    binutils \
    build-essential \
    diffutils \
    gcc \
    g++ \
    bash \
    patch \
    gzip \
    bzip2 \
    perl \
    tar \
    cpio \
    unzip \
    rsync \
    file \
    bc \
    findutils \
    awk \
    wget

RUN git clone --depth 1 --branch 2025.11-rc2 https://github.com/buildroot/buildroot.git /buildroot

ENTRYPOINT [ "/vanilla/docker/raspberripi0w/build.sh" ]
