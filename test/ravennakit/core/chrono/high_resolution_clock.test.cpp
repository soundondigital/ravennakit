/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/chrono/high_resolution_clock.hpp"

#include <catch2/catch_all.hpp>

#include <thread>

TEST_CASE("rav::HighResolutionClock") {
    SECTION("now") {
        const auto now = rav::HighResolutionClock::now();
        REQUIRE(now > 0);
    }
    SECTION("Progression") {
        for (int i = 0; i < 100; ++i) {
            const auto now = rav::HighResolutionClock::now();
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            REQUIRE(rav::HighResolutionClock::now() >= now + 100);
        }
    }
}
