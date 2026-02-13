# iOS Build Instructions

This script builds the Vanilla library for iOS using CMake. It supports building for both device and simulator architectures.

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
  --clean         Clean build directory before building
  --no-ninja      Do not use Ninja generator even if available
  --no-signing    Skip ad-hoc code signing for simulator builds
  --help          Show this help message
```

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
