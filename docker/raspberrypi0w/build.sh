#!/bin/bash

set -e

make -C /buildroot O=/build BR2_EXTERNAL=/vanilla/buildroot vanilla_raspberrypi0w_defconfig
/buildroot/utils/config --set-str BR2_PACKAGE_VANILLA_LOCAL_PATH /vanilla
make -C /buildroot O=/build

cd /build/images
mv /build/images/sdcard.img /install/
