#!/bin/bash
#
# This is a convenience script for maintainers to locally build a Docker image
# and push its BuildKit cache for use by the CI.
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

docker buildx build \
    -f "${SCRIPT_DIR}/${OS}/Dockerfile" \
    --build-arg ARCH="${ARCH}" \
    -t "${LOCAL_IMAGE}" \
    --label "org.opencontainers.image.source=https://github.com/vanilla-wiiu/vanilla" \
    --load \
    --cache-from "type=registry,ref=${IMAGE}:buildcache" \
    --cache-to "type=registry,ref=${IMAGE}:buildcache,mode=max" \
    "${SCRIPT_DIR}/.."
