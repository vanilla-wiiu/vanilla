#!/bin/bash

set -e

# Patch Buildroot to build an older version of X11 because the Tegra BSP isn't compatible with the newer one
cp /vanilla/buildroot/package/x11r7/* /buildroot/package/x11r7/xserver_xorg-server/

# Main build
make -C /buildroot O=/build BR2_EXTERNAL=/vanilla/buildroot vanilla_nx_defconfig
/buildroot/utils/config --file /build/.config -e BR2_PACKAGE_VANILLA_USE_LOCAL
/buildroot/utils/config --file /build/.config --set-str BR2_PACKAGE_VANILLA_LOCAL_PATH /vanilla
make -C /buildroot O=/build

# Copy initramfs
mkdir -p /install/bootloader/ini
mkdir -p /install/switchroot/vanilla/data
mv /build/images/rootfs.cpio.uboot /install/switchroot/vanilla/initramfs

# Copy icons
cp /vanilla/gui/res/switch/bootlogo_vanilla.bmp /install/switchroot/vanilla/
cp /vanilla/gui/res/switch/icon_vanilla.bmp /install/switchroot/vanilla/

# Copy bootloader ini
cp /vanilla/gui/res/switch/vanilla.ini /install/bootloader/ini/

# Create U-Boot image of kernel
/build/host/bin/mkimage -A arm64 -O linux -T kernel -C gzip -a 0x80200000 -e 0x80200000 -d /build/images/Image /install/switchroot/vanilla/uImage

# Create combined device tree

wget https://android.googlesource.com/platform/system/libufdt/+archive/refs/heads/master/utils.tar.gz
tar xvf utils.tar.gz --exclude tests --exclude README.md
cp src/mkdtboimg.py /build/host/bin/mkdtimg
chmod /build/host/bin/mkdtimg
rm -rf utils.tar.gz src

/build/host/bin/mkdtimg  /install/switchroot/vanilla/nx-plat.dtimg --page_size=1000 \
        /build/build/linux-linux-theofficialgman-5-2-2026/arch/arm64/boot/dts/tegra210-odin.dtb	 --id=0x4F44494E \
		/build/build/linux-linux-theofficialgman-5-2-2026/arch/arm64/boot/dts/tegra210b01-odin.dtb --id=0x4F44494E --rev=0xb01 \
		/build/build/linux-linux-theofficialgman-5-2-2026/arch/arm64/boot/dts/tegra210b01-vali.dtb --id=0x56414C49 \
		/build/build/linux-linux-theofficialgman-5-2-2026/arch/arm64/boot/dts/tegra210b01-fric.dtb --id=0x46524947

# Copy pre-made config
cp /vanilla/gui/res/switch/config.xml /install/switchroot/vanilla/data/

# Copy support files
cp /vanilla/gui/res/switch/bl31.bin /install/switchroot/vanilla/
cp /vanilla/gui/res/switch/bl33.bin /install/switchroot/vanilla/
cp /vanilla/gui/res/switch/boot.scr /install/switchroot/vanilla/
