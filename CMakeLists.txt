cmake_minimum_required(VERSION 3.22)

include(cmake/version.cmake)

project(ravennakit VERSION ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH} LANGUAGES C CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/helper_targets.cmake)

###########
# Options #
###########

option(RAV_ENABLE_SPDLOG "Enable spdlog for logging" OFF)
option(RAV_ABORT_ON_ASSERT "Abort the program when an assertion is hit" OFF)
option(RAV_TRACY_ENABLE "Enable Tracy as profiler" OFF)
option(RAV_WITH_ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)
option(RAV_WITH_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)
option(RAV_EXAMPLES "Build the examples" ON)
option(RAV_TESTS "Build the tests" ON)
option(RAV_BENCHMARKS "Build the benchmarks" OFF)
option(RAV_TOOLS "Build the tools" OFF)

set(RAV_TRACY_PATH "C:/tracy-0.11.1" CACHE STRING "Path to Tracy")
set(RAV_WINDOWS_VERSION "0x0A00" CACHE STRING "The minimum supported version of Windows")

if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(RAV_EXAMPLES OFF)
endif ()

##########################
# Compiler configuration #
##########################

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE INTERNAL "")
    add_link_options("/NODEFAULTLIB:libcmt.lib" "/NODEFAULTLIB:libcmtd.lib")
endif ()

################
# Dependencies #
################

if (RAV_ENABLE_SPDLOG)
    find_package(spdlog CONFIG REQUIRED)
endif ()

find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(nanobench CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)
find_package(boost_url CONFIG REQUIRED)
find_package(boost_circular_buffer CONFIG REQUIRED)
find_package(boost_lockfree CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)

target_compile_definitions(Boost::asio INTERFACE BOOST_ASIO_NO_DEPRECATED)

#########
# Tracy #
#########

if (RAV_TRACY_ENABLE)
    message(STATUS "Tracy enabled")
    add_library(tracy INTERFACE)
    target_compile_definitions(tracy INTERFACE TRACY_ENABLE=1)

    if (APPLE)
        target_include_directories(tracy INTERFACE /opt/homebrew/include)
        target_link_directories(tracy INTERFACE /opt/homebrew/lib)
        target_link_libraries(tracy INTERFACE TracyClient)
    endif ()

    if (WIN32)
        target_include_directories(tracy INTERFACE "${RAV_TRACY_PATH}/public")
        target_sources(tracy INTERFACE "${RAV_TRACY_PATH}/public/TracyClient.cpp")
    endif ()
else ()
    message(STATUS "Tracy disabled")
endif ()

##############
# RAVENNAKIT #
##############

file(GLOB_RECURSE RAVENNAKIT_HEADER_FILES include/*.hpp)
file(GLOB_RECURSE RAVENNAKIT_SOURCE_FILES src/*.cpp)

add_library(ravennakit STATIC ${RAVENNAKIT_HEADER_FILES} ${RAVENNAKIT_SOURCE_FILES})

target_include_directories(ravennakit PUBLIC include)

target_link_libraries(ravennakit
        PRIVATE # We don't want force consumers of ravennakit to use our strict set of warnings
        rav_recommended_warning_flags
        rav_recommended_lto_flags
        PUBLIC
        fmt::fmt
        $<$<BOOL:${RAV_ENABLE_SPDLOG}>:spdlog::spdlog>
        Boost::asio
        Boost::beast
        Boost::url
        Boost::circular_buffer
        Boost::lockfree
        Boost::json
)

if (APPLE)
    target_link_libraries(ravennakit PUBLIC "-framework CoreFoundation -framework SystemConfiguration")
endif ()

target_compile_definitions(ravennakit
        PUBLIC
        NOMINMAX=1
        WIN32_LEAN_AND_MEAN
        RAV_ENABLE_SPDLOG=$<BOOL:${RAV_ENABLE_SPDLOG}>
        RAV_ABORT_ON_ASSERT=$<BOOL:${RAV_ABORT_ON_ASSERT}>
)

if (RAV_TRACY_ENABLE)
    target_link_libraries(ravennakit PUBLIC tracy)
endif ()

# Set Windows version define
if (WIN32)
    target_compile_definitions(ravennakit PUBLIC WINVER=${RAV_WINDOWS_VERSION} _WIN32_WINNT=${RAV_WINDOWS_VERSION})
endif ()

if (RAV_WITH_ADDRESS_SANITIZER)
    message("-- -DRAV_WITH_ADDRESS_SANITIZER=ON argument was specified: Enabling Clang's Address Sanitizer")
    target_compile_options(ravennakit PUBLIC -fsanitize=address,undefined -g)
    target_link_libraries(ravennakit PUBLIC -fsanitize=address,undefined)
endif ()

if (RAV_WITH_THREAD_SANITIZER)
    message("-- -DRAV_WITH_THREAD_SANITIZER=ON argument was specified: Enabling Clang's Thread Sanitizer")
    target_compile_options(ravennakit PUBLIC -fsanitize=thread -g)
    target_link_libraries(ravennakit PUBLIC -fsanitize=thread)
endif ()

#################
# Apple Bonjour #
#################

if (WIN32)
    target_include_directories(ravennakit PUBLIC "C:/Program Files/Bonjour SDK/Include")

    # Link dnssd.lib depending on architecture
    if (MSVC_CXX_ARCHITECTURE_ID MATCHES "64")
        target_link_libraries(ravennakit PUBLIC "C:/Program Files/Bonjour SDK/Lib/x64/dnssd.lib")
    elseif (MSVC_CXX_ARCHITECTURE_ID MATCHES "86")
        target_link_libraries(ravennakit PUBLIC "C:/Program Files/Bonjour SDK/Lib/Win32/dnssd.lib")
    else ()
        message(FATAL_ERROR "Unknown MSVC architecture or non CXX language [${MSVC_CXX_ARCHITECTURE_ID}]")
    endif ()

    target_link_libraries(ravennakit PUBLIC ws2_32)
endif ()

############
# Examples #
############

if (${RAV_EXAMPLES})
    file(GLOB_RECURSE RAVENNAKIT_EXAMPLES "examples/*.cpp")

    # Build example targets based on the .cpp files in the examples subfolder
    foreach (FILE ${RAVENNAKIT_EXAMPLES})
        get_filename_component(FILE_NAME ${FILE} NAME_WE)
        add_executable(${FILE_NAME} ${FILE})
        target_link_libraries(${FILE_NAME}
                PUBLIC
                rav_recommended_warning_flags
                rav_recommended_lto_flags
                PRIVATE
                ravennakit
                portaudio_static
                CLI11::CLI11
        )
        if (RAV_TRACY_ENABLE)
            target_link_libraries(${FILE_NAME} PUBLIC tracy)
        endif ()
    endforeach ()
endif ()

#########
# Tools #
#########

if (${RAV_TOOLS})
    file(GLOB_RECURSE RAVENNAKIT_TOOLS "tools/*.cpp")

    # Build tool targets based on the .cpp files in the tools subfolder
    foreach (FILE ${RAVENNAKIT_TOOLS})
        get_filename_component(FILE_NAME ${FILE} NAME_WE)
        add_executable(${FILE_NAME} ${FILE})
        target_link_libraries(${FILE_NAME}
                PUBLIC
                rav_recommended_warning_flags
                rav_recommended_lto_flags
                PRIVATE
                ravennakit
                portaudio_static
                CLI11::CLI11
        )
        if (RAV_TRACY_ENABLE)
            target_link_libraries(${FILE_NAME} PUBLIC tracy)
        endif ()
    endforeach ()
endif ()

##############
# Unit Tests #
##############

if (${RAV_TESTS})
    file(GLOB_RECURSE TEST_HEADER_FILES test/*.test.hpp)
    file(GLOB_RECURSE TEST_SOURCE_FILES test/*.test.cpp)

    add_executable(ravennakit_tests ${TEST_HEADER_FILES} ${TEST_SOURCE_FILES})

    target_link_libraries(ravennakit_tests
            PUBLIC
            rav_recommended_warning_flags
            rav_recommended_lto_flags
            ravennakit
            PRIVATE
            Catch2::Catch2
    )
endif ()

##############
# Benchmarks #
##############

if (${RAV_BENCHMARKS})
    file(GLOB_RECURSE BENCHMARK_SOURCE_FILES benchmarks/*.bench.cpp)

    add_executable(ravennakit_benchmarks ${BENCHMARK_SOURCE_FILES})

    target_link_libraries(ravennakit_benchmarks
            PUBLIC
            rav_recommended_warning_flags
            rav_recommended_lto_flags
            ravennakit
            PRIVATE
            nanobench::nanobench
            Catch2::Catch2
    )
endif ()
