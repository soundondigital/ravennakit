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

#include "ravennakit/core/platform/windows/query_performance_counter.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>

#if RAV_WINDOWS

TEST_CASE("Benchmarking Windows system calls") {
    ankerl::nanobench::Bench b;
    b.title("Benchmarking Windows system calls").relative(false).performanceCounters(true).minEpochIterations(1085435);

    b.run("query_performance_counter_frequency()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::query_performance_counter_frequency());
    });

    b.run("query_performance_counter()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::query_performance_counter());
    });

    b.run("query_performance_counter_ns()", [&] {
        ankerl::nanobench::doNotOptimizeAway(rav::query_performance_counter_ns());
    });
}

#endif