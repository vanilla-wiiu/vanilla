# iOS build instructions

Vanilla uses CMake to generate an Xcode project for arm64 iOS devices.

## Requirements

- CMake 3.21 or newer
- Xcode 15 or 16 for an iOS 12+ device build
- Xcode 26 or newer for an iOS 15+ device build with a Liquid Glass icon

Select the required Xcode installation with `xcode-select` before configuring if
you have more than one version installed.

When no deployment target is supplied, CMake selects iOS 12 for Xcode 15 or 16 and
iOS 15 for Xcode 26 or newer. Xcode 26 and newer compile the Icon Composer bundle;
older Xcode versions use the classic asset catalog. The CI build remains pinned to
Xcode 16.4, so its artifact covers every 64-bit iPhone and iPad. These defaults are
defined by `cmake/ios-toolchain.cmake`, which CMake loads before initializing its
compilers.

Override the automatic deployment target when configuring if needed:

```bash
cmake -S . -B build/ios-device \
  -G Xcode \
  --toolchain cmake/ios-toolchain.cmake \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
```

## Configure and build

Configure a device build and compile it in Release mode:

```bash
cmake -S . -B build/ios-device \
  -G Xcode \
  --toolchain cmake/ios-toolchain.cmake \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/ios-device --config Release --target vanilla --parallel
```

Use the same generated project for a Debug build:

```bash
cmake --build build/ios-device --config Debug --target vanilla --parallel
```

To clean before rebuilding, pass `--clean-first` to the build command.

The build directory contains a generated `Vanilla.xcodeproj`. Open it in Xcode for
running and debugging:

```bash
open build/ios-device/Vanilla.xcodeproj
```

## Code signing

The generated device project disables code signing so CI and command-line builds
produce an unsigned app without Apple credentials.

To sign a device or distribution build, configure it as above and open the project:

```bash
open build/ios-device/Vanilla.xcodeproj
```

In the `vanilla` target's Build Settings, set **Code Signing Allowed** to `Yes`.
Then use Signing & Capabilities to enable automatic signing and select your Apple
Developer team. The bundle identifier is `com.mattkc.vanilla`; it must be registered
to that team before automatic signing can succeed. Because the Xcode project is
generated, repeat this configuration after deleting or regenerating its build
directory.

Configuring with Xcode 26 or newer automatically selects the iOS 15 deployment
target and the Liquid Glass icon, making it suitable as the starting point for a
manually signed App Store archive.

## Sideloading

The unsigned CI artifacts are intended for tools such as
[Sideloadly](https://sideloadly.io/), which sign the app during installation. To
create the same kind of IPA locally:

```bash
mkdir -p Payload
cp -R build/ios-device/bin/Release/Vanilla.app Payload/
zip -9rX Vanilla-unsigned.ipa Payload
```

## App Store archive

Use Xcode 26 or newer so the build uses the current SDK and Liquid Glass icon.
Generate the project and configure signing through Xcode as described above, then
choose Product > Archive and use Xcode Organizer to validate and distribute it.
