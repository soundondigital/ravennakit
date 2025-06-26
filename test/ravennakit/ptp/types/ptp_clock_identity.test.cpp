/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/types/ptp_clock_identity.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ptp::ClockIdentity") {
    SECTION("Construct from MAC address") {
        const rav::MacAddress mac_address("a1:b2:c3:d4:e5:f6");
        const rav::ptp::ClockIdentity clock_identity = rav::ptp::ClockIdentity::from_mac_address(mac_address);

        REQUIRE(clock_identity.data[0] == 0xa1);
        REQUIRE(clock_identity.data[1] == 0xb2);
        REQUIRE(clock_identity.data[2] == 0xc3);
        REQUIRE(clock_identity.data[3] == 0xff);
        REQUIRE(clock_identity.data[4] == 0xfe);
        REQUIRE(clock_identity.data[5] == 0xd4);
        REQUIRE(clock_identity.data[6] == 0xe5);
        REQUIRE(clock_identity.data[7] == 0xf6);
    }

    SECTION("Default constructor") {
        constexpr rav::ptp::ClockIdentity clock_identity;
        REQUIRE(clock_identity.empty());
    }

    SECTION("Empty") {
        rav::ptp::ClockIdentity clock_identity;
        REQUIRE(clock_identity.empty());

        for (unsigned char& i : clock_identity.data) {
            SECTION("Test every byte") {
                i = 1;
                REQUIRE_FALSE(clock_identity.empty());
            }
        }
    }

    SECTION("Comparison") {
        rav::ptp::ClockIdentity a;
        rav::ptp::ClockIdentity b;

        SECTION("Equal") {
            REQUIRE(a == b);
            REQUIRE_FALSE(a < b);
            REQUIRE_FALSE(a > b);
        }

        SECTION("a < b") {
            b.data[0] = 1;
            REQUIRE(a < b);
            REQUIRE(a != b);
        }

        SECTION("a > b") {
            a.data[0] = 1;
            REQUIRE(a > b);
            REQUIRE(a != b);
        }

        SECTION("a < b") {
            b.data[7] = 1;
            REQUIRE(a < b);
            REQUIRE(a != b);
        }

        SECTION("a > b") {
            a.data[7] = 1;
            REQUIRE(a > b);
            REQUIRE(a != b);
        }
    }
}
