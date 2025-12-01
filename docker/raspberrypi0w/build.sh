#!/bin/bash

make -C /buildroot O=/build BR2_EXTERNAL=/vanilla/buildroot vanilla_raspberrypi0w_defconfig
make -C /buildroot O=/build
