#!/bin/bash

set -e

make -C /buildroot O=/build

cd /build/images
mv /build/images/sdcard.img /install/
