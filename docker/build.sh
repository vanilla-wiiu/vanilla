#!/bin/sh

if [ $# -ne 1 ]
then
  echo "Usage: $0 <architecture>"
  exit 2
fi

echo "Building for architecture: $1"
TAG_NAME=itsmattkc/vanilla-u-build:$1-latest
docker build -t $TAG_NAME --platform $1 .

echo "Built to: $TAG_NAME"
