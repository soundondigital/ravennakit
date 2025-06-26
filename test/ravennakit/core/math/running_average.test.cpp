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
#include "ravennakit/core/math/running_average.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::RunningAverage") {
    SECTION("Initialization") {
        constexpr rav::RunningAverage avg;
        REQUIRE(rav::is_within(avg.average() , 0.0, 0.0));
        REQUIRE(avg.count() == 0);
    }

    SECTION("Do some averaging") {
        rav::RunningAverage avg;
        avg.add(1);
        avg.add(2.0);
        avg.add(3);
        REQUIRE(rav::is_within(avg.average(), 2.0, 0.0));
        REQUIRE(avg.count() == 3);
        avg.reset();
        REQUIRE(rav::is_within(avg.average(), 0.0, 0.0));
        REQUIRE(avg.count() == 0);
    }
}
