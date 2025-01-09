/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/math/safe_math.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("safe_math::add") {
    SECTION("Addition without overflow or underflow") {
        REQUIRE(rav::safe_math::add<int8_t>(10, 20) == std::optional<int8_t> {30});
        REQUIRE(rav::safe_math::add<int16_t>(1000, 2000) == std::optional<int16_t> {3000});
        REQUIRE(rav::safe_math::add<int32_t>(100000, 200000) == std::optional<int32_t> {300000});
        REQUIRE(rav::safe_math::add<uint8_t>(100, 50) == std::optional<uint8_t> {150});
    }

    SECTION("Positive overflow detection") {
        REQUIRE(rav::safe_math::add<int8_t>(100, 30) == std::nullopt);    // Exceeds int8_t max
        REQUIRE(rav::safe_math::add<uint8_t>(200, 100) == std::nullopt);  // Exceeds uint8_t max
        REQUIRE(rav::safe_math::add<int16_t>(std::numeric_limits<int16_t>::max(), 1) == std::nullopt);
        REQUIRE(rav::safe_math::add<int32_t>(std::numeric_limits<int32_t>::max(), 1) == std::nullopt);
    }

    SECTION("Negative underflow detection") {
        REQUIRE(rav::safe_math::add<int8_t>(-100, -30) == std::nullopt);  // Exceeds int8_t min
        REQUIRE(rav::safe_math::add<int16_t>(std::numeric_limits<int16_t>::min(), -1) == std::nullopt);
        REQUIRE(rav::safe_math::add<int32_t>(std::numeric_limits<int32_t>::min(), -1) == std::nullopt);
    }

    SECTION("Edge cases") {
        // No overflow when adding zero
        REQUIRE(rav::safe_math::add<int8_t>(0, 0) == std::optional<int8_t> {0});
        REQUIRE(rav::safe_math::add<int8_t>(-128, 0) == std::optional<int8_t> {-128});
        REQUIRE(rav::safe_math::add<int8_t>(127, 0) == std::optional<int8_t> {127});

        // Overflow/underflow at extremes
        REQUIRE(rav::safe_math::add<int8_t>(-1, -128) == std::nullopt);  // Underflow
        REQUIRE(rav::safe_math::add<int8_t>(127, 1) == std::nullopt);    // Overflow
        REQUIRE(rav::safe_math::add<int8_t>(-128, -1) == std::nullopt);  // Underflow
        REQUIRE(rav::safe_math::add<uint8_t>(255, 1) == std::nullopt);   // Overflow
    }

    SECTION("Unsigned edge cases") {
        REQUIRE(rav::safe_math::add<uint8_t>(0, 0) == std::optional<uint8_t> {0});
        REQUIRE(rav::safe_math::add<uint8_t>(255, 0) == std::optional<uint8_t> {255});
        REQUIRE(rav::safe_math::add<uint8_t>(255, 1) == std::nullopt);  // Overflow
        REQUIRE(rav::safe_math::add<uint8_t>(0, 1) == std::optional<uint8_t> {1});
    }
}

TEST_CASE("safe_math::sub") {
    SECTION("Subtraction without overflow or underflow") {
        REQUIRE(rav::safe_math::sub<int8_t>(10, 5) == std::optional<int8_t> {5});
        REQUIRE(rav::safe_math::sub<int16_t>(2000, 1000) == std::optional<int16_t> {1000});
        REQUIRE(rav::safe_math::sub<int32_t>(300000, 100000) == std::optional<int32_t> {200000});
        REQUIRE(rav::safe_math::sub<uint8_t>(100, 50) == std::optional<uint8_t> {50});
    }

    SECTION("Negative underflow detection") {
        REQUIRE(rav::safe_math::sub<int8_t>(-128, 1) == std::nullopt);  // Exceeds int8_t min
        REQUIRE(rav::safe_math::sub<int16_t>(std::numeric_limits<int16_t>::min(), 1) == std::nullopt);
        REQUIRE(rav::safe_math::sub<int32_t>(std::numeric_limits<int32_t>::min(), 1) == std::nullopt);
    }

    SECTION("Positive overflow detection") {
        REQUIRE(rav::safe_math::sub<int8_t>(127, -1) == std::nullopt);  // Exceeds int8_t max
        REQUIRE(rav::safe_math::sub<int16_t>(std::numeric_limits<int16_t>::max(), -1) == std::nullopt);
        REQUIRE(rav::safe_math::sub<int32_t>(std::numeric_limits<int32_t>::max(), -1) == std::nullopt);
    }

    SECTION("Edge cases") {
        REQUIRE(rav::safe_math::sub<int8_t>(0, 0) == std::optional<int8_t> {0});
        REQUIRE(rav::safe_math::sub<int8_t>(-128, 0) == std::optional<int8_t> {-128});
        REQUIRE(rav::safe_math::sub<int8_t>(127, 0) == std::optional<int8_t> {127});
        REQUIRE(rav::safe_math::sub<int8_t>(-1, -128) == std::optional<int8_t> {127});
    }

    SECTION("Unsigned edge cases") {
        REQUIRE(rav::safe_math::sub<uint8_t>(0, 0) == std::optional<uint8_t> {0});
        REQUIRE(rav::safe_math::sub<uint8_t>(255, 255) == std::optional<uint8_t> {0});
        REQUIRE(rav::safe_math::sub<uint8_t>(0, 1) == std::nullopt);  // Underflow
    }
}

TEST_CASE("safe_math::mul") {
    SECTION("Multiplication without overflow or underflow") {
        REQUIRE(rav::safe_math::mul<int8_t>(10, 2) == std::optional<int8_t> {20});
        REQUIRE(rav::safe_math::mul<int16_t>(100, 20) == std::optional<int16_t> {2000});
        REQUIRE(rav::safe_math::mul<int32_t>(1000, 2000) == std::optional<int32_t> {2000000});
        REQUIRE(rav::safe_math::mul<uint8_t>(10, 5) == std::optional<uint8_t> {50});
    }

    SECTION("Positive overflow detection") {
        REQUIRE(rav::safe_math::mul<int8_t>(100, 2) == std::nullopt);   // Exceeds int8_t max
        REQUIRE(rav::safe_math::mul<uint8_t>(20, 20) == std::nullopt);  // Exceeds uint8_t max
        REQUIRE(rav::safe_math::mul<int16_t>(std::numeric_limits<int16_t>::max() / 2 + 1, 2) == std::nullopt);
    }

    SECTION("Negative underflow detection") {
        REQUIRE(rav::safe_math::mul<int8_t>(-128, 2) == std::nullopt);  // Exceeds int8_t min
        REQUIRE(rav::safe_math::mul<int16_t>(std::numeric_limits<int16_t>::min(), 2) == std::nullopt);
        REQUIRE(rav::safe_math::mul<int32_t>(std::numeric_limits<int32_t>::min(), 2) == std::nullopt);
    }

    SECTION("Edge cases") {
        REQUIRE(rav::safe_math::mul<int8_t>(0, 0) == std::optional<int8_t> {0});
        REQUIRE(rav::safe_math::mul<int8_t>(127, 0) == std::optional<int8_t> {0});
        REQUIRE(rav::safe_math::mul<int8_t>(-128, 0) == std::optional<int8_t> {0});
        REQUIRE(rav::safe_math::mul<int8_t>(-128, -1) == std::nullopt);  // Overflow
        REQUIRE(rav::safe_math::mul<int8_t>(127, 1) == std::optional<int8_t> {127});
    }

    SECTION("Unsigned edge cases") {
        REQUIRE(rav::safe_math::mul<uint8_t>(255, 0) == std::optional<uint8_t> {0});
        REQUIRE(rav::safe_math::mul<uint8_t>(255, 1) == std::optional<uint8_t> {255});
        REQUIRE(rav::safe_math::mul<uint8_t>(255, 2) == std::nullopt);  // Overflow
    }
}

TEST_CASE("safe_math::div") {
    SECTION("Division without overflow or division by zero") {
        REQUIRE(rav::safe_math::div<int8_t>(10, 2) == std::optional<int8_t> {5});
        REQUIRE(rav::safe_math::div<int16_t>(1000, 10) == std::optional<int16_t> {100});
        REQUIRE(rav::safe_math::div<int32_t>(300000, 100) == std::optional<int32_t> {3000});
        REQUIRE(rav::safe_math::div<uint8_t>(100, 5) == std::optional<uint8_t> {20});
    }

    SECTION("Division by zero detection") {
        REQUIRE(rav::safe_math::div<int8_t>(10, 0) == std::nullopt);
        REQUIRE(rav::safe_math::div<int16_t>(-100, 0) == std::nullopt);
        REQUIRE(rav::safe_math::div<uint8_t>(0, 0) == std::nullopt);
    }

    SECTION("Overflow detection for signed types") {
        REQUIRE(rav::safe_math::div<int8_t>(std::numeric_limits<int8_t>::min(), -1) == std::nullopt);
        REQUIRE(rav::safe_math::div<int16_t>(std::numeric_limits<int16_t>::min(), -1) == std::nullopt);
        REQUIRE(rav::safe_math::div<int32_t>(std::numeric_limits<int32_t>::min(), -1) == std::nullopt);
    }

    SECTION("Edge cases") {
        REQUIRE(rav::safe_math::div<int8_t>(0, 1) == std::optional<int8_t> {0});   // Zero divided by a positive number
        REQUIRE(rav::safe_math::div<int8_t>(0, -1) == std::optional<int8_t> {0});  // Zero divided by a negative number
        REQUIRE(rav::safe_math::div<int8_t>(127, 1) == std::optional<int8_t> {127});  // Positive number divided by 1
        REQUIRE(rav::safe_math::div<int8_t>(-128, -1) == std::nullopt);               // Overflow
        REQUIRE(rav::safe_math::div<int8_t>(-127, -1) == std::optional<int8_t> {127});               // Overflow
    }

    SECTION("Unsigned edge cases") {
        REQUIRE(rav::safe_math::div<uint8_t>(255, 1) == std::optional<uint8_t> {255});
        REQUIRE(rav::safe_math::div<uint8_t>(255, 255) == std::optional<uint8_t> {1});
        REQUIRE(rav::safe_math::div<uint8_t>(0, 1) == std::optional<uint8_t> {0});
    }
}
