/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/platform/apple/mach.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>

#if RAV_APPLE

TEST_CASE("Benchmarking Apple system calls") {
    ankerl::nanobench::Bench b;
    b.title("Benchmarking Apple system calls").relative(false).performanceCounters(true).minEpochIterations(100'000);

    b.run("mach_absolute_time()", [&] {
        ankerl::nanobench::doNotOptimizeAway(mach_absolute_time());
    });

    b.run("mach_absolute_time_ns()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::mach_absolute_time_ns());
    });

    b.run("mach_absolute_time_to_nanoseconds()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::mach_absolute_time_to_nanoseconds(1234));
    });

    b.run("mach_nanoseconds_to_absolute_time()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::mach_nanoseconds_to_absolute_time(1234));
    });
}
#endif
