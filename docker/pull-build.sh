#!/bin/bash
#
# This is a convenience script for importing the CI's BuildKit cache, loading a
# runnable image locally, and using it to build Vanilla.
#
# The script assumes you have authenticated with `docker login ghcr.io`
set -e

if [ $# -ne 2 ]; then
    echo "Usage: $0 <os> <arch>   (e.g. linux x86_64)" >&2
    exit 1
fi

OS="$1"
ARCH="$2"
IMAGE="ghcr.io/vanilla-wiiu/vanilla-${OS}-${ARCH}"
LOCAL_IMAGE="vanilla-${OS}-${ARCH}"
SCRIPT_DIR=$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")
ROOT="${SCRIPT_DIR}/.."

docker buildx build \
    -f "${SCRIPT_DIR}/${OS}/Dockerfile" \
    --build-arg ARCH="${ARCH}" \
    -t "${LOCAL_IMAGE}" \
    --load \
    --cache-from "type=registry,ref=${IMAGE}:buildcache" \
    "${ROOT}"

docker run --rm \
    -v "${ROOT}":/vanilla \
    -v "${ROOT}/install":/install \
    "${LOCAL_IMAGE}"
