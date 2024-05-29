# Vanilla

This is a **work-in-progress** software clone of the Wii U gamepad for Linux (including Raspberry Pi and Steam Deck) and Android. No warranty is provided and everything should be considered **alpha** at best.

<p align="center">
    <a href="https://youtu.be/DSgFu4rDxgc">
        <img align="center" src="https://github.com/vanilla-wiiu/vanilla/assets/64917206/d450047d-0961-4a16-9823-e07f9f9a356c" width="50%">
        <br>
        <i>Announcement Video</i>
    </a>
</p>

## Compiling (Linux)

Vanilla currently requires the following dependencies:

- Debian/Ubuntu
  ```
  # apt install qt6-base-dev qt6-multimedia-dev libavcodec-dev libavutil-dev libavfilter-dev libsdl2-dev libnl-genl-3-dev isc-dhcp-client
  ```
- Arch
  ```
  # pacman -S qt6 ffmpeg libnl sdl2 dhclient
  ```

The build process is otherwise normal for a CMake program:

```
git clone https://github.com/vanilla-wiiu/vanilla.git
cd vanilla
mkdir build
cd build
cmake ..
cmake --build .
```
