# Vanilla

This is a **work-in-progress** software clone of the Wii U gamepad. No warranty is provided and everything should be considered **alpha** at best.

<p align="center">
    <img src="https://raw.githubusercontent.com/vanilla-wiiu/vanilla/master/images/screenshot1.png">
    <br>
    <a href="https://youtu.be/DSgFu4rDxgc">
        Announcement Video
    </a>
</p>

## Usage/Installing

Official builds are provided for all supported platforms on the [Releases](https://github.com/vanilla-wiiu/vanilla/releases) page. Most users are recommended to use these.

### Distro-specific packages

Vanilla is also available in the package managers of certain Linux distributions.

#### Arch (AUR)

An Arch User Repository (AUR) package called `vanilla-wiiu-git` is available for easy installation on Arch Linux and derivatives. For more information, see the AUR page: [https://aur.archlinux.org/packages/vanilla-wiiu-git](https://aur.archlinux.org/packages/vanilla-wiiu-git)

## What devices are supported?

- [Steam Deck](https://github.com/vanilla-wiiu/vanilla/wiki/Steam-Deck)
- [Linux](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide) (check [Wi-Fi hardware compatibility](https://github.com/vanilla-wiiu/vanilla/wiki/Wireless-Compatibility))
- [Nintendo Switch](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide) (requires [Broadcom firmware patch](https://github.com/vanilla-wiiu/nexmon) or [external Wi-Fi adapter](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide#nintendo-switch))
- [Android](https://github.com/vanilla-wiiu/vanilla/wiki/Android-Setup-Guide) (currently frontend only)
- [Windows](https://github.com/vanilla-wiiu/vanilla/wiki/Windows-Setup-Guide) (currently frontend only)
- [Raspberry Pi](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide) (requires [external Wi-Fi adapter](https://github.com/vanilla-wiiu/vanilla/wiki/Linux-Setup-Guide#raspberry-pi))
- *iOS* - Coming soon

## What Wi-Fi adapter should I use?

At a minimum, you will need an adapter that supports 802.11n 5GHz. Newer standards (e.g. 802.11ac) are backwards compatible and should work as long as they can run at 5GHz.

In practice, not all hardware/drivers appear to work at this time. Check the [Wireless Compatibility](https://github.com/vanilla-wiiu/vanilla/wiki/Wireless-Compatibility) page on the wiki to check if a card is confirmed working or not.

## Compiling (Linux)
Vanilla currently requires the following dependencies:

- **Debian/Ubuntu**
  ```sh
  apt install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libnl-genl-3-dev libnl-route-3-dev libssl-dev libxml2-dev libnm-dev libpolkit-agent-1-dev
  ```
- **Fedora**
  ```sh
  dnf install libavcodec-free-devel libavutil-free-devel libavfilter-free-devel libnl3-devel SDL2-devel SDL2_image-devel SDL2_ttf-devel openssl-devel make automake gcc gcc-c++ kernel-devel cmake libxml2-devel NetworkManager-libnm-devel polkit-devel
  ```
- **Arch**
  ```sh
  pacman -S base-devel make cmake ffmpeg libnl sdl2 sdl2_image sdl2_ttf libxml2 libnm openssl polkit
  ```
- **Alpine/postmarketOS**
  ```sh
  apk add build-base cmake sdl2-dev sdl2_image-dev sdl2_ttf-dev ffmpeg-dev libnl3-dev libxml2-dev openssl-dev networkmanager-dev polkit-dev
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
