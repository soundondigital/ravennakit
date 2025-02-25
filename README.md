# RAVENNA Software Development Kit (SDK)

## Introduction

This software library provides a set of tools and libraries to develop applications for RAVENNA Audio over IP
technology. It is based on the [RAVENNA Audio over IP technology](https://ravenna-network.com/technology/).

## Prerequisites

To build and run this project you will need to install the following external dependencies:

- Recent version of Git
- CMake (see CMakeLists.txt for required version)
- Python 3.9 (https://www.python.org/downloads/)
- Windows: Visual Studio (Community | Professional | Enterprise) 2019 or higher
- macOS: Xcode 12 or higher
- macOS: Homebrew

## Dependencies

This project requires the following dependencies:

- [spdlog](https://github.com/gabime/spdlog) (MIT License)
- [fmt](https://github.com/fmtlib/fmt) (MIT License)
- [asio](https://github.com/chriskohlhoff/asio) (Boost Software License)
- [tl-expected](https://github.com/TartanLlama/expected) (CC0 1.0 Universal)

Optional:
- [tracy-profiler](https://github.com/wolfpld/tracy) (BSD 3-Clause License)

Unit tests:
- [Catch2](https://github.com/catchorg/Catch2) (Boost Software License)

Examples:
- [portaudio](https://github.com/PortAudio/portaudio) (MIT License)
- [cli11](https://github.com/CLIUtils/CLI11) (BSD 3-Clause License)

All dependencies, except for Tracy, are managed by vcpkg.

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

In the root you'll find a script called build.py. This script builds the project in all supported configurations and runs the unit test. This script is considered the single source of truth on how to build, pack and distribute the library and is supposed to be called by a CI script.

For all platforms, using sensible defaults:
```
python3 -u build.py
```

For more options and info run:
```
python3 -u build.py --help
```

## How to build the project for macOS using CMake

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

## How to build the project for Windows using CMake

```
# From the project root (generates build folder if necessary)
cmake -B win-build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="submodules\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_OVERLAY_TRIPLETS=triplets -DVCPKG_TARGET_TRIPLET=windows-x64
cmake --build win-build --config Release
```

Note: adjust the parameters to match your platform.

## How to configure CLion to use vcpkg

Add the following CMake options:

```
-DCMAKE_TOOLCHAIN_FILE=submodules/vcpkg/scripts/buildsystems/vcpkg.cmake
-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
-DVCPKG_OVERLAY_TRIPLETS=../triplets
-DVCPKG_TARGET_TRIPLET=macos-arm64 
```

Note: change the triplet to match your cpu architecture.

## CMake options

| CMake option                        | Description                                          |
|-------------------------------------|------------------------------------------------------|
| -DRAV_WITH_ADDRESS_SANITIZER=ON/OFF | When enabled, the address sanitizer will be engaged. |
| -DRAV_WITH_THREAD_SANITIZER=ON/OFF  | When enabled, the thread sanitizer will be engaged.  |
| -DRAV_EXAMPLES=ON/OFF               | When ON (default), examples will be build            |
| -DRAV_TESTS=ON/OFF                  | When ON (default), tests will be build               |

## Build and CMake options

| Compile option    | CMake option               | Description                                                                             |
|-------------------|----------------------------|-----------------------------------------------------------------------------------------|
| RAV_ENABLE_SPDLOG | -DRAV_ENABLE_SPDLOG=ON/OFF | When enabled, spdlog will be used for logging otherwise logs will be written to stdout. |
| TRACY_ENABLE      | -DRAV_TRACY_ENABLE=ON/OFF  | When enabled, Tracy will be compiled into the library.                                  |

## Environment variables

| Variable      | Values                                         | Description          |
|---------------|------------------------------------------------|----------------------|
| RAV_LOG_LEVEL | TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF | The runtime loglevel |

## RFCs

RFC 3550: RTP: A Transport Protocol for Real-Time Applications  
https://datatracker.ietf.org/doc/html/rfc3550

RFC 3551: RTP Profile for Audio and Video Conferences with Minimal Control  
https://datatracker.ietf.org/doc/html/rfc3551

RFC 3190: RTP Payload Format for 12-bit DAT Audio and 20- and 24-bit Linear Sampled Audio  
https://datatracker.ietf.org/doc/html/rfc3190

RFC 8866: SDP: Session Description Protocol  
https://datatracker.ietf.org/doc/html/rfc8866

RFC 4570: Session Description Protocol (SDP) Source Filters  
https://datatracker.ietf.org/doc/html/rfc4570

RFC 2326: Real Time Streaming Protocol (RTSP)  
https://datatracker.ietf.org/doc/html/rfc2326

RFC 2068: Hypertext Transfer Protocol -- HTTP/1.1  
https://datatracker.ietf.org/doc/html/rfc2068
