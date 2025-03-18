# Building

## Prerequisites

To build the project you will need to install the following external dependencies:

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

All dependencies, except for Tracy, can be managed by vcpkg. There is a vcpkg.json file in the root of the project that
can be used to install the dependencies, but it is recommended to set up CMake to use vcpkg.

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

## Build options

Several variables can be set to influence how the library is built. The following table lists the available options:

## CMake options

| CMake option                        | Description                                          |
|-------------------------------------|------------------------------------------------------|
| -DRAV_WITH_ADDRESS_SANITIZER=ON/OFF | When enabled, the address sanitizer will be engaged. |
| -DRAV_WITH_THREAD_SANITIZER=ON/OFF  | When enabled, the thread sanitizer will be engaged.  |
| -DRAV_EXAMPLES=ON/OFF               | When ON (default), examples will be build            |
| -DRAV_TESTS=ON/OFF                  | When ON (default), tests will be build               |

### Build and CMake options

| Compile option    | CMake option               | Description                                                                                           |
|-------------------|----------------------------|-------------------------------------------------------------------------------------------------------|
| RAV_ENABLE_SPDLOG | -DRAV_ENABLE_SPDLOG=ON/OFF | When enabled (recommended), spdlog will be used for logging otherwise logs will be written to stdout. |
| TRACY_ENABLE      | -DRAV_TRACY_ENABLE=ON/OFF  | When enabled, Tracy will be compiled into the library.                                                |

## Environment variables

Aside from build-time options, the library can be influenced by setting environment variables:

| Variable      | Values                                         | Description                   |
|---------------|------------------------------------------------|-------------------------------|
| RAV_LOG_LEVEL | TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF | Sets the the runtime loglevel |

