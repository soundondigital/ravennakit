/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/audio/audio_data.hpp"

#include <catch2/catch_all.hpp>

using namespace rav::audio_data;

TEST_CASE("audio_data::interleaving", "[audio_data]") {
    using namespace rav::audio_data::interleaving;
    SECTION("get_sample_index()") {
        constexpr size_t num_frames = 3;
        constexpr size_t num_channels = 2;

        SECTION("interleaved") {
            REQUIRE(interleaved::get_sample_index(0, 0, num_channels, num_frames) == 0);
            REQUIRE(interleaved::get_sample_index(1, 0, num_channels, num_frames) == 1);
            REQUIRE(interleaved::get_sample_index(0, 1, num_channels, num_frames) == 2);
            REQUIRE(interleaved::get_sample_index(1, 1, num_channels, num_frames) == 3);
            REQUIRE(interleaved::get_sample_index(0, 2, num_channels, num_frames) == 4);
            REQUIRE(interleaved::get_sample_index(1, 2, num_channels, num_frames) == 5);
        }

        SECTION("noninterleaved") {
            REQUIRE(noninterleaved::get_sample_index(0, 0, num_channels, num_frames) == 0);
            REQUIRE(noninterleaved::get_sample_index(0, 1, num_channels, num_frames) == 1);
            REQUIRE(noninterleaved::get_sample_index(0, 2, num_channels, num_frames) == 2);
            REQUIRE(noninterleaved::get_sample_index(1, 0, num_channels, num_frames) == 3);
            REQUIRE(noninterleaved::get_sample_index(1, 1, num_channels, num_frames) == 4);
            REQUIRE(noninterleaved::get_sample_index(1, 2, num_channels, num_frames) == 5);
        }
    }
}

TEST_CASE("audio_data::copy()", "[audio_data]") {
    SECTION("Convert int8 to int16 little endian interleaved") {
        constexpr std::array<uint8_t, 4> src {0x0, 0x01, 0x02, 0x03};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int8<byte_order::le>, interleaving::interleaved, format::int16<byte_order::le>,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x0, 0x01, 0x0, 0x02, 0x0, 0x03, 0x0});
    }

    SECTION("Convert int8 to int8 little endian from interleaved to non interleaved") {
        constexpr std::array<uint8_t, 4> src {0x0, 0x01, 0x02, 0x03};
        std::array<uint8_t, 4> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int8<byte_order::le>, interleaving::interleaved, format::int8<byte_order::le>,
                interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 4> {0x0, 0x2, 0x1, 0x3});
    }
}
