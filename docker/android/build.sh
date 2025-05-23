#!/bin/sh
set -e
cd /vanilla/android
./gradlew -PbuildDir=/build assemble
zipalign -v -p 4 /build/outputs/apk/release/app-release-unsigned.apk /build/outputs/apk/release/app-release-unsigned-aligned.apk
apksigner sign --ks /vanilla/docker/android/vanilla.jks --out /install/app-release.apk --ks-pass pass:$ANDROID_SIGNING_KEY /build/outputs/apk/release/app-release-unsigned-aligned.apk
