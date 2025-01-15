/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util.hpp"
#include "ravennakit/core/streams/input_stream_view.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("input_stream_view") {
    SECTION("raw data") {
        constexpr uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
        rav::input_stream_view stream(data, rav::util::num_elements_in_array(data));
        REQUIRE(stream.read_be<uint32_t>() == 0x11223344);
    }

    SECTION("vector") {
        const std::vector<uint8_t> data = {0x11, 0x22, 0x33, 0x44};
        rav::input_stream_view stream(data);
        REQUIRE(stream.read_be<uint32_t>() == 0x11223344);
    }

    SECTION("array") {
        constexpr std::array<uint8_t, 4> data = {0x11, 0x22, 0x33, 0x44};
        rav::input_stream_view stream(data);
        REQUIRE(stream.read_be<uint32_t>() == 0x11223344);
    }

    SECTION("Other functions") {
        const std::vector<uint8_t> data = {0x11, 0x22, 0x33, 0x44};
        rav::input_stream_view stream(data);
        REQUIRE(stream.size().has_value());
        REQUIRE(stream.size().value() == 4);
        REQUIRE_FALSE(stream.exhausted());
        REQUIRE(stream.get_read_position() == 0);
        REQUIRE(stream.read_be<uint32_t>() == 0x11223344);
        REQUIRE(stream.get_read_position() == 4);
        REQUIRE(stream.exhausted());
        stream.reset();
        REQUIRE(stream.get_read_position() == 0);
        REQUIRE_FALSE(stream.exhausted());
        REQUIRE(stream.read_be<uint32_t>() == 0x11223344);
        REQUIRE(stream.exhausted());
        REQUIRE(stream.set_read_position(1));
        REQUIRE_FALSE(stream.read_be<uint32_t>().has_value());
    }
}
