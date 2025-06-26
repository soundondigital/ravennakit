/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/math/sliding_stats.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>

TEST_CASE("SlidingStats Benchmark") {
    ankerl::nanobench::Bench b;
    b.title("SlidingStats Benchmark")
        .warmup(100)
        .relative(false)
        .minEpochIterations(20800)
        .performanceCounters(true);

    rav::SlidingStats stats(1024);
    double i = 0.0;
    b.run("Add", [&] {
        stats.add(i);
        i += 1.0;
    });
}
