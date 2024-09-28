/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <string> // For size_t

#if defined(_WIN32) || defined(_WIN64)
    #define RAV_WINDOWS 1
    #define RAV_OPERATING_SYSTEM_NAME "Windows"
#elif defined(__ANDROID__)
    #define RAV_ANDROID 1
    #define RAV_OPERATING_SYSTEM_NAME "Android"
#elif defined(LINUX) || defined(__linux__)
    #define RAV_LINUX 1
    #define RAV_OPERATING_SYSTEM_NAME "Linux"
#elif __APPLE__
    #define RAV_APPLE 1
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define RAV_IOS 1
        #define RAV_OPERATING_SYSTEM_NAME "iOS"
    #else
        #define RAV_MACOS 1
        #define RAV_OPERATING_SYSTEM_NAME "macOS"
    #endif
#elif defined(__FreeBSD__) || (__OpenBSD__)
    #define RAV_BSD 1
    #define RAV_OPERATING_SYSTEM_NAME "BSD"
#elif defined(_POSIX_VERSION)
    #define RAV_POSIX 1
    #define RAV_OPERATING_SYSTEM_NAME "Posix"
#else
    #error "Unknown platform!"
#endif

#ifdef RAV_WINDOWS
    #ifndef NOMINMAX
        #error "Please define NOMINMAX as compile constant in your build system."
    #endif
#endif

static_assert(sizeof(size_t) == sizeof(uint64_t), "size_t must be 64-bit");
