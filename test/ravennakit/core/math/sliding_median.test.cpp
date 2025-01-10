/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/math/sliding_median.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("sliding_median") {
    rav::sliding_median median(5);
    REQUIRE(median.count() == 0);
    REQUIRE(median.median() == 0.0);
    median.add(1);
    REQUIRE(median.count() == 1);
    REQUIRE(median.median() == 1.0);
    median.add(500);
    median.add(4);
    median.add(3);
    REQUIRE(median.count() == 4);
    REQUIRE(median.median() == 3.5);
    median.add(2);
    REQUIRE(median.count() == 5);
    REQUIRE(median.median() == 3.0);
}
