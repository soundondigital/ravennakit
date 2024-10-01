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
#include <string>  // For size_t

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define RAV_WINDOWS 1

    #ifdef _WIN64
        #define RAV_WINDOWS_64BIT 1
    #else
        #define RAV_WINDOWS_32BIT 1
    #endif
#elif __APPLE__
    #define RAV_APPLE 1
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR // iOS, tvOS, or watchOS Simulator
        #define RAV_SIMULATOR 1
    #elif TARGET_OS_MACCATALYST // Mac's Catalyst (ports iOS API into Mac, like UIKit).
        #define RAV_MACCATALYST 1
    #elif TARGET_OS_IPHONE // iOS, tvOS, or watchOS device
        #define RAV_IPHONE 1
    #elif TARGET_OS_MAC // Apple desktop OS
        #define RAV_MACOS 1
    #else
        #error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    #define RAV_ANDROID 1
#elif __linux__
    #define RAV_LINUX 1
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    #define RAV_BSD 1
#elif __unix__
    #define RAV_UNIX 1
#elif defined(_POSIX_VERSION)
    #define RAV_POSIX 1
#else
    #error "Unknown platform!"
#endif

#ifdef RAV_WINDOWS
    #ifndef NOMINMAX
        #error "Please define NOMINMAX as compile constant in your build system."
    #endif
#endif
