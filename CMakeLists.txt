cmake_minimum_required(VERSION 3.22)

include(submodules/build_tools/cmake/git_version.cmake)

project(ravenna-sdk VERSION ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH} LANGUAGES C CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/HelperTargets.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] "YES")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=RelWithDebInfo] "YES")

option(WITH_ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)
if (WITH_ADDRESS_SANITIZER)
    message("-- -DWITH_ADDRESS_SANITIZER=ON argument was specified: Enabling Clang's Address Sanitizer")
    add_compile_options(-fsanitize=address,undefined -g)
    link_libraries(-fsanitize=address,undefined)
endif ()

option(WITH_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)
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
# VCPKG #
#########

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

#######################
# RAVENNA SDK Library #
#######################

add_library(ravenna_sdk STATIC
        include/ravenna-sdk/containers/BufferView.hpp
        include/ravenna-sdk/rtp/RtpPacketView.hpp
        include/ravenna-sdk/platform/ByteOrder.hpp
        include/ravenna-sdk/util/Util.hpp
        src/ravenna-sdk/rtp/RtpPacketView.cpp
        src/ravenna-sdk/util/empty_source.cpp
)

target_include_directories(ravenna_sdk PUBLIC include)

target_link_libraries(ravenna_sdk
        PUBLIC
        rsdk_recommended_warning_flags
        rsdk_recommended_lto_flags
        asio::asio
        spdlog::spdlog
)

##############################
# Multicast receiver example #
##############################

add_executable(multicast_receiver_example
        examples/multicast_receiver_example.cpp
)

target_link_libraries(multicast_receiver_example
        PUBLIC
        ravenna_sdk
)

############################
# Unicast receiver example #
############################

add_executable(unicast_receiver_example
        examples/unicast_receiver_example.cpp
)

target_link_libraries(unicast_receiver_example
        PUBLIC
        ravenna_sdk
)

##############
# Unit Tests #
##############

add_executable(ravenna_sdk_tests
        test/ravenna-sdk/rtp/RtpPacketView.test.cpp
        test/ravenna-sdk/util/Util.test.cpp
        test/ravenna-sdk/containers/BufferView.test.cpp
        test/ravenna-sdk/platform/ByteOrder.test.cpp
)

target_link_libraries(ravenna_sdk_tests
        PRIVATE
        Catch2::Catch2 Catch2::Catch2WithMain
        ravenna_sdk
)
