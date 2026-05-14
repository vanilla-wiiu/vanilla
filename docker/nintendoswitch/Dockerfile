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
    python3 \
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

# Patch Buildroot to build an older version of X11 because the Tegra BSP isn't compatible with the newer one
COPY buildroot/package/x11r7/* /buildroot/package/x11r7/xserver_xorg-server/

# Build everything except for Vanilla
COPY buildroot /vanilla/buildroot
RUN make -C /buildroot O=/build BR2_EXTERNAL=/vanilla/buildroot vanilla_nintendoswitch_defconfig && \
    /buildroot/utils/config --file /build/.config -d BR2_PACKAGE_VANILLA && \
    make -C /buildroot O=/build && \
    /buildroot/utils/config --file /build/.config -e BR2_PACKAGE_VANILLA && \
    /buildroot/utils/config --file /build/.config -d BR2_PACKAGE_VANILLA_USE_VAAPI && \
    /buildroot/utils/config --file /build/.config -d BR2_PACKAGE_VANILLA_USE_DRM && \
    /buildroot/utils/config --file /build/.config -e BR2_PACKAGE_VANILLA_USE_NX_IMU && \
    /buildroot/utils/config --file /build/.config -e BR2_PACKAGE_VANILLA_USE_LOCAL && \
    /buildroot/utils/config --file /build/.config --set-str BR2_PACKAGE_VANILLA_LOCAL_PATH /vanilla

ENTRYPOINT [ "/vanilla/docker/nintendoswitch/build.sh" ]
