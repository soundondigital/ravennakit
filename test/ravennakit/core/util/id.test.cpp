/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/id.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("id", "[id]") {
    SECTION("Invalid id") {
        constexpr rav::id invalid_id;
        REQUIRE_FALSE(invalid_id.is_valid());
    }

    SECTION("Invalid id 2") {
        rav::id invalid_id(0);
        REQUIRE_FALSE(invalid_id.is_valid());
    }

    SECTION("Generator") {
        rav::id::generator gen;
        REQUIRE(gen.next() == 1);
        REQUIRE(gen.next() == 2);
        REQUIRE(gen.next() == 3);
    }

    SECTION("Process-wide id") {
        REQUIRE(rav::id::next_process_wide_unique_id() == 1);
        REQUIRE(rav::id::next_process_wide_unique_id() == 2);
        REQUIRE(rav::id::next_process_wide_unique_id() == 3);
    }
}
