/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/containers/vector_stream.hpp"

#include <array>
#include <catch2/catch_all.hpp>

TEST_CASE("vector_stream", "[vector_stream]") {
    rav::vector_stream<uint32_t> stream;

    SECTION("write") {
        stream.push_back(0x12345678);
        stream.push_back(0x87654321);
        stream.push_back(0x56);
        stream.push_back(0x78);

        REQUIRE_FALSE(stream == std::vector<uint32_t> {0x12345678});

        if constexpr (rav::little_endian) {
            REQUIRE(stream == std::vector<uint32_t> {0x12345678, 0x87654321, 0x56, 0x78});
        } else {
            REQUIRE(stream == std::vector<uint32_t> {0x78563412, 0x21436587, 0x56000000, 0x78000000});
        }
    }

    SECTION("write_be") {
        stream.push_back_be(0x12345678);
        stream.push_back_be(0x87654321);
        stream.push_back_be({0x56, 0x78});

        REQUIRE(stream == std::vector<uint32_t> {0x78563412, 0x21436587, 0x56000000, 0x78000000});
    }

    SECTION("write_le") {
        stream.push_back_le(0x12345678);
        stream.push_back_le(0x87654321);
        stream.push_back_le(0x56);
        stream.push_back_le(0x78);

        REQUIRE(stream == std::vector<uint32_t> {0x12345678, 0x87654321, 0x56, 0x78});
    }

    SECTION("read") {
        stream.push_back(0x12345678);
        stream.push_back(0x87654321);
        stream.push_back(0x56);
        stream.push_back(0x78);

        REQUIRE(stream.read() == 0x12345678);
        REQUIRE(stream.read() == 0x87654321);
        REQUIRE(stream.read() == 0x56);
        REQUIRE(stream.read() == 0x78);
        REQUIRE(stream.read() == 0);
    }

    SECTION("read_le") {
        stream.push_back_le(0x12345678);
        stream.push_back_le(0x87654321);
        stream.push_back_le(0x56);
        stream.push_back_le(0x78);

        REQUIRE(stream.read_le() == 0x12345678);
        REQUIRE(stream.read_le() == 0x87654321);
        REQUIRE(stream.read_le() == 0x56);
        REQUIRE(stream.read_le() == 0x78);
        REQUIRE(stream.read_le() == 0);
    }

    SECTION("read_be") {
        stream.push_back_be(0x12345678);
        stream.push_back_be(0x87654321);
        stream.push_back_be(0x56);
        stream.push_back_be(0x78);

        REQUIRE(stream.read_be() == 0x12345678);
        REQUIRE(stream.read_be() == 0x87654321);
        REQUIRE(stream.read_be() == 0x56);
        REQUIRE(stream.read_be() == 0x78);
        REQUIRE(stream.read_be() == 0);
    }

    SECTION("write le read be") {
        stream.push_back_le(0x12345678);
        REQUIRE(stream.read_be() == 0x78563412);
    }

    SECTION("write be read le") {
        stream.push_back_be(0x12345678);
        REQUIRE(stream.read_le() == 0x78563412);
    }

    SECTION("reset") {
        stream.push_back_be(0x12345678);
        stream.reset();
        REQUIRE(stream.size() == 0); // NOLINT
        REQUIRE(stream.empty());
    }
}
