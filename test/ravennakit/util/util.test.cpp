/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/util/util.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("util::num_elements_in_array()", "[util]") {
    SECTION("Test int buffer") {
        int data[] = {1, 2, 3, 4, 5};
        REQUIRE(rav::util::num_elements_in_array(data) == 5);
    }

    SECTION("Test char buffer") {
        char data[] = {1, 2, 3, 4, 5};
        REQUIRE(rav::util::num_elements_in_array(data) == 5);
    }
}
