/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_reference_clock.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("reference_clock", "[reference_clock]") {
    SECTION("Test IEEE1588-2008 with domain") {
        const auto str = "ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:1";
        auto result = rav::sdp::reference_clock::parse_new(str);
        REQUIRE(result.is_ok());
        auto ref_clock = result.move_ok();
        REQUIRE(ref_clock.source() == rav::sdp::reference_clock::clock_source::ptp);
        REQUIRE(ref_clock.ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008);
        REQUIRE(ref_clock.gmid() == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(ref_clock.domain() == 1);
    }

    SECTION("Test IEEE1588-2002:traceable") {
        const auto str = "ptp=IEEE1588-2002:traceable";
        auto result = rav::sdp::reference_clock::parse_new(str);
        REQUIRE(result.is_ok());
        auto ref_clock = result.move_ok();
        REQUIRE(ref_clock.source() == rav::sdp::reference_clock::clock_source::ptp);
        REQUIRE(ref_clock.ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2002);
        REQUIRE(ref_clock.gmid() == "traceable");
        REQUIRE_FALSE(ref_clock.domain().has_value());
    }

    SECTION("Test IEEE802.1AS-2011") {
        const auto str = "ptp=IEEE802.1AS-2011:39-A7-94-FF-FE-07-CB-D0";
        auto result = rav::sdp::reference_clock::parse_new(str);
        REQUIRE(result.is_ok());
        auto ref_clock = result.move_ok();
        REQUIRE(ref_clock.source() == rav::sdp::reference_clock::clock_source::ptp);
        REQUIRE(ref_clock.ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_802_1AS_2011);
        REQUIRE(ref_clock.gmid() == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE_FALSE(ref_clock.domain().has_value());
    }

    SECTION("Test traceable") {
        const auto str = "ptp=traceable";
        auto result = rav::sdp::reference_clock::parse_new(str);
        REQUIRE(result.is_ok());
        auto ref_clock = result.move_ok();
        REQUIRE(ref_clock.source() == rav::sdp::reference_clock::clock_source::ptp);
        REQUIRE(ref_clock.ptp_version() == rav::sdp::reference_clock::ptp_ver::traceable);
        REQUIRE_FALSE(ref_clock.gmid().has_value());
        REQUIRE_FALSE(ref_clock.domain().has_value());
    }
}
