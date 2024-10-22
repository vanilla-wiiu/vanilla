# Vanilla

This is a **work-in-progress** software clone of the Wii U gamepad for Linux (including Raspberry Pi and Steam Deck) and Android. No warranty is provided and everything should be considered **alpha** at best.

<p align="center">
    <img src="https://raw.githubusercontent.com/vanilla-wiiu/vanilla/master/images/screenshot1.png">
    <br>
    <a href="https://youtu.be/DSgFu4rDxgc">
        Announcement Video
    </a>
</p>

## What Wi-Fi adapter should I use?

At a minimum, you will need an adapter that supports 802.11n 5GHz. Newer standards (e.g. 802.11ac) are backwards compatible and should work as long as they can run at 5GHz.

In practice, not all hardware/drivers appear to work at this time. Check the [Wireless Compatibility](https://github.com/vanilla-wiiu/vanilla/wiki/Wireless-Compatibility) page on the wiki to check if a card is confirmed working or not.

## Compiling (Linux)
#### Note: Currently Qt 6.4 or later is required
Vanilla currently requires the following dependencies:

- Debian/Ubuntu 
  ```
  # apt install libsystemd-dev qt6-base-dev qt6-multimedia-dev qt6-svg-dev libavcodec-dev libavutil-dev libavfilter-dev libsdl2-dev libnl-genl-3-dev isc-dhcp-client libssl-dev build-essential cmake
  ```
- Fedora
  ```
  # dnf install systemd-devel qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtsvg-devel libavcodec-free-devel libavutil-free-devel libavfilter-free-devel libnl3-devel SDL2-devel openssl-devel make automake gcc gcc-c++ kernel-devel cmake

  ```
- Arch
  ```
  # pacman -S systemd-libs qt6 ffmpeg libnl sdl2 dhclient base-devel make cmake
  ```
- Alpine/postmarketOS
  ```
  # apk add elogind-dev qt6-qtbase-dev qt6-qtmultimedia-dev qt6-qtsvg-dev ffmpeg-dev libnl3-dev sdl2-dev dhclient build-base cmake
  ```

The build process is otherwise normal for a CMake program:

```
git clone --recursive https://github.com/vanilla-wiiu/vanilla.git
cd vanilla
mkdir build && cd build
cmake ..
cmake --build .
```

Optionally, to install the program:

```
sudo cmake --install .
```
