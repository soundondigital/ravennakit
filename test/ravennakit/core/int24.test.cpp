/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/int24.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("int24_t") {
    SECTION("int32 to int24") {
        const rav::int24_t min(-8388608);
        REQUIRE(static_cast<int32_t>(min) == -8388608);

        const rav::int24_t max(8388607);
        REQUIRE(static_cast<int32_t>(max) == 8388607);

        const rav::int24_t zero(0);
        REQUIRE(static_cast<int32_t>(zero) == 0);

        const rav::int24_t twenty_four(24);
        REQUIRE(static_cast<int32_t>(twenty_four) == 24);

        const rav::int24_t in32max(std::numeric_limits<int32_t>::max());
        REQUIRE(static_cast<int32_t>(in32max) == 8388607);  // Value is truncated

        const rav::int24_t in32min(std::numeric_limits<int32_t>::min());
        REQUIRE(static_cast<int32_t>(in32min) == -8388608);  // Value is truncated
    }
}
