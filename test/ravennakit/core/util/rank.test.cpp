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

TEST_CASE("Rank") {
    rav::Rank rank(0);
    CHECK(rank.value() == 0);

    rank++;
    CHECK(rank.value() == 1);

    rank++;
    CHECK(rank.value() == 2);

    rank = rav::Rank(5);
    CHECK(rank.value() == 5);

    rank = rav::Rank(10);
    CHECK(rank.value() == 10);

    rav::Rank rank2;
    rank2 = rank++;
    CHECK(rank2.value() == 10);
}
