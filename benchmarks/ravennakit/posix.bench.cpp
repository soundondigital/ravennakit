/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/platform/posix/clock.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>

#if RAV_POSIX

TEST_CASE("Benchmarking posix system calls") {
    ankerl::nanobench::Bench b;
    b.title("Benchmarking posix system calls").relative(false).performanceCounters(true).minEpochIterations(100'000);

    b.run("clock_get_time_ns()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::clock_get_time_ns());
    });
}

#endif
