/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/types/uint48.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::uint48_t") {
    SECTION("uint64 to uint48") {
        const rav::uint48_t min(0);
        REQUIRE(static_cast<uint64_t>(min) == 0);

        const rav::uint48_t max(0xffffffffffff);
        REQUIRE(static_cast<uint64_t>(max) == 0xffffffffffff);

        const rav::uint48_t in64max(std::numeric_limits<uint64_t>::max());
        REQUIRE(static_cast<uint64_t>(in64max) == 0xffffffffffff);  // Value is truncated

        const rav::uint48_t forty_eight(48);
        REQUIRE(static_cast<uint64_t>(forty_eight) == 48);
    }
}
