# RAVENNA Software Development Kit (SDK)

## Introduction

This repository provides a cross-platform C++ SDK for professional networked audio using AES67, RAVENNA, and ST2110-30.
It runs on macOS, Windows, Linux, and in virtualized or containerized environments, enabling low-latency audio I/O for
desktop applications and cloud deployments.

[Skip to Getting started](#getting-started)

## What's Included

### RAVENNA / AES67 / ST2110-30 
Full support for RAVENNA as specified by the RAVENNA protocol including AES67 and ST2110-30 ([ravennakit/ravenna](include/ravennakit/ravenna)).

### NMOS

NMOS IS-04 for discovery and IS-05 for connection management ([ravennakit/nmos](include/ravennakit/nmos)).

### RTP  
An implementation of RTP and RTCP to support the main audio-over-IP protocols ([ravennakit/rtp](include/ravennakit/rtp)).

### DNS-SD  
DNS-SD support for device discovery on local networks. Currently implemented for macOS and Windows; Linux support is
planned ([ravennakit/dnssd](include/ravennakit/dnssd)).

### PTPv2  
A virtual PTP follower based on IEEE 1588-2019 ([ravennakit/ptp](include/ravennakit/ptp)).

### RTSP
RTSP client and server implementation used for connection management in RAVENNA workflows ([ravennakit/rtsp](include/ravennakit/rtsp)).

### SDP  
Session Description Protocol (SDP) parsing and generation to support signaling between devices ([ravennakit/sdp](include/ravennakit/sdp)).

### Core Utilities
A rich set of utilities for audio buffers, audio formats, generic containers, streams, lock-free programming, integer
wraparound, URIs, and more to support all of the above ([ravennakit/core](include/ravennakit/core)).

## Demo application

A full JUCE base example app can be found [here](https://github.com/soundondigital/ravennakit_juce_demo). The source
code is available as well as pre-built binaries and installers.

## Commercial Support & Licensing

The SDK is released under the AGPLv3 license. This ensures that the core remains open and that improvements can be
shared with the community. If AGPLv3 works for your project, you are free to use the SDK under its terms.

If you cannot or do not want to open source your product under AGPLv3 a commercial license is available. This license
allows you to:

- Use the SDK in proprietary products without the copyleft requirements of AGPLv3.
- Keep your application source code closed while still benefiting from the SDK.
- Optionally bundle the SDK as part of a larger commercial offering.

### Commercial Support

If you are integrating AES67 / RAVENNA / ST2110-30 into a product and want to reduce risk and time-to-market, we can
help with:

- Integration support. Guidance and hands-on help integrating the SDK into your existing architecture.
- Feature development & extensions. Development of new features, protocol extensions, or optimizations specific to your
  use case.
- Performance & reliability tuning. Profiling, troubleshooting, and improving latency, robustness, and scalability in
  real-world network conditions.

### Get in Touch

For commercial support or licensing inquiries, please contact us at: https://ravennakit.com/contact/

## Getting started

To learn how to get started integrating RAVENNAKIT into your project, head over to
the [getting started documentation](docs/getting-started.md).

Reference documentation can be found at: https://reference.ravennakit.com/

## Prerequisites

To build the project you will need to install the following external dependencies:

- Recent version of Git
- CMake (see CMakeLists.txt for required version)
- Python 3.9 or higher
- Windows: Visual Studio (Community | Professional | Enterprise) 2019 or higher
- macOS: Xcode 12 or higher
- macOS: Homebrew

## Dependencies

Head over to [dependencies](docs/dependencies.md) for more information on the required C++ dependencies.

## Prepare the build environment

### macOS

Install vcpkg dependencies:

```
brew install pkg-config
```

Install build.py dependencies:

```
pip install pygit2
```

## How to build the project using build.py

In the root you'll find a script called build.py. This script builds the project in all supported configurations and
runs the unit test. This script is considered the single source of truth on how to build, pack and distribute the
library and is intended to be called by a CI/CD script.

To build for the current platform, using sensible defaults:

```
python3 -u build.py
```

For more options and info run:

```
python3 -u build.py --help
```

## Building using CMake

To use CMake directly, you can use the following commands:

### macOS

```
# From the project root (generates build folder if necessary)
cmake -B xcode-build -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_NUMBER=0 \
    -DCMAKE_TOOLCHAIN_FILE=submodules/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
    -DVCPKG_OVERLAY_TRIPLETS=triplets \
    -DVCPKG_TARGET_TRIPLET=macos-$(uname -m) \
    -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
cmake --build xcode-build --config Release
```

Note: adjust the parameters to match your platform.

### Linux

```
# From the project root (generates build folder if necessary)
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=submodules/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_OVERLAY_TRIPLETS=triplets \
    -DVCPKG_TARGET_TRIPLET=linux-x64
cmake --build build --config Release
```

Note: adjust the parameters to match your platform.

### Windows

```
# From the project root (generates build folder if necessary)
cmake -B win-build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="submodules\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_OVERLAY_TRIPLETS=triplets -DVCPKG_TARGET_TRIPLET=windows-x64
cmake --build win-build --config Release
```

Note: adjust the parameters to match your platform.
