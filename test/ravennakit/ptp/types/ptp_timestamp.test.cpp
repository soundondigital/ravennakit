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
        REQUIRE(ts.raw_seconds() == 1);
        REQUIRE(ts.raw_nanoseconds() == 0);
    }

    SECTION("Construct from nanos 2") {
        rav::ptp_timestamp ts(1'000'000'001);
        REQUIRE(ts.raw_seconds() == 1);
        REQUIRE(ts.raw_nanoseconds() == 1);
    }

    SECTION("Construct from max nanos value") {
        rav::ptp_timestamp ts(std::numeric_limits<uint64_t>::max());
        REQUIRE(ts.raw_seconds() == 18446744073);
        REQUIRE(ts.raw_nanoseconds() == 709551615);
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
            REQUIRE(ts.raw_seconds() == 1);
            REQUIRE(ts.raw_nanoseconds() == 3);
        }

        SECTION("Add 2.5ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(0.0000000025);  // 2.5ns
            REQUIRE(ts.raw_seconds() == 1);
            REQUIRE(ts.raw_nanoseconds() == 4);  // Rounded up
        }

        SECTION("Add -2.5ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            rav::ptp_time_interval ti(0, -3, 0x8000);
            // Note how ti normalizes to -2.5ns:
            REQUIRE(ti.seconds() == -1);
            REQUIRE(ti.nanos() == 999'999'997);
            ts.add(ti);
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 999999998);
        }

        SECTION("Add -2ns") {
            rav::ptp_timestamp ts(1'000'000'001);
            rav::ptp_time_interval ti(0, -2, 0x0000);
            ts.add(ti);
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 999999999);
        }

        SECTION("Add -2ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(-0.000000002);  // 2ns
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 999999999);
        }

        SECTION("Add -2.5ns double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(-0.0000000025);
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 999999998);
        }

        SECTION("Add 2.5s") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add({2, 500'000'001, 0});
            REQUIRE(ts.raw_seconds() == 3);
            REQUIRE(ts.raw_nanoseconds() == 500'000'002);
        }

        SECTION("Add 2.5s double") {
            rav::ptp_timestamp ts(1'000'000'001);
            ts.add_seconds(2.500000001);
            REQUIRE(ts.raw_seconds() == 3);
            REQUIRE(ts.raw_nanoseconds() == 500'000'002);
        }

        SECTION("Add -2.49s") {
            rav::ptp_timestamp ts(3'000'000'000);
            ts.add({-3, 510'000'000, 0});
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 510'000'000);
        }

        SECTION("Add -2.49s double") {
            rav::ptp_timestamp ts(3'000'000'000);
            ts.add_seconds(-2.490'000'000);  // Equals ptp_time_interval{-3, 510'000'000, 0}
            REQUIRE(ts.raw_seconds() == 0);
            REQUIRE(ts.raw_nanoseconds() == 510'000'000);
        }
    }

    SECTION("To time interval") {
        rav::ptp_timestamp ts(3'000'000'001);
        auto ti = ts.to_time_interval();
        REQUIRE(ti.seconds() == 3);
        REQUIRE(ti.nanos() == 1);
        REQUIRE(ti.total_nanos() == 3'000'000'001);
    }

    SECTION("Convert to samples") {
        SECTION("0.5 seconds") {
            rav::ptp_timestamp ts(500'000'000);
            REQUIRE(ts.to_samples(44'100) == 22'050);
            REQUIRE(ts.to_samples(48'000) == 24'000);
            REQUIRE(ts.to_samples(96'000) == 48'000);
        }

        SECTION("1 second") {
            rav::ptp_timestamp ts(1'000'000'000);
            REQUIRE(ts.to_samples(44'100) == 44'100);
            REQUIRE(ts.to_samples(48'000) == 48'000);
            REQUIRE(ts.to_samples(96'000) == 96'000);
        }

        SECTION("2 seconds") {
            rav::ptp_timestamp ts(2'000'000'000);
            REQUIRE(ts.to_samples(44'100) == 88'200);
            REQUIRE(ts.to_samples(48'000) == 96'000);
            REQUIRE(ts.to_samples(96'000) == 192'000);
        }

        SECTION("1 second") {
            rav::ptp_timestamp ts(1'000'000'000);
            REQUIRE(ts.to_samples(44'100) == 44'100);
            REQUIRE(ts.to_samples(48'000) == 48'000);
            REQUIRE(ts.to_samples(96'000) == 96'000);
        }

        SECTION("2.5 seconds") {
            rav::ptp_timestamp ts(2'500'000'000);
            REQUIRE(ts.to_samples(44'100) == 110'250);
            REQUIRE(ts.to_samples(48'000) == 120'000);
            REQUIRE(ts.to_samples(96'000) == 240'000);
        }

        SECTION("330 years 2300 absolute") {
            auto ts = rav::ptp_timestamp::from_years(330);
            auto r44 = ts.to_samples(44'100);
            auto r48 = ts.to_samples(48'000);
            auto r96 = ts.to_samples(96'000);
            auto r384 = ts.to_samples(384'000);
            REQUIRE(r44 == 458'943'408'000'000);
            REQUIRE(r48 == 499'530'240'000'000);
            REQUIRE(r96 == 999'060'480'000'000);
            REQUIRE(r384 == 3'996'241'920'000'000);
            REQUIRE(r48 * 2 == r96);
            REQUIRE(r48 * 8 == r384);
        }

        SECTION("Take lower 32 bits") {
            auto ts = rav::ptp_timestamp::from_seconds(0x1122334455667788);
            auto samples = ts.to_samples(1);
            REQUIRE(samples == 0x1122334455667788);
            // Casting to uint32_t gives us the 4 least significant bits.
            REQUIRE(static_cast<uint32_t>(samples) == 0x55667788);
            REQUIRE((samples & 0xffffffff) == 0x55667788);
        }
    }
}
