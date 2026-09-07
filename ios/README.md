# iOS build instructions

Vanilla uses CMake to generate an Xcode project for each supported iOS SDK and
architecture. The checked-in presets keep device, legacy, and simulator builds in
separate directories because the vendored FFmpeg build targets one architecture at
a time.

## Requirements

- CMake 3.21 or newer
- Xcode 26 for the modern iOS 15+ build
- Xcode 15 or 16 for the legacy iOS 12+ build

Select the required Xcode installation with `xcode-select` before configuring if
you have more than one version installed.

## Build variants

| Preset | Minimum iOS | Architecture | App icon |
| --- | --- | --- | --- |
| `ios-device-modern` | 15.0 | arm64 | Icon Composer `AppIcon.icon` |
| `ios-device-legacy` | 12.0 | arm64 | Classic `Assets.xcassets` catalog |
| `ios-simulator-arm64` | 15.0 | arm64 | Icon Composer `AppIcon.icon` |
| `ios-simulator-x86_64` | 15.0 | x86_64 | Icon Composer `AppIcon.icon` |

The legacy device build covers every 64-bit iPhone and iPad. It requires Xcode 15
or 16 because Xcode 26 cannot target iOS versions below 15 and older Xcode versions
cannot compile the Icon Composer bundle used by the modern build.

## Configure and build

Configure a modern device build and compile it in Release mode:

```bash
cmake --preset ios-device-modern
cmake --build --preset ios-device-modern-release
```

Use the corresponding `-debug` build preset for a Debug build. For example:

```bash
cmake --build --preset ios-device-modern-debug
```

Legacy device builds use their own configure and build presets:

```bash
cmake --preset ios-device-legacy
cmake --build --preset ios-device-legacy-release
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
open build/ios-device-modern/Vanilla.xcodeproj
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

The checked-in device presets disable code signing so pull-request CI can compile
the app without Apple credentials. Simulator builds use an ad-hoc identity. To let
Xcode sign a local device or distribution build, reconfigure with your Apple
development team:

```bash
cmake --preset ios-device-modern \
  -DVANILLA_IOS_CODE_SIGNING=ON \
  -DVANILLA_IOS_DEVELOPMENT_TEAM=<TEAM_ID>
cmake --build --preset ios-device-modern-release
```

The bundle identifier is `com.mattkc.vanilla`; it must be registered to the selected
team before automatic signing can succeed.

## Sideloading

The unsigned CI artifacts are intended for tools such as
[Sideloadly](https://sideloadly.io/), which sign the app during installation. To
create the same kind of IPA locally:

```bash
mkdir -p Payload
cp -R build/ios-device-modern/bin/Release/Vanilla.app Payload/
zip -9rX Vanilla-unsigned.ipa Payload
```

## App Store archive

An App Store build must be signed, archived, validated, and exported or uploaded;
renaming an unsigned ZIP to `.ipa` is not sufficient. After configuring with code
signing enabled, create an archive from the generated scheme:

```bash
xcodebuild \
  -project build/ios-device-modern/Vanilla.xcodeproj \
  -scheme vanilla \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  -archivePath "$PWD/build/Vanilla.xcarchive" \
  archive
```

Open the archive in Xcode Organizer to validate and distribute it, or export it from
CI with `xcodebuild -exportArchive` and an appropriate `ExportOptions.plist` stored
outside the repository.
