# RAVENNA Software Development Kit (SDK)

## Introduction

This software library provides a set of tools and libraries to develop applications for RAVENNA Audio over IP
technology. It is based on the [RAVENNA Audio over IP technology](https://ravenna-network.com/technology/).

## Getting started

To learn how to get started integrating RAVENNAKIT into your project, head over to
the [getting started documentation](docs/getting-started.md).

## Prerequisites

To build the project you will need to install the following external dependencies:

- Recent version of Git
- CMake (see CMakeLists.txt for required version)
- Python 3.9 (https://www.python.org/downloads/)
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

## Building for macOS using CMake

To use CMake directly, you can use the following commands:

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

## Building for Windows using CMake

```
# From the project root (generates build folder if necessary)
cmake -B win-build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="submodules\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_OVERLAY_TRIPLETS=triplets -DVCPKG_TARGET_TRIPLET=windows-x64
cmake --build win-build --config Release
```

Note: adjust the parameters to match your platform.
