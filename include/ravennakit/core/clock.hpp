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

#include "ravennakit/core/platform/apple/mach.hpp"
#include "ravennakit/core/platform/posix/clock.hpp"
#include "ravennakit/core/platform/windows/query_performance_counter.hpp"

namespace rav::clock {

inline uint64_t now_monotonic_high_resolution_ns() {
#if RAV_APPLE
    return mach_absolute_time_ns();
#elif RAV_WINDOWS
    return query_performance_counter_ns();
#elif RAV_POSIX
    return clock_get_time_ns();
#endif
}

}  // namespace rav::clock
