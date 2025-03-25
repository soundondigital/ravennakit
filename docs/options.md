# Options

Several variables can be set to influence how the library is built or run. 
The following tables lists the available options:

## CMake options

These are options which influence the CMake configuration.

| CMake option                        | Description                                          |
|-------------------------------------|------------------------------------------------------|
| -DRAV_WITH_ADDRESS_SANITIZER=ON/OFF | When enabled, the address sanitizer will be engaged. |
| -DRAV_WITH_THREAD_SANITIZER=ON/OFF  | When enabled, the thread sanitizer will be engaged.  |
| -DRAV_EXAMPLES=ON/OFF               | When ON (default), examples will be build            |
| -DRAV_TESTS=ON/OFF                  | When ON (default), tests will be build               |

### Build and CMake options

These are options which are also defined ad compile constant.

| Compile option    | CMake option               | Description                                                                                           |
|-------------------|----------------------------|-------------------------------------------------------------------------------------------------------|
| RAV_ENABLE_SPDLOG | -DRAV_ENABLE_SPDLOG=ON/OFF | When enabled (recommended), spdlog will be used for logging otherwise logs will be written to stdout. |
| TRACY_ENABLE      | -DRAV_TRACY_ENABLE=ON/OFF  | When enabled, Tracy will be compiled into the library.                                                |

## Environment variables

Aside from build-time options, the library can be influenced by setting environment variables:

| Variable      | Values                                         | Description                   |
|---------------|------------------------------------------------|-------------------------------|
| RAV_LOG_LEVEL | TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF | Sets the the runtime loglevel |
