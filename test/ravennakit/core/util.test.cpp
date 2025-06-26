/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/util.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("rav utils") {
    SECTION("Test int buffer") {
        int data[] = {1, 2, 3, 4, 5};
        REQUIRE(rav::num_elements_in_array(data) == 5);
    }

    SECTION("Test char buffer") {
        char data[] = {1, 2, 3, 4, 5};
        REQUIRE(rav::num_elements_in_array(data) == 5);
    }

    SECTION("is_within()") {
        REQUIRE(rav::is_within(1.0, 1.0, 0.0));
        REQUIRE_FALSE(rav::is_within(1.0, 1.000001, 0.0));

        REQUIRE(rav::is_within(1, 1, 0));
        REQUIRE_FALSE(rav::is_within(1, 2, 0));
    }
}
