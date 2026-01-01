#!/bin/bash

# iOS is complex so it will be nice to have a script that can be used outside of
# CI/CD too.

set -eou pipefail

SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"
PROJECT_ROOT="$(realpath "${SCRIPT_DIR}/..")"


BUILD_TYPE="Release"
TARGET_SDK="iphoneos"
CLEAN_BUILD=0
NINJA_EXPLICITLY_DISABLED=0
SKIP_ADHOC_SIGNING=0

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --debug         Build in Debug mode (default: Release)"
    echo "  --simulator     Build for iOS Simulator instead of device"
    echo "  --clean         Clean build directory before building"
    echo "  --no-ninja      Do not use Ninja generator even if available"
    echo "  --no-signing    Skip ad-hoc code signing for simulator builds"
    echo "  --help          Show this help message"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --simulator)
            TARGET_SDK="iphonesimulator"
            shift
            ;;
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --no-ninja)
            NINJA_EXPLICITLY_DISABLED=1
            shift
            ;;
        --no-signing)
            SKIP_ADHOC_SIGNING=1
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Reduce compile time when targeting both simulator and device
# by NOT using the same build directory and NOT require re-configuration / rebuilds.
if [ "$TARGET_SDK" = "iphonesimulator" ]; then
    ARCH=$(uname -m)
    BUILD_DIR="${PROJECT_ROOT}/build/ios-simulator"
    IOS_SIMULATOR_FLAG="-DIOS_SIMULATOR=ON"
else
    ARCH="arm64"
    BUILD_DIR="${PROJECT_ROOT}/build/ios-device"
    IOS_SIMULATOR_FLAG=""
fi

if [ "$CLEAN_BUILD" -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

START_TIME=$(date +%s)

# Check for Ninja
# Ninja is just so much faster than make.
if command -v ninja >/dev/null 2>&1 && [ "$NINJA_EXPLICITLY_DISABLED" -eq 0 ]; then
    echo "Using Ninja generator."
    GENERATOR="-G Ninja"
else
    GENERATOR="-G Unix Makefiles"
fi

echo "Configuring CMake..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/toolchain.cmake" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_OSX_SYSROOT="${TARGET_SDK}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
    -DVANILLA_BUILD_PIPE=OFF \
    -DVANILLA_BUILD_VENDORED=ON \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    ${IOS_SIMULATOR_FLAG} \
    ${GENERATOR} \
    "$PROJECT_ROOT"

echo "Building..."
cmake --build "$BUILD_DIR" --parallel $(sysctl -n hw.ncpu)
echo "Build finished."

APP_DIR="${BUILD_DIR}/bin/Vanilla.app"

if [ "$TARGET_SDK" = "iphonesimulator" ] && [ "$SKIP_ADHOC_SIGNING" -eq 0 ]; then
    echo "Ad-hoc code signing for simulator build..."
    codesign --force --sign - "${APP_DIR}"
fi

END_TIME=$(date +%s)
ELAPSED_TIME=$((END_TIME - START_TIME))
echo "Done in ${ELAPSED_TIME} seconds."
echo "Built app is located at: ${APP_DIR}"
echo ""
