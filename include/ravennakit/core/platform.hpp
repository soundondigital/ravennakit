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

// Note: these constants are treated as tri-state variables, so they can be 0, 1, or undefined.

// Windows
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define RAV_WINDOWS 1

    #ifdef _WIN64
        #define RAV_WINDOWS_64BIT 1
    #else
        #define RAV_WINDOWS_32BIT 1
    #endif
#else
    #define RAV_WINDOWS 0
    #define RAV_WINDOWS_64BIT 0
    #define RAV_WINDOWS_32BIT 0
#endif

// Apple
#if defined(__APPLE__)
    #define RAV_APPLE 1
    #define RAV_POSIX 1 // POSIX-certified.
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR  // iOS, tvOS, or watchOS Simulator
        #define RAV_SIMULATOR 1
    #elif TARGET_OS_MACCATALYST  // Mac's Catalyst (ports iOS API into Mac, like UIKit).
        #define RAV_MACCATALYST 1
    #elif TARGET_OS_IPHONE  // iOS, tvOS, or watchOS device
        #define RAV_IPHONE 1
    #elif TARGET_OS_MAC  // Apple desktop OS
        #define RAV_MACOS 1
    #else
        #error "Unknown Apple platform"
    #endif
#else
    #define RAV_APPLE 0
    #define RAV_SIMULATOR 0
    #define RAV_MACCATALYST 0
    #define RAV_IPHONE 0
    #define RAV_MACOS 0
#endif

// Android
#if defined(__ANDROID__)
    #define RAV_ANDROID 1
    #define RAV_POSIX 1  // Mostly POSIX compliant.
#else
    #define RAV_ANDROID 0
#endif

// Linux
#if defined(__linux__)
    #define RAV_LINUX 1
    #define RAV_POSIX 1  // Most distributions are mostly POSIX compliant.
#else
    #define RAV_LINUX 0
#endif

// BSD
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    #define RAV_BSD 1
    #define RAV_POSIX 1  // Mostly POSIX compliant.
#else
    #define RAV_BSD 0
#endif

// Unix
#if defined(__unix__)
    #define RAV_UNIX 1
#else
    #define RAV_UNIX 0
#endif

// Posix
#ifndef RAV_POSIX
    #if defined(_POSIX_VERSION)
        #define RAV_POSIX 1
    #else
        #define RAV_POSIX 0
    #endif
#endif

#ifdef RAV_WINDOWS
    #ifndef NOMINMAX
        #error "Please define NOMINMAX as compile constant in your build system."
    #endif
#endif
