/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/interfaces/mac_address.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("mac_address") {
    SECTION("Construct from string 1") {
        rav::mac_address mac("00:01:02:03:04:05");
        auto bytes = mac.bytes();
        REQUIRE(bytes[0] == 0x00);
        REQUIRE(bytes[1] == 0x01);
        REQUIRE(bytes[2] == 0x02);
        REQUIRE(bytes[3] == 0x03);
        REQUIRE(bytes[4] == 0x04);
        REQUIRE(bytes[5] == 0x05);
    }

    SECTION("Construct from string 2") {
        rav::mac_address mac("1a:2b:3c:d4:e5:e6");
        auto bytes = mac.bytes();
        REQUIRE(bytes[0] == 0x1a);
        REQUIRE(bytes[1] == 0x2b);
        REQUIRE(bytes[2] == 0x3c);
        REQUIRE(bytes[3] == 0xd4);
        REQUIRE(bytes[4] == 0xe5);
        REQUIRE(bytes[5] == 0xe6);
    }
}
