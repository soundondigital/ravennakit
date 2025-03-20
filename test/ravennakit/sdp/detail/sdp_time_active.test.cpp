/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_time_active.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("media_description | time_active_field") {
    SECTION("Test time field") {
        auto result = rav::sdp::TimeActiveField::parse_new("t=123456789 987654321");
        REQUIRE(result.is_ok());
        const auto time = result.move_ok();
        REQUIRE(time.start_time == 123456789);
        REQUIRE(time.stop_time == 987654321);
    }

    SECTION("Test invalid time field") {
        auto result = rav::sdp::TimeActiveField::parse_new("t=123456789 ");
        REQUIRE(result.is_err());
    }

    SECTION("Test invalid time field") {
        auto result = rav::sdp::TimeActiveField::parse_new("t=");
        REQUIRE(result.is_err());
    }
}
