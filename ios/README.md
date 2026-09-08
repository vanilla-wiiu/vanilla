# iOS build instructions

Vanilla uses CMake to generate Xcode projects for iOS devices and simulators. The
checked-in presets keep each architecture in a separate directory because the
vendored FFmpeg build targets one architecture at a time.

## Requirements

- CMake 3.21 or newer
- Xcode 15 or 16 for an iOS 12+ device build
- Xcode 26 or newer for an iOS 15+ device build with a Liquid Glass icon

Select the required Xcode installation with `xcode-select` before configuring if
you have more than one version installed.

## Build variants

| Preset | Minimum iOS | Architecture | App icon |
| --- | --- | --- | --- |
| `ios-device` | 12.0 with Xcode 15/16; 15.0 with Xcode 26+ | arm64 | Selected for Xcode version |
| `ios-simulator-arm64` | 15.0 | arm64 | Selected for Xcode version |
| `ios-simulator-x86_64` | 15.0 | x86_64 | Selected for Xcode version |

When no deployment target is supplied, CMake selects iOS 12 for Xcode 15 or 16 and
iOS 15 for Xcode 26 or newer. Xcode 26 and newer compile the Icon Composer bundle;
older Xcode versions use the classic asset catalog. The CI build remains pinned to
Xcode 16.4, so its artifact covers every 64-bit iPhone and iPad. These defaults are
defined by `cmake/ios-toolchain.cmake`, which the shared iOS preset loads before
CMake initializes its compilers.

Override the automatic deployment target when configuring if needed:

```bash
cmake --preset ios-device -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
```

## Configure and build

Configure a device build and compile it in Release mode:

```bash
cmake --preset ios-device
cmake --build --preset ios-device-release
```

Use the corresponding `-debug` build preset for a Debug build. For example:

```bash
cmake --build --preset ios-device-debug
```

For the simulator, select the preset matching the Mac host architecture:

```bash
uname -m
cmake --preset ios-simulator-arm64
cmake --build --preset ios-simulator-arm64-release
```

On an Intel Mac, replace `arm64` with `x86_64`. To clean before rebuilding, pass
`--clean-first` to the build command.

Each build directory contains a generated `Vanilla.xcodeproj`. It can be opened in
Xcode for running and debugging:

```bash
open build/ios-device/Vanilla.xcodeproj
```

## Simulator installation

Start Simulator and find the desired device UUID:

```bash
open -a Simulator
xcrun simctl list devices
```

Install and launch the Release build, adjusting the architecture in the path when
needed:

```bash
xcrun simctl install <UUID> build/ios-simulator-arm64/bin/Release/Vanilla.app
xcrun simctl launch <UUID> com.mattkc.vanilla
```

## Code signing

The generated device project disables code signing so CI and command-line builds
produce an unsigned app without Apple credentials. Simulator builds use an ad-hoc
identity.

To sign a device or distribution build, first generate and open the project:

```bash
cmake --preset ios-device
open build/ios-device/Vanilla.xcodeproj
```

In the `vanilla` target's Build Settings, set **Code Signing Allowed** to `Yes`.
Then use Signing & Capabilities to enable automatic signing and select your Apple
Developer team. The bundle identifier is `com.mattkc.vanilla`; it must be registered
to that team before automatic signing can succeed. Because the Xcode project is
generated, repeat this configuration after deleting or regenerating its build
directory.

Configuring the same preset with Xcode 26 or newer automatically selects the iOS 15
deployment target and the Liquid Glass icon, making it suitable as the starting
point for a manually signed App Store archive.

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
Generate the project and configure signing through Xcode as described above:

```bash
cmake --preset ios-device
open build/ios-device/Vanilla.xcodeproj
```

Choose Product > Archive, then use Xcode Organizer to validate and distribute it.
