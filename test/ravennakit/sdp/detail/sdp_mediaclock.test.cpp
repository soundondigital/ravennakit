/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>

#include "ravennakit/sdp/detail/sdp_media_clock_source.hpp"

TEST_CASE("rav::sdp::MediaClockSource", "[media_clock]") {
    SECTION("Parse direct only") {
        constexpr auto line = "direct";
        const auto clock = rav::sdp::parse_media_clock_source(line);
        REQUIRE(clock);
        REQUIRE(clock->mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE_FALSE(clock->offset.has_value());
        REQUIRE_FALSE(clock->rate.has_value());
    }

    SECTION("Parse direct with offset") {
        constexpr auto line = "direct=555";
        const auto clock = rav::sdp::parse_media_clock_source(line);
        REQUIRE(clock);
        REQUIRE(clock->mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE(clock->offset.value() == 555);
        REQUIRE_FALSE(clock->rate.has_value());
    }

    SECTION("Parse direct with offset and rate") {
        constexpr auto line = "direct=555 rate=48000/1";
        const auto clock = rav::sdp::parse_media_clock_source(line);
        REQUIRE(clock);
        REQUIRE(clock->mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE(clock->offset.value() == 555);
        REQUIRE(clock->rate.value().numerator == 48000);
        REQUIRE(clock->rate.value().denominator == 1);
    }

    SECTION("Parse direct without offset and rate") {
        constexpr auto line = "direct rate=48000/1";
        const auto clock = rav::sdp::parse_media_clock_source(line);
        REQUIRE(clock);
        REQUIRE(clock->mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE_FALSE(clock->offset.has_value());
        REQUIRE(clock->rate.value().numerator == 48000);
        REQUIRE(clock->rate.value().denominator == 1);
    }
}
