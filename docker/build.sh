#!/bin/sh

# This script will build and name the Docker environment for building Vanilla.

set -e

if [ $# -ne 1 ]
then
  echo "Usage: $0 <architecture>"
  exit 2
fi

echo "Building for architecture: $1"
TAG_NAME=itsmattkc/vanilla-u-build:$1-latest
docker build -t $TAG_NAME $(dirname "$0") --build-arg PLATFORM=$1

echo "Built to: $TAG_NAME"
