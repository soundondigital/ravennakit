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

TEST_CASE("rav::sdp::ReferenceClock") {
    SECTION("Test IEEE1588-2008 with domain") {
        const auto str = "ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:1";
        auto ref_clock = rav::sdp::parse_reference_clock(str);
        REQUIRE(ref_clock);
        REQUIRE(ref_clock->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(ref_clock->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
        REQUIRE(ref_clock->gmid_ == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(ref_clock->domain_ == 1);
    }

    SECTION("Test IEEE1588-2002:traceable") {
        const auto str = "ptp=IEEE1588-2002:traceable";
        auto ref_clock = rav::sdp::parse_reference_clock(str);
        REQUIRE(ref_clock);
        REQUIRE(ref_clock->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(ref_clock->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2002);
        REQUIRE(ref_clock->gmid_ == "traceable");
        REQUIRE_FALSE(ref_clock->domain_.has_value());
    }

    SECTION("Test IEEE802.1AS-2011") {
        const auto str = "ptp=IEEE802.1AS-2011:39-A7-94-FF-FE-07-CB-D0";
        auto ref_clock = rav::sdp::parse_reference_clock(str);
        REQUIRE(ref_clock);
        REQUIRE(ref_clock->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(ref_clock->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_802_1AS_2011);
        REQUIRE(ref_clock->gmid_ == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE_FALSE(ref_clock->domain_.has_value());
    }

    SECTION("Test traceable") {
        const auto str = "ptp=traceable";
        auto ref_clock = rav::sdp::parse_reference_clock(str);
        REQUIRE(ref_clock);
        REQUIRE(ref_clock->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(ref_clock->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::traceable);
        REQUIRE_FALSE(ref_clock->gmid_.has_value());
        REQUIRE_FALSE(ref_clock->domain_.has_value());
    }
}
