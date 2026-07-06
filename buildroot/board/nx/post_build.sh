#!/bin/sh
set -e

TARGET_DIR="$1"

grep -q '/usr/bin/vanilla-wrapper' "$TARGET_DIR/etc/inittab" || \
    sed -i '/^::sysinit:\/etc\/init\.d\/rcS/a ::once:/usr/bin/vanilla-wrapper' \
        "$TARGET_DIR/etc/inittab"

grep -q '/mnt' "$TARGET_DIR/etc/fstab" || \
    echo "/dev/mmcblk0p1 /mnt vfat defaults 0 0" >> "$TARGET_DIR/etc/fstab"
