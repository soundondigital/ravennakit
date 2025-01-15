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
            ts.add({0, 2, 0x8000});
            REQUIRE(ts.seconds() == 1);
            REQUIRE(ts.nanoseconds() == 3);
        }

        SECTION("Add 2.5ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(0.0000000025);  // 2.5ns
            REQUIRE(ts.seconds() == 1);
            REQUIRE(ts.nanoseconds() == 4); // Rounded up
        }

        SECTION("Add -2.5ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            rav::ptp_time_interval ti(0, -3, 0x8000);
            // Note how ti normalizes to -2.5ns:
            REQUIRE(ti.seconds() == -1);
            REQUIRE(ti.nanos() == 999'999'997);
            ts.add(ti);
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 999999998);
        }

        SECTION("Add -2ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            rav::ptp_time_interval ti(0, -2, 0x0000);
            ts.add(ti);
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 999999999);
        }

        SECTION("Add -2ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(-0.000000002);  // 2ns
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 999999999);
        }

        SECTION("Add -2.5ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(-0.0000000025);
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 999999998);
        }

        SECTION("Add 2.5s") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add({2, 500'000'001, 0});
            REQUIRE(ts.seconds() == 3);
            REQUIRE(ts.nanoseconds() == 500'000'002);
        }

        SECTION("Add 2.5s double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(2.500000001);
            REQUIRE(ts.seconds() == 3);
            REQUIRE(ts.nanoseconds() == 500'000'002);
        }

        SECTION("Add -2.49s") {
            rav::ptp_timestamp ts(3'000'000'000);
            ts.add({-3, 510'000'000, 0});
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 510'000'000);
        }

        SECTION("Add -2.49s double") {
            rav::ptp_timestamp ts(3'000'000'000);
            ts.add_seconds(-2.490'000'000); // Equals ptp_time_interval{-3, 510'000'000, 0}
            REQUIRE(ts.seconds() == 0);
            REQUIRE(ts.nanoseconds() == 510'000'000);
        }
    }

    SECTION("To time interval") {
        rav::ptp_timestamp ts(3'000'000'001);
        auto ti = ts.to_time_interval();
        REQUIRE(ti.seconds() == 3);
        REQUIRE(ti.nanos() == 1);
        REQUIRE(ti.total_nanos() == 3'000'000'001);
    }
}
