FROM ubuntu:24.04

# Install build dependencies
RUN apt update && \
    apt -y install wget unzip git openjdk-17-jdk make && \
    apt-get clean && \
    apt-get autoremove --yes && \
    rm -rf /var/lib/{apt,dpkg,cache,log}/

# Install Android SDK command line tools
RUN mkdir -p /opt/android/cmdline-tools && \
    wget 'https://dl.google.com/android/repository/commandlinetools-linux-6200805_latest.zip' -P /tmp && \
    unzip -d /opt/android/cmdline-tools /tmp/commandlinetools-linux-6200805_latest.zip

ENV ANDROID_NDK="25.1.8937393"
ENV SYSTEM_ARCH="x86_64"

# Install Android SDK and NDK
RUN yes Y | /opt/android/cmdline-tools/tools/bin/sdkmanager --install "ndk;${ANDROID_NDK}"
# RUN yes Y | /opt/android/cmdline-tools/tools/bin/sdkmanager --install "cmake;3.31.5" "build-tools;35.0.1" "platforms;android-35" "platform-tools" "ndk;28.0.13004108"

# Accept all licenses
RUN yes Y | /opt/android/cmdline-tools/tools/bin/sdkmanager --licenses

# Add ndk-build to the search path
ENV ANDROID_HOME=/opt/android
ENV ANDROID_NDK_HOME=/opt/android/ndk/${ANDROID_NDK}
ENV PATH="$PATH:/opt/gradlew:$ANDROID_HOME/cmdline-tools/tools/bin:/opt/android/platform-tools:$ANDROID_NDK_HOME:$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-${SYSTEM_ARCH}/bin"
COPY "build.sh" "/"

CMD [ "/build.sh" ]