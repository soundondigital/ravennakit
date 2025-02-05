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
        rav::wrapping_uint<T> lhs(1);
        rav::wrapping_uint<T> rhs(1);

        REQUIRE(lhs == rhs);
        REQUIRE_FALSE(lhs != rhs);

        rhs = 2;

        REQUIRE_FALSE(lhs == rhs);
        REQUIRE(lhs != rhs);

        REQUIRE(lhs == 1);
        REQUIRE(lhs != 2);
    }

    SECTION("Relational") {
        rav::wrapping_uint<T> lhs(0);
        rav::wrapping_uint<T> rhs(1);

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
        rav::wrapping_uint<T> seq(0);
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
        rav::wrapping_uint<T> seq(1);
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
        rav::wrapping_uint<T> seq(0);
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
}

TEST_CASE("wrapping_uint") {
    test_wrapping_uint<uint8_t>();
    test_wrapping_uint<uint16_t>();
    test_wrapping_uint<uint32_t>();
    test_wrapping_uint<uint64_t>();
}
