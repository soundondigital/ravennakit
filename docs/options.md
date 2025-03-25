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

### CMake options and compile constants

These are options which are to influence the CMake configuration and are also defined as compile constants.

| Compile option    | CMake option               | Description                                                                                           |
|-------------------|----------------------------|-------------------------------------------------------------------------------------------------------|
| RAV_ENABLE_SPDLOG | -DRAV_ENABLE_SPDLOG=ON/OFF | When enabled (recommended), spdlog will be used for logging otherwise logs will be written to stdout. |
| TRACY_ENABLE      | -DRAV_TRACY_ENABLE=ON/OFF  | When enabled, Tracy will be compiled into the library.                                                |

### Compile constants

| Compile option                    | Description                                                                                                        |
|-----------------------------------|--------------------------------------------------------------------------------------------------------------------|
| RAV_LOG_ON_ASSERT=0/1             | When enabled, RAV_ASSERT (and friends) will log a message when the assertion fails.                                |
| RAV_THROW_EXCEPTION_ON_ASSERT=0/1 | When enabled, RAV_ASSERT (and friends) will throw an exception when the assertion fails.                           |
| RAV_ABORT_ON_ASSERT=0/1           | When enabled, RAV_ASSERT (and friends) will call std::abort() when the assertion fails. Very useful for debugging. |

## Environment variables

Aside from build-time options, the library can be influenced by setting environment variables:

| Variable      | Values                                         | Description                   |
|---------------|------------------------------------------------|-------------------------------|
| RAV_LOG_LEVEL | TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF | Sets the the runtime loglevel |
