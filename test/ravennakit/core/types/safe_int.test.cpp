/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/math/safe_int.hpp"

#include <catch2/catch_all.hpp>

namespace {

template<class T, class U>
tl::expected<T, rav::safe_int_error> test_add(T a, U b) {
    rav::safe_int<T> safe_a {a};
    auto r = safe_a + b;
    REQUIRE(safe_a.value() == a);  // Test that the original is unchanged
    return r.expected();
}

template<class T, class U>
tl::expected<T, rav::safe_int_error> test_sub(T a, U b) {
    rav::safe_int<T> safe_a {a};
    auto r = safe_a - b;
    REQUIRE(safe_a.value() == a);  // Test that the original is unchanged
    return r.expected();
}

template<class T, class U>
tl::expected<T, rav::safe_int_error> test_mul(T a, U b) {
    rav::safe_int<T> safe_a {a};
    auto r = safe_a * b;
    REQUIRE(safe_a.value() == a);  // Test that the original is unchanged
    return r.expected();
}

template<class T, class U>
tl::expected<T, rav::safe_int_error> test_div(T a, U b) {
    rav::safe_int<T> safe_a {a};
    auto r = safe_a / b;
    REQUIRE(safe_a.value() == a);  // Test that the original is unchanged
    return r.expected();
}

}  // namespace

TEST_CASE("safe_int") {
    SECTION("add") {
        SECTION("Addition without overflow or underflow") {
            REQUIRE(test_add<int8_t, int8_t>(10, 20) == 30);
            REQUIRE(test_add<int16_t, int16_t>(1000, 2000) == 3000);
            REQUIRE(test_add<int32_t, int32_t>(100000, 200000) == 300000);
            REQUIRE(test_add<uint8_t, uint8_t>(100, 50) == 150);
        }

        SECTION("Positive overflow detection") {
            REQUIRE(test_add<int8_t, int8_t>(100, 30) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_add<uint8_t, uint8_t>(200, 100) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(
                test_add<int16_t, int16_t>(std::numeric_limits<int16_t>::max(), 1)
                == tl::unexpected(rav::safe_int_error::overflow)
            );
            REQUIRE(
                test_add<int32_t, int32_t>(std::numeric_limits<int32_t>::max(), 1)
                == tl::unexpected(rav::safe_int_error::overflow)
            );
        }

        SECTION("Negative underflow detection") {
            REQUIRE(test_add<int8_t, int8_t>(-100, -30) == tl::unexpected(rav::safe_int_error::underflow));
            REQUIRE(
                test_add<int16_t, int16_t>(std::numeric_limits<int16_t>::min(), -1)
                == tl::unexpected(rav::safe_int_error::underflow)
            );
            REQUIRE(
                test_add<int32_t, int32_t>(std::numeric_limits<int32_t>::min(), -1)
                == tl::unexpected(rav::safe_int_error::underflow)
            );
        }

        SECTION("Edge cases") {
            // No overflow when adding zero
            REQUIRE(test_add<int8_t, int8_t>(0, 0) == 0);
            REQUIRE(test_add<int8_t, int8_t>(-128, 0) == -128);
            REQUIRE(test_add<int8_t, int8_t>(127, 0) == 127);

            // Overflow/underflow at extremes
            REQUIRE(test_add<int8_t, int8_t>(-1, -128) == tl::unexpected(rav::safe_int_error::underflow));
            REQUIRE(test_add<int8_t, int8_t>(127, 1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_add<int8_t, int8_t>(-128, -1) == tl::unexpected(rav::safe_int_error::underflow));
            REQUIRE(test_add<uint8_t, uint8_t>(255, 1) == tl::unexpected(rav::safe_int_error::overflow));
        }

        SECTION("Unsigned edge cases") {
            REQUIRE(test_add<uint8_t, uint8_t>(0, 0) == 0);
            REQUIRE(test_add<uint8_t, uint8_t>(255, 0) == 255);
            REQUIRE(test_add<uint8_t, uint8_t>(255, 1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_add<uint8_t, uint8_t>(0, 1) == 1);
        }
    }

    SECTION("sub") {
        SECTION("Subtraction without overflow or underflow") {
            REQUIRE(test_sub<int8_t, int8_t>(10, 5) == 5);
            REQUIRE(test_sub<int16_t, int16_t>(2000, 1000) == 1000);
            REQUIRE(test_sub<int32_t, int32_t>(300000, 100000) == 200000);
            REQUIRE(test_sub<uint8_t, uint8_t>(100, 50) == 50);
        }

        SECTION("Negative underflow detection") {
            REQUIRE(test_sub<int8_t, int8_t>(-128, 1) == tl::unexpected(rav::safe_int_error::underflow));
            REQUIRE(
                test_sub<int16_t, int16_t>(std::numeric_limits<int16_t>::min(), 1)
                == tl::unexpected(rav::safe_int_error::underflow)
            );
            REQUIRE(
                test_sub<int32_t, int32_t>(std::numeric_limits<int32_t>::min(), 1)
                == tl::unexpected(rav::safe_int_error::underflow)
            );
        }

        SECTION("Positive overflow detection") {
            REQUIRE(test_sub<int8_t, int8_t>(127, -1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(
                test_sub<int16_t, int16_t>(std::numeric_limits<int16_t>::max(), -1)
                == tl::unexpected(rav::safe_int_error::overflow)
            );
            REQUIRE(
                test_sub<int32_t, int32_t>(std::numeric_limits<int32_t>::max(), -1)
                == tl::unexpected(rav::safe_int_error::overflow)
            );
        }

        SECTION("Edge cases") {
            REQUIRE(test_sub<int8_t, int8_t>(0, 0) == 0);
            REQUIRE(test_sub<int8_t, int8_t>(-128, 0) == -128);
            REQUIRE(test_sub<int8_t, int8_t>(127, 0) == 127);
            REQUIRE(test_sub<int8_t, int8_t>(-1, -128) == 127);
        }

        SECTION("Unsigned edge cases") {
            REQUIRE(test_sub<uint8_t, uint8_t>(0, 0) == 0);
            REQUIRE(test_sub<uint8_t, uint8_t>(255, 255) == 0);
            REQUIRE(test_sub<uint8_t, uint8_t>(0, 1) == tl::unexpected(rav::safe_int_error::underflow));
        }
    }

    SECTION("mul") {
        SECTION("Multiplication without overflow or underflow") {
            REQUIRE(test_mul<int8_t, int8_t>(10, 2) == 20);
            REQUIRE(test_mul<int16_t, int16_t>(100, 20) == 2000);
            REQUIRE(test_mul<int32_t, int32_t>(1000, 2000) == 2000000);
            REQUIRE(test_mul<uint8_t, uint8_t>(10, 5) == 50);
        }

        SECTION("Positive overflow detection") {
            REQUIRE(test_mul<int8_t, int8_t>(100, 2) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_mul<uint8_t, uint8_t>(20, 20) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(
                test_mul<int16_t>(std::numeric_limits<int16_t>::max() / 2 + 1, 2)
                == tl::unexpected(rav::safe_int_error::overflow)
            );
        }

        SECTION("Negative underflow detection") {
            REQUIRE(test_mul<int8_t, int8_t>(-128, 2) == tl::unexpected(rav::safe_int_error::underflow));
            REQUIRE(
                test_mul<int16_t>(std::numeric_limits<int16_t>::min(), 2) == tl::unexpected(rav::safe_int_error::underflow)
            );
            REQUIRE(
                test_mul<int32_t>(std::numeric_limits<int32_t>::min(), 2) == tl::unexpected(rav::safe_int_error::underflow)
            );
        }

        SECTION("Edge cases") {
            REQUIRE(test_mul<int8_t, int8_t>(0, 0) == 0);
            REQUIRE(test_mul<int8_t, int8_t>(127, 0) == 0);
            REQUIRE(test_mul<int8_t, int8_t>(-128, 0) == 0);
            REQUIRE(test_mul<int8_t, int8_t>(-128, -1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_mul<int8_t, int8_t>(127, 1) == 127);
        }

        SECTION("Unsigned edge cases") {
            REQUIRE(test_mul<uint8_t, uint8_t>(255, 0) == 0);
            REQUIRE(test_mul<uint8_t, uint8_t>(255, 1) == 255);
            REQUIRE(test_mul<uint8_t, uint8_t>(255, 2) == tl::unexpected(rav::safe_int_error::overflow));
        }
    }

    SECTION("div") {
        SECTION("Division without overflow or division by zero") {
            REQUIRE(test_div<int8_t, int8_t>(10, 2) == 5);
            REQUIRE(test_div<int16_t, int16_t>(1000, 10) == 100);
            REQUIRE(test_div<int32_t, int32_t>(300000, 100) == 3000);
            REQUIRE(test_div<uint8_t, uint8_t>(100, 5) == 20);
        }

        SECTION("Division by zero detection") {
            REQUIRE(test_div<int8_t, int8_t>(10, 0) == tl::unexpected(rav::safe_int_error::div_by_zero));
            REQUIRE(test_div<int16_t, int16_t>(-100, 0) == tl::unexpected(rav::safe_int_error::div_by_zero));
            REQUIRE(test_div<uint8_t, uint8_t>(0, 0) == tl::unexpected(rav::safe_int_error::div_by_zero));
        }

        SECTION("Overflow detection for signed types") {
            REQUIRE(test_div<int8_t, int8_t>(std::numeric_limits<int8_t>::min(), -1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_div<int16_t, int16_t>(std::numeric_limits<int16_t>::min(), -1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_div<int32_t, int32_t>(std::numeric_limits<int32_t>::min(), -1) == tl::unexpected(rav::safe_int_error::overflow));
        }

        SECTION("Edge cases") {
            REQUIRE(test_div<int8_t, int8_t>(0, 1) == 0);
            REQUIRE(test_div<int8_t, int8_t>(0, -1) == 0);
            REQUIRE(test_div<int8_t, int8_t>(127, 1) == 127);
            REQUIRE(test_div<int8_t, int8_t>(-128, -1) == tl::unexpected(rav::safe_int_error::overflow));
            REQUIRE(test_div<int8_t, int8_t>(-127, -1) == 127);
        }

        SECTION("Unsigned edge cases") {
            REQUIRE(test_div<uint8_t, uint8_t>(255, 1) == 255);
            REQUIRE(test_div<uint8_t, uint8_t>(255, 255) == 1);
            REQUIRE(test_div<uint8_t, uint8_t>(0, 1) == 0);
        }
    }

    SECTION("Chaining") {
        auto r = rav::safe_int<int8_t>(10) + 20 - 5 * 4 / 2;
        REQUIRE(r.value() == 20);
    }

    SECTION("Chaining with error") {
        auto r = rav::safe_int<int8_t>(127) + 1 - 5 * 4 / 2;
        REQUIRE(r.error() == rav::safe_int_error::overflow);
    }
}
