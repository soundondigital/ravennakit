/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/platform.hpp"

#if RAV_WINDOWS

    #include <windows.h>

namespace rav {

inline LARGE_INTEGER query_performance_counter_frequency() {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        return {};
    }
    return frequency;
}

inline LARGE_INTEGER query_performance_counter()
{
    static const auto frequency = query_performance_counter_frequency();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter;
}

inline uint64_t query_performance_counter_ns()
{
    static const auto frequency = query_performance_counter_frequency();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>((counter.QuadPart * 1'000'000'000) / frequency.QuadPart);
}

}

#endif
