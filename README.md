# Vanilla

This is a **work-in-progress** software clone of the Wii U gamepad. No warranty is provided and everything should be considered **alpha** at best.

<p align="center">
    <a href="https://youtu.be/DSgFu4rDxgc">
        <img align="center" src="https://i9.ytimg.com/vi_webp/DSgFu4rDxgc/maxresdefault.webp" width="50%">
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
