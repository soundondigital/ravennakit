cmake_minimum_required(VERSION 3.22)

include(submodules/build_tools/cmake/git_version.cmake)

project(ravennakit VERSION ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH} LANGUAGES C CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/helper_targets.cmake)

###########
# Options #
###########

option(RAV_ENABLE_SPDLOG "Enable spdlog for logging" OFF)
option(RAV_ABORT_ON_ASSERT "Abort the program when an assertion is hit" OFF)
option(TRACY_ENABLE "Enable Tracy as profiler" OFF)
option(WITH_ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)
option(WITH_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)
option(BUILD_EXAMPLES "Build the examples" ON)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(BUILD_EXAMPLES OFF)
endif ()

##########################
# Compiler configuration #
##########################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] "YES")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=RelWithDebInfo] "YES")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif ()

if (WITH_ADDRESS_SANITIZER)
    message("-- -DWITH_ADDRESS_SANITIZER=ON argument was specified: Enabling Clang's Address Sanitizer")
    add_compile_options(-fsanitize=address,undefined -g)
    link_libraries(-fsanitize=address,undefined)
endif ()

if (WITH_THREAD_SANITIZER)
    message("-- -DWITH_THREAD_SANITIZER=ON argument was specified: Enabling Clang's Thread Sanitizer")
    add_compile_options(-fsanitize=thread -g)
    link_libraries(-fsanitize=thread)
endif ()

################
# _WIN32_WINNT #
################

if (WIN32)
    # Sets the correct _WIN32_WINNT version based on the OS version
    # Borrowed from: https://stackoverflow.com/a/40217291

    macro(get_WIN32_WINNT version)
        if (CMAKE_SYSTEM_VERSION)
            set(ver ${CMAKE_SYSTEM_VERSION})
            string(REGEX MATCH "^([0-9]+).([0-9])" ver ${ver})
            string(REGEX MATCH "^([0-9]+)" verMajor ${ver})
            # Check for Windows 10, b/c we'll need to convert to hex 'A'.
            if ("${verMajor}" MATCHES "10")
                set(verMajor "A")
                string(REGEX REPLACE "^([0-9]+)" ${verMajor} ver ${ver})
            endif ()
            # Remove all remaining '.' characters.
            string(REPLACE "." "" ver ${ver})
            # Prepend each digit with a zero.
            string(REGEX REPLACE "([0-9A-Z])" "0\\1" ver ${ver})
            set(${version} "0x${ver}")
        endif ()
    endmacro()

    get_WIN32_WINNT(ver)
    add_definitions(-D_WIN32_WINNT=${ver})
endif ()

#########
# Tracy #
#########

set(TRACY_PATH "C:/tracy-0.11.1" CACHE STRING "Path to Tracy")

if (TRACY_ENABLE)
    message(STATUS "Tracy enabled")
    add_library(tracy INTERFACE)
    target_compile_definitions(tracy INTERFACE TRACY_ENABLE=1)

    if (APPLE)
        target_include_directories(tracy INTERFACE /opt/homebrew/include)
        target_link_directories(tracy INTERFACE /opt/homebrew/lib)
        target_link_libraries(tracy INTERFACE TracyClient)
    endif ()

    if (WIN32)
        target_include_directories(tracy INTERFACE "${TRACY_PATH}/public")
        target_sources(tracy INTERFACE "${TRACY_PATH}/public/TracyClient.cpp")
    endif ()
else ()
    message(STATUS "Tracy disabled")
endif ()

#########
# VCPKG #
#########

if (RAV_ENABLE_SPDLOG)
    find_package(spdlog CONFIG REQUIRED)
endif ()

find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
target_compile_definitions(asio::asio INTERFACE ASIO_NO_DEPRECATED ASIO_NO_DYNAMIC_BUFFER_V1)
#target_compile_definitions(asio::asio INTERFACE ASIO_ENABLE_HANDLER_TRACKING)

##############
# RAVENNAKIT #
##############

file(GLOB_RECURSE RAVENNAKIT_HEADER_FILES include/*.hpp)
file(GLOB_RECURSE RAVENNAKIT_SOURCE_FILES src/*.cpp)

add_library(ravennakit STATIC ${RAVENNAKIT_HEADER_FILES} ${RAVENNAKIT_SOURCE_FILES})

target_include_directories(ravennakit PUBLIC include)

target_link_libraries(ravennakit
        PUBLIC
        rav_recommended_warning_flags
        rav_recommended_lto_flags
        asio::asio
        fmt::fmt
        $<$<BOOL:${RAV_ENABLE_SPDLOG}>:spdlog::spdlog>
)

if (APPLE)
    target_link_libraries(ravennakit PUBLIC "-framework SystemConfiguration")
endif ()

target_compile_definitions(ravennakit
        PUBLIC
        NOMINMAX=1
        WIN32_LEAN_AND_MEAN
        RAV_ENABLE_SPDLOG=$<BOOL:${RAV_ENABLE_SPDLOG}>
        RAV_ABORT_ON_ASSERT=$<BOOL:${RAV_ABORT_ON_ASSERT}>
)

if (TRACY_ENABLE)
    target_link_libraries(ravennakit PUBLIC tracy)
endif ()

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

if (${BUILD_EXAMPLES})
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
        if (TRACY_ENABLE)
            target_link_libraries(${FILE_NAME} PUBLIC tracy)
        endif ()
    endforeach ()
endif ()

##############
# Unit Tests #
##############

file(GLOB_RECURSE TEST_SOURCE_FILES test/*.test.cpp)

add_executable(ravennakit_tests ${TEST_SOURCE_FILES})

target_link_libraries(ravennakit_tests
        PUBLIC
        rav_recommended_warning_flags
        rav_recommended_lto_flags
        ravennakit
        PRIVATE
        Catch2::Catch2
)
