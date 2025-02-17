FROM ubuntu:24.04

ENV NDK_VERSION="android-ndk-r28"

# Installing packages and glibc
RUN apt update && \
    apt -y install wget unzip git && \
    apt-get clean && \
    apt-get autoremove --yes && \
    rm -rf /var/lib/{apt,dpkg,cache,log}/

# Download and unzip the Android NDK
RUN wget -O /${NDK_VERSION}.zip https://dl.google.com/android/repository/${NDK_VERSION}-linux.zip && \
	unzip /${NDK_VERSION}.zip && \
	rm /${NDK_VERSION}.zip

# Build SDL2
RUN git clone --depth 1 --branch release-2.30.12 https://github.com/libsdl-org/SDL.git

# Build FFmpeg

# Build 

# Add ndk-build to the search path
ENV PATH="/${NDK_VERSION}/:${PATH}"