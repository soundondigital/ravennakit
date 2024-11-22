/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/containers/vector_buffer.hpp"

#include <array>
#include <catch2/catch_all.hpp>

TEST_CASE("vector_buffer", "[vector_buffer]") {
    rav::vector_buffer<uint32_t> buffer;

    SECTION("write") {
        buffer.push_back(0x12345678);
        buffer.push_back(0x87654321);
        buffer.push_back(0x56);
        buffer.push_back(0x78);

        REQUIRE_FALSE(buffer == std::vector<uint32_t> {0x12345678});

        if constexpr (rav::little_endian) {
            REQUIRE(buffer == std::vector<uint32_t> {0x12345678, 0x87654321, 0x56, 0x78});
        } else {
            REQUIRE(buffer == std::vector<uint32_t> {0x78563412, 0x21436587, 0x56000000, 0x78000000});
        }
    }

    SECTION("write_be") {
        buffer.push_back_be(0x12345678);
        buffer.push_back_be(0x87654321);
        buffer.push_back_be({0x56, 0x78});

        REQUIRE(buffer == std::vector<uint32_t> {0x78563412, 0x21436587, 0x56000000, 0x78000000});
    }

    SECTION("write_le") {
        buffer.push_back_le(0x12345678);
        buffer.push_back_le(0x87654321);
        buffer.push_back_le(0x56);
        buffer.push_back_le(0x78);

        REQUIRE(buffer == std::vector<uint32_t> {0x12345678, 0x87654321, 0x56, 0x78});
    }

    SECTION("read") {
        buffer.push_back(0x12345678);
        buffer.push_back(0x87654321);
        buffer.push_back(0x56);
        buffer.push_back(0x78);

        REQUIRE(buffer.read() == 0x12345678);
        REQUIRE(buffer.read() == 0x87654321);
        REQUIRE(buffer.read() == 0x56);
        REQUIRE(buffer.read() == 0x78);
        REQUIRE(buffer.read() == 0);
    }

    SECTION("read_le") {
        buffer.push_back_le(0x12345678);
        buffer.push_back_le(0x87654321);
        buffer.push_back_le(0x56);
        buffer.push_back_le(0x78);

        REQUIRE(buffer.read_le() == 0x12345678);
        REQUIRE(buffer.read_le() == 0x87654321);
        REQUIRE(buffer.read_le() == 0x56);
        REQUIRE(buffer.read_le() == 0x78);
        REQUIRE(buffer.read_le() == 0);
    }

    SECTION("read_be") {
        buffer.push_back_be(0x12345678);
        buffer.push_back_be(0x87654321);
        buffer.push_back_be(0x56);
        buffer.push_back_be(0x78);

        REQUIRE(buffer.read_be() == 0x12345678);
        REQUIRE(buffer.read_be() == 0x87654321);
        REQUIRE(buffer.read_be() == 0x56);
        REQUIRE(buffer.read_be() == 0x78);
        REQUIRE(buffer.read_be() == 0);
    }

    SECTION("write le read be") {
        buffer.push_back_le(0x12345678);
        REQUIRE(buffer.read_be() == 0x78563412);
    }

    SECTION("write be read le") {
        buffer.push_back_be(0x12345678);
        REQUIRE(buffer.read_le() == 0x78563412);
    }

    SECTION("reset") {
        buffer.push_back_be(0x12345678);
        buffer.reset();
        REQUIRE(buffer.size() == 0); // NOLINT
        REQUIRE(buffer.empty());
    }
}
