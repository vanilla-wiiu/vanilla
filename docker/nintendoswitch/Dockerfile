FROM docker.io/debian:trixie

ARG ARCH=x86_64
ENV ARCH=${ARCH}

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN dpkg --add-architecture i386 && \
    apt -y update && \
    apt -y install which \
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
    gawk \
    wget \
    flex \
    xxd \
    bison \
    libc6:i386 \
    libstdc++6:i386 \
    qpdf \
    zstd \
    git && \
    apt-get clean && \
    apt-get autoremove --yes && \
    rm -rf /var/lib/{apt,dpkg,cache,log}/

RUN git clone --branch 2025.11-rc2 https://github.com/buildroot/buildroot.git /buildroot

ENV FORCE_UNSAFE_CONFIGURE=1

ENTRYPOINT [ "/vanilla/docker/nintendoswitch/build.sh" ]
