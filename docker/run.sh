#!/bin/sh

# This is the entrypoint script for the Docker image. It is not intended to be
# run outside of the Docker environment.

set -e

CREATE_ARCHIVE=1

for i in "$@" ; do
    if [ "$i" = "--no-archive" ] ; then
        CREATE_ARCHIVE=0
    fi
done

echo "Building Vanilla.."
echo "  Building TAR: $CREATE_ARCHIVE"

git config --global --add safe.directory /vanilla/lib/hostap
cp /vanilla/lib/hostap/conf/wpa_supplicant.config /vanilla/lib/hostap/wpa_supplicant/.config
/usr/local/bin/cmake /vanilla -B/build -DCMAKE_PREFIX_PATH=${PREFIX}/usr -DCMAKE_INSTALL_PREFIX=/AppDir/usr
cmake --build /build --parallel
cmake --install /build
cp ${PREFIX}/usr/sbin/dhclient /AppDir/usr/bin
mkdir -p /AppDir/usr/sbin
wget https://raw.githubusercontent.com/isc-projects/dhcp/master/client/scripts/linux -O /AppDir/usr/sbin/dhclient-script
chmod +x /AppDir/usr/sbin/dhclient-script
linuxdeployqt /AppDir/usr/bin/vanilla-gui -executable-dir=/AppDir/usr/bin -bundle-non-qt-libs -qmake=${PREFIX}/usr/bin/qmake

if [ "$CREATE_ARCHIVE" -eq 1 ]; then
	tar czf /vanilla/Vanilla_U-${PLATFORM}.tar.gz -C /AppDir .
fi
