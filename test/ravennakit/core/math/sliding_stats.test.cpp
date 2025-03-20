/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("sliding_stats") {
    SECTION("average") {
        rav::sliding_stats avg(5);
        REQUIRE(avg.count() == 0);
        REQUIRE(rav::is_within(avg.average(), 0.0, 0.0));
        avg.add(1);
        avg.add(2);
        avg.add(3);
        avg.add(4);
        avg.add(5);
        REQUIRE(avg.count() == 5);
        REQUIRE(rav::is_within(avg.average(), 3.0, 0.0));
        avg.add(6);
        REQUIRE(avg.count() == 5);
        REQUIRE(rav::is_within(avg.average(), 4.0, 0.0));
        avg.add(7);
        REQUIRE(avg.count() == 5);
        REQUIRE(rav::is_within(avg.average(), 5.0, 0.0));
        avg.reset();
        REQUIRE(avg.count() == 0);
        REQUIRE(rav::is_within(avg.average(), 0.0, 0.0));
    }

    SECTION("median") {
        rav::sliding_stats stats(5);
        REQUIRE(stats.count() == 0);
        REQUIRE(rav::is_within(stats.median(), 0.0, 0.0));
        stats.add(1);
        REQUIRE(stats.count() == 1);
        REQUIRE(rav::is_within(stats.median(), 1.0, 0.0));
        stats.add(500);
        stats.add(4);
        stats.add(3);
        REQUIRE(stats.count() == 4);
        REQUIRE(rav::is_within(stats.median(), 3.5, 0.0));
        stats.add(2);
        REQUIRE(stats.count() == 5);
        REQUIRE(rav::is_within(stats.median(), 3.0, 0.0));
    }
}
