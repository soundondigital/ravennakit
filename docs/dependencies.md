# Dependencies

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
