#!/bin/sh
set -e

TARGET_DIR="$1"

grep -q '/usr/bin/vanilla' "$TARGET_DIR/etc/inittab" || \
    sed -i '/^::sysinit:\/etc\/init\.d\/rcS/a ::once:/usr/bin/vanilla' \
        "$TARGET_DIR/etc/inittab"
