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

TEST_CASE("rav::Id", "[id]") {
    SECTION("Invalid id") {
        constexpr rav::Id invalid_id;
        REQUIRE_FALSE(invalid_id.is_valid());
    }

    SECTION("Invalid id 2") {
        rav::Id invalid_id(0);
        REQUIRE_FALSE(invalid_id.is_valid());
    }

    SECTION("Generator") {
        rav::Id::Generator gen;
        REQUIRE(gen.next() == 1);
        REQUIRE(gen.next() == 2);
        REQUIRE(gen.next() == 3);
    }

    SECTION("Process-wide id") {
        auto id = rav::Id::get_next_process_wide_unique_id();
        REQUIRE(rav::Id::get_next_process_wide_unique_id() == id.value() + 1);
        REQUIRE(rav::Id::get_next_process_wide_unique_id() == id.value() + 2);
        REQUIRE(rav::Id::get_next_process_wide_unique_id() == id.value() + 3);
    }
}
