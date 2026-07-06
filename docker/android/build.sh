#!/bin/sh
set -e
cd /vanilla/android
./gradlew -PbuildDir=/build assembleRelease
zipalign -v -p 4 /build/outputs/apk/release/app-release-unsigned.apk /install/app-release.apk
