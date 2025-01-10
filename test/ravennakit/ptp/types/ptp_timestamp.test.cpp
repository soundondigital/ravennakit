/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/types/ptp_timestamp.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_timestamp") {
    SECTION("Construct from nanos") {
        rav::ptp_timestamp ts(1'000'000'000);
        REQUIRE(ts.seconds() == 1);
        REQUIRE(ts.nanoseconds() == 0);
    }

    SECTION("Construct from nanos 2") {
        rav::ptp_timestamp ts(1'000'000'001);
        REQUIRE(ts.seconds() == 1);
        REQUIRE(ts.nanoseconds() == 1);
    }

    SECTION("Construct from max nanos value") {
        rav::ptp_timestamp ts(std::numeric_limits<uint64_t>::max());
        REQUIRE(ts.seconds() == 18446744073);
        REQUIRE(ts.nanoseconds() == 709551615);
    }

    SECTION("Add") {
        rav::ptp_timestamp ts1(1'000'000'001);
        rav::ptp_timestamp ts2(1'000'000'002);
        auto result = ts1 + ts2;
        REQUIRE(result.seconds() == 2);
        REQUIRE(result.nanos() == 3);
    }

    SECTION("Add overflow") {
        rav::ptp_timestamp ts1(1'500'000'000);
        rav::ptp_timestamp ts2(1'500'000'001);
        auto result = ts1 + ts2;
        REQUIRE(result.seconds() == 3);
        REQUIRE(result.nanos() == 1);
    }

    SECTION("Subtract") {
        rav::ptp_timestamp ts1(2'000'000'002);
        rav::ptp_timestamp ts2(1'000'000'001);
        auto result = ts1 - ts2;
        REQUIRE(result.seconds() == 1);
        REQUIRE(result.nanos() == 1);
    }

    SECTION("Subtract underflow") {
        rav::ptp_timestamp ts1(2'500'000'001);
        rav::ptp_timestamp ts2(1'500'000'002);
        auto result = ts1 - ts2;
        REQUIRE(result.seconds() == 0);
        REQUIRE(result.nanos() == 999999999);
    }

    SECTION("Less than") {
        rav::ptp_timestamp ts1(1'000'000'001);
        rav::ptp_timestamp ts2(1'000'000'002);
        REQUIRE(ts1 < ts2);
        REQUIRE_FALSE(ts2 < ts1);
    }

    SECTION("Less than or equal") {
        rav::ptp_timestamp ts1(1'000'000'001);
        rav::ptp_timestamp ts2(1'000'000'002);
        REQUIRE(ts1 <= ts2);
        REQUIRE_FALSE(ts2 <= ts1);
        REQUIRE(ts1 <= ts1);
    }

    SECTION("Greater than") {
        rav::ptp_timestamp ts1(1'000'000'002);
        rav::ptp_timestamp ts2(1'000'000'001);
        REQUIRE(ts1 > ts2);
        REQUIRE_FALSE(ts2 > ts1);
    }

    SECTION("Greater than or equal") {
        rav::ptp_timestamp ts1(1'000'000'002);
        rav::ptp_timestamp ts2(1'000'000'001);
        REQUIRE(ts1 >= ts2);
        REQUIRE_FALSE(ts2 >= ts1);
        REQUIRE(ts1 >= ts1);
    }

    SECTION("Add time interval") {
        SECTION("Add 2.5ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add({0, 2, 0x8000});  // 2.5ns
            REQUIRE(ts.seconds() == 1);
            REQUIRE(ts.nanoseconds() == 3);  // 4 because of rounding
        }

        SECTION("Add -2.5ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add({0, -3, 0x8000});  // -2.5ns
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 0);
        }

        SECTION("Add 2.5s") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add({2, 500'000'001, 0});  // 2.5s + 1
            REQUIRE(ts.seconds() == 3);
            REQUIRE(ts.nanoseconds() == 500'000'002);
        }

        SECTION("Add -2.5s") {
            rav::ptp_timestamp ts(3'000'000'001);
            ts.add({-2, 500'000'001, 0});  // 2.5s + 1
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 500'000'000);
        }
    }

    SECTION("To time interval") {
        rav::ptp_timestamp ts(3'000'000'001);
        auto ti = ts.to_time_interval();
        REQUIRE(ti.seconds() == 3);
        REQUIRE(ti.nanos() == 1);
        REQUIRE(ti.nanos_total() == 3'000'000'001);
    }
}
