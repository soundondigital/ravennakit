/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/types/ptp_time_interval.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_time_interval") {
    SECTION("Default constructor initializes to zero") {
        constexpr rav::ptp_time_interval interval;
        REQUIRE(interval.seconds() == 0);
        REQUIRE(interval.nanos() == 0);
        REQUIRE(interval.fraction() == 0);
    }

    SECTION("Constructor initializes with correct values") {
        SECTION("Positive values") {
            rav::ptp_time_interval interval(5, 5'000'000, 0x3fff);  // 5.50000005s
            REQUIRE(interval.seconds() == 5);
            REQUIRE(interval.nanos() == 5'000'000);
            REQUIRE(interval.fraction() == 0x3fff);
        }

        SECTION("Negative values") {
            rav::ptp_time_interval negative_interval(-5, 5'000'000, 0x3fff);
            REQUIRE(negative_interval.seconds() == -5);
            REQUIRE(negative_interval.nanos() == 5'000'000);
            REQUIRE(negative_interval.fraction() == 0x3fff);
        }

        SECTION("Normalize nanos") {
            rav::ptp_time_interval normalize(5, 2'000'000'000, 0x3fff);
            REQUIRE(normalize.seconds() == 7);
            REQUIRE(normalize.nanos() == 0);
            REQUIRE(normalize.fraction() == 0x3fff);
        }

        SECTION("Normalize nanos 2") {
            rav::ptp_time_interval normalize(5, 2'100'000'000, 0x3fff);
            REQUIRE(normalize.seconds() == 7);
            REQUIRE(normalize.nanos() == 100'000'000);
            REQUIRE(normalize.fraction() == 0x3fff);
        }

        SECTION("Normalize nanos 3") {
            rav::ptp_time_interval normalize(5, -1'000'000'000, 0);
            REQUIRE(normalize.seconds() == 4);
            REQUIRE(normalize.nanos() == 0);
            REQUIRE(normalize.fraction() == 0);
        }

        SECTION("Normalize nanos 4") {
            rav::ptp_time_interval normalize(5, -1'000'000'000, 0x3fff);
            REQUIRE(normalize.seconds() == 4);
            REQUIRE(normalize.nanos() == 0);
            REQUIRE(normalize.fraction() == 0x3fff);
        }

        SECTION("Normalize nanos 5") {
            rav::ptp_time_interval normalize(5, -1'100'000'000, 0x3fff);
            REQUIRE(normalize.seconds() == 3);
            REQUIRE(normalize.nanos() == 900'000'000);
            REQUIRE(normalize.fraction() == 0x3fff);
        }

        SECTION("Normalize nanos 6") {
            rav::ptp_time_interval normalize(5, -900'000'000, 0x3fff);
            REQUIRE(normalize.seconds() == 4);
            REQUIRE(normalize.nanos() == 100'000'000);
            REQUIRE(normalize.fraction() == 0x3fff);
        }
    }

    SECTION("Arithmetic addition works correctly") {
        SECTION("Check arithmetic") {
            rav::ptp_time_interval interval1(3, 50000, 0x2000);
            rav::ptp_time_interval interval2(4, 70000, 0x1000);

            rav::ptp_time_interval result = interval1 + interval2;
            REQUIRE(result.seconds() == 7);
            REQUIRE(result.nanos() == 120000);
            REQUIRE(result.fraction() == 0x3000);
        }

        SECTION("Check normalization of nanos during addition") {
            rav::ptp_time_interval interval3(3, 500'000'000, 0x2000);
            rav::ptp_time_interval interval4(1, 500'000'000, 0x1000);

            rav::ptp_time_interval result2 = interval3 + interval4;
            REQUIRE(result2.seconds() == 5);
            REQUIRE(result2.nanos() == 0);
            REQUIRE(result2.fraction() == 0x3000);
        }

        SECTION("Check normalization of fractal during addition") {
            // Check normalization of fractal during addition
            rav::ptp_time_interval interval1(3, 0, 0xffff);
            rav::ptp_time_interval interval2(1, 0, 1);

            rav::ptp_time_interval result = interval1 + interval2;
            REQUIRE(result.seconds() == 4);
            REQUIRE(result.nanos() == 1);
            REQUIRE(result.fraction() == 0);
        }
    }

    SECTION("Arithmetic subtraction works correctly") {
        SECTION("Check arithmetic") {
            rav::ptp_time_interval interval1(3, 50000, 0x2000);
            rav::ptp_time_interval interval2(4, 70000, 0x1000);

            rav::ptp_time_interval result = interval1 - interval2;
            REQUIRE(result.seconds() == -2);
            REQUIRE(result.nanos() == 0x3b9a7be0);
            REQUIRE(result.fraction() == 0x1000);
        }

        SECTION("Check normalization of nanos during subtraction") {
            rav::ptp_time_interval interval3(3, 500'000'000, 0x2000);
            rav::ptp_time_interval interval4(1, 600'000'000, 0x1000);

            rav::ptp_time_interval result2 = interval3 - interval4;
            REQUIRE(result2.seconds() == 1);
            REQUIRE(result2.nanos() == 900'000'000);
            REQUIRE(result2.fraction() == 0x1000);
        }

        SECTION("Check normalization of fraction during subtraction") {
            rav::ptp_time_interval interval1(0, 0, 0);
            rav::ptp_time_interval interval2(0, 0, 1);

            rav::ptp_time_interval result = interval1 - interval2;
            REQUIRE(result.seconds() == -1);
            REQUIRE(result.nanos() == 0x3b9ac9ff);
            REQUIRE(result.fraction() == 0xffff);
        }
    }

    SECTION("From wire positive") {
        const auto interval = rav::ptp_time_interval::from_wire_format(0x24000);
        REQUIRE(interval.seconds() == 0);
        REQUIRE(interval.nanos() == 0x2);
        REQUIRE(interval.fraction() == 0x4000);
    }

    SECTION("From wire negative") {
        const auto interval = rav::ptp_time_interval::from_wire_format(-0x24000);
        REQUIRE(interval.seconds() == -1);
        REQUIRE(interval.nanos() == 0x3b9ac9fd);
        REQUIRE(interval.fraction() == 0xc000);  // 0x10000 - 0x4000
    }

    SECTION("To wire positive") {
        const auto interval = rav::ptp_time_interval(0, 2, 0x4000);
        REQUIRE(interval.to_wire_format() == 0x24000);

        // Clamp overflow to max value
        REQUIRE(
            rav::ptp_time_interval(std::numeric_limits<int64_t>::max(), 0, 0).to_wire_format()
            == std::numeric_limits<int64_t>::max()
        );
    }

    SECTION("To wire negative") {
        const auto interval = rav::ptp_time_interval(0, -3, 0xc000);
        REQUIRE(interval.to_wire_format() == -0x24000);

        // Clamp underflow to min value
        REQUIRE(
            rav::ptp_time_interval(std::numeric_limits<int64_t>::min(), 0, 0).to_wire_format()
            == std::numeric_limits<int64_t>::min()
        );
    }

    SECTION("From and to wire roundtrip") {
        const auto interval = rav::ptp_time_interval::from_wire_format(0x28000);
        REQUIRE(interval.to_wire_format() == 0x28000);
    }

    SECTION("From and to wire roundtrip negative") {
        const auto interval = rav::ptp_time_interval::from_wire_format(-0x28000);
        REQUIRE(interval.to_wire_format() == -0x28000);
    }

    SECTION("Equality and inequality operators") {
        rav::ptp_time_interval interval1(5, 10000, 1);
        rav::ptp_time_interval interval2(5, 10000, 1);
        rav::ptp_time_interval interval3(6, 20000, 2);

        REQUIRE(interval1 == interval2);
        REQUIRE(interval1 != interval3);
        REQUIRE(interval2 != interval3);
    }

    SECTION("Chained arithmetic operations") {
        rav::ptp_time_interval interval1(1, 1, 1);
        rav::ptp_time_interval interval2(2, 2, 2);
        rav::ptp_time_interval interval3(3, 3, 3);

        rav::ptp_time_interval result = interval1 + interval2 - interval3;
        REQUIRE(result.seconds() == 0);
        REQUIRE(result.nanos() == 0);
        REQUIRE(result.fraction() == 0);
    }

    SECTION("Self-assignment operators") {
        rav::ptp_time_interval interval1(1, 10000, 1);
        rav::ptp_time_interval interval2(2, 20000, 2);

        interval1 += interval2;
        REQUIRE(interval1.seconds() == 3);
        REQUIRE(interval1.nanos() == 30000);
        REQUIRE(interval1.fraction() == 3);

        interval1 -= interval2;
        REQUIRE(interval1.seconds() == 1);
        REQUIRE(interval1.nanos() == 10000);
        REQUIRE(interval1.fraction() == 1);
    }

    SECTION("Nanos rounded") {
        rav::ptp_time_interval interval1(0, 1, 0x8000);
        REQUIRE(interval1.nanos_rounded() == 2);

        rav::ptp_time_interval interval2(0, 1, 0x7fff);
        REQUIRE(interval2.nanos_rounded() == 1);
    }

    SECTION("Divide 1") {
        rav::ptp_time_interval interval1(5, 10000, 2);
        auto nanos = interval1.total_nanos();
        interval1 /= 2;
        REQUIRE(nanos / 2 == interval1.total_nanos());
        REQUIRE(interval1.seconds() == 2);
        REQUIRE(interval1.nanos() == 500'005'000);
        REQUIRE(interval1.fraction() == 1);
    }

    SECTION("Divide 2") {
        auto r = rav::ptp_time_interval(5, 10000, 2) / 2;
        REQUIRE(r.seconds() == 2);
        REQUIRE(r.nanos() == 500'005'000);
        REQUIRE(r.fraction() == 1);
    }

    SECTION("Multiply") {
        rav::ptp_time_interval interval1(5, 10000, 2);
        interval1 *= 2;
        REQUIRE(interval1.seconds() == 10);
        REQUIRE(interval1.nanos() == 20000);
        REQUIRE(interval1.fraction() == 4);
    }

    SECTION("Multiply") {
        rav::ptp_time_interval interval1(5, 600'000'000, 2);
        interval1 *= 2;
        REQUIRE(interval1.seconds() == 11);
        REQUIRE(interval1.nanos() == 200'000'000);
        REQUIRE(interval1.fraction() == 4);
    }

    SECTION("Multiply") {
        rav::ptp_time_interval interval1(5, 600'000'000, 1);
        auto nanos = interval1.total_nanos();
        interval1 *= -1;
        REQUIRE(nanos * -1 == interval1.total_nanos());
        REQUIRE(interval1.seconds() == -6);
        REQUIRE(interval1.nanos() == 400'000'000);
        REQUIRE(interval1.fraction() == 1);
    }

    SECTION("Multiply") {
        rav::ptp_time_interval interval1(5, 600'000'000, 1);
        auto nanos = interval1.total_nanos();
        interval1 *= -2;
        REQUIRE(nanos * -2 == interval1.total_nanos());
        REQUIRE(interval1.seconds() == -12);
        REQUIRE(interval1.nanos() == 800'000'000);
        REQUIRE(interval1.fraction() == 2);
    }

    SECTION("Multiply 2") {
        auto interval = rav::ptp_time_interval(5, 10000, 2) * 2;
        REQUIRE(interval.seconds() == 10);
        REQUIRE(interval.nanos() == 20000);
        REQUIRE(interval.fraction() == 4);
    }

    SECTION("Multiply 2") {
        auto interval = rav::ptp_time_interval(5, 600'000'000, 2) * 2;
        REQUIRE(interval.seconds() == 11);
        REQUIRE(interval.nanos() == 200'000'000);
        REQUIRE(interval.fraction() == 4);
    }

    SECTION("Multiply 2") {
        auto interval = rav::ptp_time_interval(5, 600'000'000, 1) * -1;
        REQUIRE(interval.seconds() == -6);
        REQUIRE(interval.nanos() == 400'000'000);
        REQUIRE(interval.fraction() == 1);
    }

    SECTION("Multiply 2") {
        auto interval = rav::ptp_time_interval(5, 600'000'000, 1) * -2;
        REQUIRE(interval.seconds() == -12);
        REQUIRE(interval.nanos() == 800'000'000);
        REQUIRE(interval.fraction() == 2);
    }
}
