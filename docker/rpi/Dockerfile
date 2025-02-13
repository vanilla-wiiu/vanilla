FROM --platform=linux/arm/v6 arm32v6/alpine

RUN apk update
RUN apk upgrade
RUN apk add build-base cmake libnl3-dev openssl-dev sdl2-dev linux-headers ffmpeg-dev eudev-dev libxml2-dev

CMD [ "make", "-j32", "-C", "/data/buildpi" ]
