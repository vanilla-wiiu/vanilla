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

In practice, not all hardware/drivers appear to work at this time. Check the [Wireless Compatibility](/vanilla-wiiu/vanilla/wiki/Wireless-Compatibility) page on the wiki to check if a card is confirmed working or not.

## Compiling (Linux)

Vanilla currently requires the following dependencies:

- Debian/Ubuntu
  ```
  # apt install qt6-base-dev qt6-multimedia-dev libavcodec-dev libavutil-dev libavfilter-dev libsdl2-dev libnl-genl-3-dev isc-dhcp-client
  ```
- Fedora
  ```
  # dnf install qt6-qtbase-devel qt6-qtmultimedia-devel libavcodec-free-devel libavutil-free-devel libavfilter-free-devel libnl3-devel SDL2-devel openssl-devel
  ```
- Arch
  ```
  # pacman -S qt6 ffmpeg libnl sdl2 dhclient
  ```

The build process is otherwise normal for a CMake program:

```
git clone --recursive https://github.com/vanilla-wiiu/vanilla.git
cd vanilla
mkdir build
cd build
cmake ..
cmake --build .
```
