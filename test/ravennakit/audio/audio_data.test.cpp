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

TEST_CASE("audio_data | interleaving", "[audio_data]") {
    SECTION("Interleaved to interleaved int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int16, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
    }

    SECTION("Interleaved to interleaved int32") {
        constexpr std::array<uint8_t, 16> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int32, byte_order::le, interleaving::interleaved, format::int32, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(
            dst
            == std::array<uint8_t, 16> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}
        );
    }

    SECTION("Noninterleaved to noninterleaved int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::noninterleaved, format::int16, byte_order::le,
                interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
    }

    SECTION("Noninterleaved to noninterleaved int32") {
        constexpr std::array<uint8_t, 16> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int32, byte_order::le, interleaving::noninterleaved, format::int32, byte_order::le,
                interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(
            dst
            == std::array<uint8_t, 16> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}
        );
    }

    SECTION("Interleaved to non-interleaved int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int16, byte_order::le,
                interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x4, 0x5, 0x2, 0x3, 0x6, 0x7});
    }

    SECTION("Interleaved to non-interleaved int32") {
        constexpr std::array<uint8_t, 16> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int32, byte_order::le, interleaving::interleaved, format::int32, byte_order::le,
                interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(
            dst
            == std::array<uint8_t, 16> {0x0, 0x1, 0x2, 0x3, 0x8, 0x9, 0xA, 0xB, 0x4, 0x5, 0x6, 0x7, 0xC, 0xD, 0xE, 0xF}
        );
    }

    SECTION("Noninterleaved to interleaved int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::noninterleaved, format::int16, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x4, 0x5, 0x2, 0x3, 0x6, 0x7});
    }

    SECTION("Noninterleaved to interleaved int32") {
        constexpr std::array<uint8_t, 16> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int32, byte_order::le, interleaving::noninterleaved, format::int32, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(
            dst
            == std::array<uint8_t, 16> {0x0, 0x1, 0x2, 0x3, 0x8, 0x9, 0xA, 0xB, 0x4, 0x5, 0x6, 0x7, 0xC, 0xD, 0xE, 0xF}
        );
    }
}

TEST_CASE("audio_data | endian conversions", "[audio_data]") {
    SECTION("Big-endian to little-endian int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::be, interleaving::interleaved, format::int16, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6});
    }

    SECTION("Big-endian to native-endian int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::be, interleaving::interleaved, format::int16, byte_order::ne,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

#if RAV_LITTLE_ENDIAN
        REQUIRE(dst == std::array<uint8_t, 8> {0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6});
#else
        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
#endif
    }

    SECTION("Big-endian to big-endian int16") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::be, interleaving::interleaved, format::int16, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
    }

    SECTION("Little-endian to big-endian int16") {
        constexpr std::array<uint8_t, 8> src {0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int16, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
    }

    SECTION("Little-endian to native-endian int16") {
        constexpr std::array<uint8_t, 8> src {0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int16, byte_order::ne,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

#if RAV_LITTLE_ENDIAN
        REQUIRE(dst == std::array<uint8_t, 8> {0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6});
#else
        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
#endif
    }
}

TEST_CASE("audio_data | sample size conversions", "[audio_data]") {
    SECTION("Convert int8 to int16") {
        constexpr std::array<uint8_t, 4> src {0x0, 0x01, 0x02, 0x03};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int8, byte_order::be, interleaving::interleaved, format::int16, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x0, 0x0, 0x1, 0x0, 0x2, 0x0, 0x3});
    }

    SECTION("Convert uint8 to uint16") {
        constexpr std::array<uint8_t, 4> src {0x0, 0x01, 0x02, 0x03};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::uint8, byte_order::be, interleaving::interleaved, format::uint16, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0x0, 0x0, 0x1, 0x0, 0x2, 0x0, 0x3});
    }
}

TEST_CASE("audio_data | scale sample", "[audio_data]") {}
