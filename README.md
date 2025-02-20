# Vanilla

This is a **work-in-progress** software clone of the Wii U gamepad for Linux (including Raspberry Pi and Steam Deck) and Android. No warranty is provided and everything should be considered **alpha** at best.

<p align="center">
    <img src="https://raw.githubusercontent.com/vanilla-wiiu/vanilla/master/images/screenshot1.png">
    <br>
    <a href="https://youtu.be/DSgFu4rDxgc">
        Announcement Video
    </a>
</p>

## What devices are supported?

- [Steam Deck](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide)
- [Nintendo Switch](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide)
- [Android](https://github.com/vanilla-wiiu/vanilla/wiki/Android-Setup-Guide)
- [Linux](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide)
- [Windows](https://github.com/vanilla-wiiu/vanilla/wiki/Windows-Setup-Guide)
- [Raspberry Pi](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide)
- *iOS* - Coming soon

## What Wi-Fi adapter should I use?

At a minimum, you will need an adapter that supports 802.11n 5GHz. Newer standards (e.g. 802.11ac) are backwards compatible and should work as long as they can run at 5GHz.

In practice, not all hardware/drivers appear to work at this time. Check the [Wireless Compatibility](https://github.com/vanilla-wiiu/vanilla/wiki/Wireless-Compatibility) page on the wiki to check if a card is confirmed working or not.

## Compiling (Linux)
Vanilla currently requires the following dependencies:

- Debian/Ubuntu 
  ```sh
  apt install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libnl-genl-3-dev libnl-route-3-dev libssl-dev libxml2-dev
  ```
- Fedora
  ```sh
  dnf install libavcodec-free-devel libavutil-free-devel libavfilter-free-devel libnl3-devel SDL2-devel SDL2_image-devel SDL2_ttf-devel openssl-devel make automake gcc gcc-c++ kernel-devel cmake libxml2-devel
  ```
- Fedora (with RPM Fusion multimedia packages)
  ```
  # dnf install qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtsvg-devel ffmpeg-devel libnl3-devel SDL2-devel openssl-devel make automake gcc gcc-c++ kernel-devel cmake
  ```
- Arch
  ```sh
  pacman -S base-devel make cmake ffmpeg libnl sdl2 sdl2_image sdl2_ttf libxml2
  ```
- Alpine/postmarketOS
  ```sh
  apk add build-base cmake sdl2-dev sdl2_image-dev sdl2_ttf-dev ffmpeg-dev libnl3-dev libxml2-dev
  ```

The build process is otherwise normal for a CMake program:

```
git clone https://github.com/vanilla-wiiu/vanilla.git
cd vanilla
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

Optionally, to install the program:

```
sudo cmake --install .
```
