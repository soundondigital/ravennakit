/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/wrapping_uint.hpp"

#include <catch2/catch_all.hpp>

template<class T>
void test_wrapping_uint() {
    SECTION("Equality") {
        rav::WrappingUint<T> lhs(1);
        rav::WrappingUint<T> rhs(1);

        REQUIRE(lhs == rhs);
        REQUIRE_FALSE(lhs != rhs);

        rhs = 2;

        REQUIRE_FALSE(lhs == rhs);
        REQUIRE(lhs != rhs);

        REQUIRE(lhs == 1);
        REQUIRE(lhs != 2);
    }

    SECTION("Relational") {
        rav::WrappingUint<T> lhs(0);
        rav::WrappingUint<T> rhs(1);

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = 2;
        rhs = 0;

        REQUIRE_FALSE(rhs > lhs);
        REQUIRE_FALSE(rhs >= lhs);
        REQUIRE(rhs < lhs);
        REQUIRE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max();
        rhs = 0;

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max() - 10;
        rhs = 10;

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max() / 2;
        rhs = std::numeric_limits<T>::max() / 2;

        REQUIRE_FALSE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max() / 2;
        rhs = std::numeric_limits<T>::max() / 2 + 1;

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max() / 2 - 1;
        rhs = std::numeric_limits<T>::max() / 2;

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max() / 2 + 1;
        rhs = std::numeric_limits<T>::max() / 2;

        REQUIRE_FALSE(rhs > lhs);
        REQUIRE_FALSE(rhs >= lhs);
        REQUIRE(rhs < lhs);
        REQUIRE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max();
        rhs = std::numeric_limits<T>::max() / 2;  // Until half of max, rhs is newer

        REQUIRE(rhs > lhs);
        REQUIRE(rhs >= lhs);
        REQUIRE_FALSE(rhs < lhs);
        REQUIRE_FALSE(rhs <= lhs);

        lhs = std::numeric_limits<T>::max();
        rhs = std::numeric_limits<T>::max() / 2 + 1;  // After half of max, lhs is newer

        REQUIRE_FALSE(rhs > lhs);
        REQUIRE_FALSE(rhs >= lhs);
        REQUIRE(rhs < lhs);
        REQUIRE(rhs <= lhs);
    }

    SECTION("Add") {
        rav::WrappingUint<T> seq(0);
        seq += 1;
        REQUIRE(seq == 1);

        seq = std::numeric_limits<T>::max();
        seq += 1;
        REQUIRE(seq == 0);

        auto seq2 = seq + 1;
        REQUIRE(seq2 == 1);

        seq = std::numeric_limits<T>::max() - 1;
        seq += 3;
        REQUIRE(seq == 1);
    }

    SECTION("Sub") {
        rav::WrappingUint<T> seq(1);
        seq -= 1;
        REQUIRE(seq == 0);

        seq = 0;
        seq -= 1;
        REQUIRE(seq == std::numeric_limits<T>::max());

        auto seq2 = seq - 1;
        REQUIRE(seq2 == std::numeric_limits<T>::max() - 1);

        seq = 1;
        seq -= 3;
        REQUIRE(seq == std::numeric_limits<T>::max() - 1);
    }

    SECTION("Update") {
        rav::WrappingUint<T> seq(0);
        REQUIRE(seq.update(1) == 1);
        REQUIRE(seq == 1);

        REQUIRE(seq.update(1) == 0);
        REQUIRE(seq == 1);

        REQUIRE(seq.update(3) == 2);
        REQUIRE(seq == 3);

        REQUIRE(seq.update(2) == std::nullopt);  // Value is older than current
        REQUIRE(seq == 3);

        seq = std::numeric_limits<T>::max();
        REQUIRE(seq.update(0) == 1);
        REQUIRE(seq == 0);

        seq = std::numeric_limits<T>::max() - 1;
        REQUIRE(seq.update(1) == 3);
        REQUIRE(seq == 1);

        seq = std::numeric_limits<T>::max() / 2;
        REQUIRE(seq.update(std::numeric_limits<T>::max() / 2) == 0);
        REQUIRE(seq == std::numeric_limits<T>::max() / 2);

        seq = std::numeric_limits<T>::max() / 2;
        REQUIRE(seq.update(0) == std::nullopt);  // Value is older than current
        REQUIRE(seq == std::numeric_limits<T>::max() / 2);

        seq = std::numeric_limits<T>::max() / 2;
        REQUIRE(seq.update(std::numeric_limits<T>::max() / 2 - 1) == std::nullopt);  // Value is older than current.
        REQUIRE(seq == std::numeric_limits<T>::max() / 2);

        seq = std::numeric_limits<T>::max() / 2 + 1;
        REQUIRE(seq.update(0).value() == std::numeric_limits<T>::max() / 2 + 1);
        REQUIRE(seq == 0);

        seq = std::numeric_limits<T>::max() / 2 + 2;
        REQUIRE(seq.update(0) == std::numeric_limits<T>::max() / 2);
        REQUIRE(seq == 0);

        seq = std::numeric_limits<T>::max() / 2 + 100;
        REQUIRE(seq.update(0) == std::numeric_limits<T>::max() / 2 - 98);
        REQUIRE(seq == 0);
    }

    SECTION("Difference one") {
        rav::WrappingUint<T> a(0);
        rav::WrappingUint<T> b(1);
        REQUIRE(a.diff(b) == 1);
    }

    SECTION("Difference two") {
        rav::WrappingUint<T> a(0);
        rav::WrappingUint<T> b(2);
        REQUIRE(a.diff(b) == 2);
    }

    SECTION("Difference one wrapped") {
        rav::WrappingUint<T> a(std::numeric_limits<T>::max());
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == 1);
    }

    SECTION("Difference two wrapped") {
        rav::WrappingUint<T> a(std::numeric_limits<T>::max() - 1);
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == 2);
    }

    SECTION("Difference three wrapped") {
        rav::WrappingUint<T> a(std::numeric_limits<T>::max() - 1);
        rav::WrappingUint<T> b(1);
        REQUIRE(a.diff(b) == 3);
    }

    SECTION("Difference one negative") {
        rav::WrappingUint<T> a(1);
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == -1);
    }

    SECTION("Difference big negative") {
        rav::WrappingUint<T> a(std::numeric_limits<T>::max() / 2);
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == -static_cast<std::make_signed_t<T>>(std::numeric_limits<T>::max() / 2));
    }

    SECTION("Difference big negative") {
        rav::WrappingUint<T> a(std::numeric_limits<T>::max() / 2 + 1);
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == std::numeric_limits<std::make_signed_t<T>>::min());
    }

    SECTION("Difference two negative") {
        rav::WrappingUint<T> a(2);
        rav::WrappingUint<T> b(0);
        REQUIRE(a.diff(b) == -2);
    }

    SECTION("Difference one wrapped negative") {
        rav::WrappingUint<T> a(0);
        rav::WrappingUint<T> b(std::numeric_limits<T>::max());
        REQUIRE(a.diff(b) == -1);
    }

    SECTION("Difference two wrapped negative") {
        rav::WrappingUint<T> a(0);
        rav::WrappingUint<T> b(std::numeric_limits<T>::max() - 1);
        REQUIRE(a.diff(b) == -2);
    }

    SECTION("Difference three wrapped negative") {
        rav::WrappingUint<T> a(1);
        rav::WrappingUint<T> b(std::numeric_limits<T>::max() - 1);
        REQUIRE(a.diff(b) == -3);
    }
}

TEST_CASE("rav::WrappingUint") {
    test_wrapping_uint<uint8_t>();
    test_wrapping_uint<uint16_t>();
    test_wrapping_uint<uint32_t>();
    test_wrapping_uint<uint64_t>();
}
