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
#include "ravennakit/core/math/sliding_window_average.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("sliding_window_average") {
    rav::sliding_window_average avg(5);
    REQUIRE(avg.count() == 0);
    REQUIRE(avg.average() == 0.0);
    avg.add(1);
    avg.add(2);
    avg.add(3);
    avg.add(4);
    avg.add(5);
    REQUIRE(avg.count() == 5);
    REQUIRE(rav::util::is_within(avg.average(), 3.0, 0.0));
    avg.add(6);
    REQUIRE(avg.count() == 5);
    REQUIRE(rav::util::is_within(avg.average(), 4.0, 0.0));
    avg.add(7);
    REQUIRE(avg.count() == 5);
    REQUIRE(rav::util::is_within(avg.average(), 5.0, 0.0));
    avg.reset();
    REQUIRE(avg.count() == 0);
    REQUIRE(avg.average() == 0.0);
}
