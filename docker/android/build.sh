#!/bin/sh
set -e
cd /vanilla/android
./gradlew -PbuildDir=/build assembleRelease
zipalign -v -p 4 /build/outputs/apk/release/app-release-unsigned.apk /build/outputs/apk/release/app-release-unsigned-aligned.apk

# If signing key is available, sign the APK
if [[ -n "$ANDROID_SIGNING_KEY" ]]
then
    apksigner sign --ks /vanilla/docker/android/vanilla.jks --out /install/app-release.apk --ks-pass pass:$ANDROID_SIGNING_KEY /build/outputs/apk/release/app-release-unsigned-aligned.apk
else
    # Otherwise, copy into the location expected by other scripts
    cp /build/outputs/apk/release/app-release-unsigned-aligned.apk /install/app-release.apk
fi
