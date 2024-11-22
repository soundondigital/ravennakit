/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_receive_buffer.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_receive_buffer") {
    SECTION("Read with wraparound") {
        rav::rtp_receive_buffer buffer;
        buffer.resize(10, 2);

        std::array<uint8_t, 4> input = {0x0, 0x1, 0x2, 0x3};
        std::array<uint8_t, 4> output = {0x0, 0x1, 0x2, 0x3};

        const rav::buffer_view buffer_view(input.data(), input.size());
        buffer.write(4, buffer_view);
        REQUIRE(buffer.next_ts() == 6);

        buffer.read(0, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        buffer.read(2, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        buffer.read(4, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x1, 0x2, 0x3});

        buffer.read(6, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        buffer.read(8, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        // Here the wraparound happens
        buffer.read(10, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        buffer.read(12, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x0, 0x0, 0x0});

        // This timestamp matches 4
        buffer.read(14, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x0, 0x1, 0x2, 0x3});
    }

    SECTION("Fill buffer in one go") {
        rav::rtp_receive_buffer buffer;
        buffer.resize(4, 2);

        std::array<uint8_t, 8> input = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        std::array<uint8_t, 4> output = {};

        const rav::buffer_view buffer_view(input.data(), input.size());
        buffer.write(2, buffer_view);
        REQUIRE(buffer.next_ts() == 6);

        buffer.read(2, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x1, 0x2, 0x3, 0x4});
        buffer.read(0, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 4> {0x5, 0x6, 0x7, 0x8});
    }

    SECTION("Clear until") {
        rav::rtp_receive_buffer buffer;
        buffer.resize(4, 2);

        std::array<uint8_t, 8> input = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        std::array<uint8_t, 8> output = {};

        const rav::buffer_view buffer_view(input.data(), input.size());
        buffer.write(2, buffer_view);
        REQUIRE(buffer.next_ts() == 6);

        buffer.read(2, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 8> {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8});

        REQUIRE_FALSE(buffer.clear_until(6));
        REQUIRE(buffer.clear_until(8));
        buffer.read(2, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 8> {0x0, 0x0, 0x0, 0x0, 0x5, 0x6, 0x7, 0x8});

        buffer.read(4, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 8> {0x5, 0x6, 0x7, 0x8, 0x0, 0x0, 0x0, 0x0});

        buffer.set_clear_value(0xFF);
        REQUIRE(buffer.clear_until(10));

        buffer.read(4, output.data(), output.size());
        REQUIRE(output == std::array<uint8_t, 8> {0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0, 0x0});
    }
}
