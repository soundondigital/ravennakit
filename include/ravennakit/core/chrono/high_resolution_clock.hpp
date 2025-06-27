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

#include "ravennakit/core/platform.hpp"
#include "ravennakit/core/math/fraction.hpp"
#include "ravennakit/core/platform/apple/mach.hpp"

#include <cstdint>
#include <ctime>

#if RAV_WINDOWS
    #include <windows.h>
#endif

namespace rav {

/**
 * Provides access to the current time with the highest possible resolution.
 */
class HighResolutionClock {
  public:
    /**
     * @returns the current time in nanoseconds since an arbitrary point in time.
     * On macOS the value does not progress while the system is asleep.
     */
    static uint64_t now() {
        static const HighResolutionClock clock;
#if RAV_APPLE
        const uint64_t raw = mach_absolute_time();
        return raw * clock.timebase_.numerator / clock.timebase_.denominator;
#elif RAV_WINDOWS
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return static_cast<uint64_t>((counter.QuadPart * 1'000'000'000) / clock.frequency_.QuadPart);
#elif RAV_POSIX
        timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000 + static_cast<uint64_t>(ts.tv_nsec);
#else
    #error "high_resolution_clock is not implemented for this platform."
        return 0;
#endif
    }

  private:
#if RAV_APPLE
    Fraction<uint32_t> timebase_ {};
#elif RAV_WINDOWS
    LARGE_INTEGER frequency_ {};
#endif

    HighResolutionClock() {
#if RAV_APPLE
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        timebase_.numerator = info.numer;
        timebase_.denominator = info.denom;
#elif RAV_WINDOWS
        QueryPerformanceFrequency(&frequency_);
#endif
    }
};

}  // namespace rav
