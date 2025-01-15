/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/streams/byte_stream.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("byte_stream", "[byte_stream]") {
    SECTION("Read") {
        rav::byte_stream stream;

        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.write_ne<uint16_t>(2));
        REQUIRE(stream.write_ne<uint8_t>(3));
        REQUIRE(stream.write_ne<int64_t>(4));

        REQUIRE(stream.get_read_position() == 0);

        REQUIRE(stream.read_ne<uint32_t>() == 1);
        REQUIRE(stream.read_ne<uint16_t>() == 2);
        REQUIRE(stream.read_ne<uint8_t>() == 3);
        REQUIRE(stream.read_ne<int64_t>() == 4);

        REQUIRE(stream.read_ne<int64_t>().has_value() == false);
    }

    SECTION("Set read position") {
        rav::byte_stream stream;
        REQUIRE(stream.write_ne<uint32_t>(1));

        REQUIRE(stream.read_ne<uint32_t>() == 1);
        REQUIRE(stream.set_read_position(0) == true);
        REQUIRE(stream.read_ne<uint32_t>() == 1);
        REQUIRE(stream.set_read_position(5) == false);
    }

    SECTION("Get read position") {
        rav::byte_stream stream;
        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.get_read_position() == 0);
        REQUIRE(stream.read_ne<uint32_t>());
        REQUIRE(stream.get_read_position() == 4);
    }

    SECTION("Get read position") {
        rav::byte_stream stream;
        REQUIRE(stream.size() == 0);
        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.size() == 4);
    }

    SECTION("Set write position") {
        rav::byte_stream stream;
        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.set_write_position(0));
        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.set_write_position(10));
        REQUIRE(stream.get_write_position() == 10);
        REQUIRE(stream.size() == 4);
        REQUIRE(stream.write_ne<uint32_t>(1));
        REQUIRE(stream.size() == 14);
        REQUIRE(stream.get_write_position() == 14);
    }

    SECTION("Flush") {
        rav::byte_stream stream;
        REQUIRE(stream.write_ne<uint32_t>(1));
        stream.flush();
    }

    SECTION("Construct with data") {
        rav::byte_stream stream({0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8});
        REQUIRE(stream.get_read_position() == 0);
        REQUIRE(stream.get_write_position() == 8);
        REQUIRE(stream.size() == 8);
    }
}
