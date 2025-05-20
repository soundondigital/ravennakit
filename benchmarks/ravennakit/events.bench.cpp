/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/events.hpp"
#include "ravennakit/core/sync/triple_buffer.hpp"

#include <catch2/catch_all.hpp>
#include <nanobench.h>
#include <thread>

TEST_CASE("Events Benchmark") {
    ankerl::nanobench::Bench b;
    b.title("Events Benchmark")
        .warmup(100)
        .relative(false)
        .minEpochIterations(1'000'000)
        .performanceCounters(true);

    rav::Events<int, double, std::string> events;

    events.on<int>([](const int& i) {
        ankerl::nanobench::doNotOptimizeAway(i);
    });

    events.on<double>([](const double& d) {
        ankerl::nanobench::doNotOptimizeAway(d);
    });

    events.on<std::string>([](const std::string& s) {
        ankerl::nanobench::doNotOptimizeAway(s);
    });

    int i_int = 0;
    double i_double = 0.0;
    b.run("Int, double and string", [&] {
        events.emit(i_int++);
        events.emit(i_double++);
        events.emit(std::to_string(i_int));
    });
}
