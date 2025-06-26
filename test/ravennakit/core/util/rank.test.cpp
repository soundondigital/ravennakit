/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/rank.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::Rank") {
    rav::Rank rank(0);
    REQUIRE(rank.value() == 0);
    REQUIRE(rank++ == 0);
    REQUIRE(rank++ == 1);
    REQUIRE(++rank == 3);
    REQUIRE(++rank == 4);

    rank = rav::Rank(5);
    REQUIRE(rank == 5);

    rank = rav::Rank(10);
    REQUIRE(rank == 10);

    rav::Rank rank2;
    rank2 = rank++;
    REQUIRE(rank2 == 10);
    REQUIRE(10 == rank2);
}
