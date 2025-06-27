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

#if RAV_APPLE

    #include <mach/mach_time.h>

namespace rav {

inline mach_timebase_info_data_t get_mach_timebase_info() {
    mach_timebase_info_data_t info {};
    if (mach_timebase_info(&info) != KERN_SUCCESS) {
        return {1, 1};  // On failure return ratio 1
    }
    return info;
}

inline uint64_t mach_absolute_time_to_nanoseconds(const uint64_t absolute_time) {
    static const auto info = get_mach_timebase_info();
    return absolute_time * info.numer / info.denom;
}

inline uint64_t mach_nanoseconds_to_absolute_time(const uint64_t nanoseconds) {
    static const auto info = get_mach_timebase_info();
    return nanoseconds * info.denom / info.numer;
}

inline uint64_t mach_absolute_time_ns() {
    return mach_absolute_time_to_nanoseconds(mach_absolute_time());
}

}  // namespace rav

#endif
