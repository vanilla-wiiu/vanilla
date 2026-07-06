# iOS Build Instructions

This script builds the Vanilla library for iOS using CMake. It supports building for both device and simulator architectures.

## Build variants

There are two device build variants (both `arm64`):

| | Modern (default) | Legacy (`--legacy`) |
| --- | --- | --- |
| Minimum iOS | 15.0 | 12.0 |
| App icon | `AppIcon.icon` (Icon Composer) | `Assets.xcassets` (classic `.appiconset`) |
| Toolchain | Xcode 26 (iOS 26 SDK) | Xcode 15 / 16 |
| Distribution | App Store or sideload | Sideload |

The **modern** build is the one to submit to the App Store — Apple requires building
with the current SDK, and it uses the Liquid Glass `AppIcon.icon`.

The **legacy** build lowers the deployment target to iOS 12.0, which covers every
64-bit iPhone/iPad ever shipped (iPhone 5s and newer). It's `arm64`-only: the only
devices that can't reach iOS 12 are all 32-bit (`armv7`), and no currently-available
Xcode (or GitHub-hosted runner) can build a 32-bit slice, so iOS 12 is the practical
floor. Note that Xcode 26 cannot target below iOS 15, so the legacy build must be made
with Xcode 15 or 16 — and because those can't read the Icon Composer `AppIcon.icon`
bundle, the legacy build compiles `ios/Assets.xcassets` (a traditional
`AppIcon.appiconset`) instead. CI builds it on the `macos-15` runner.

## Usage

Build the project by running the following command in the terminal:
```bash
./ios/build.sh [options]
```

The options are:
```
Usage: ./ios/build.sh [OPTIONS]

Options:
  --debug         Build in Debug mode (default: Release)
  --simulator     Build for iOS Simulator instead of device
  --legacy        Build the legacy iOS 12.0 back-compat variant (arm64)
                  instead of the modern App Store build (iOS 15). Requires
                  Xcode 15/16 (Xcode 26 cannot target below iOS 15).
  --clean         Clean build directory before building
  --no-ninja      Do not use Ninja generator even if available
  --no-signing    Skip ad-hoc code signing for simulator builds
  --help          Show this help message
```

The legacy build output lands in `build/ios-device-legacy/bin/Vanilla.app` so it does
not clobber the modern `build/ios-device` tree.

## Installing

### In the simulator

Launch the Simulator app.
```bash
open -a Simulator
```

Look at the list of available devices and note down the UUID.
```bash
xcrun simctl list devices
```

Install the built app on the simulator using the UUID.
```bash
xcrun simctl install <UUID> build/ios-simulator/bin/Vanilla.app
```

Launch the app on the simulator.
```bash
xcrun simctl launch <UUID> com.mattkc.Vanilla
```

### On a physical device (Sideloaddy)

Download and install [Sideloaddy](https://sideloadly.io/).

Make an IPA file from the built app.
```bash
cd build/ios-device/bin
mkdir -p Payload
cp -r Vanilla.app Payload/
zip -r Vanilla.ipa Payload
cd ../../..
```

Open Sideloaddy, select the IPA file, and follow the instructions to install it on your device.
