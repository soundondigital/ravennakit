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

#include "ravennakit/containers/buffer_view.hpp"
#include "ravennakit/core/util.hpp"

using namespace rav::audio_data;

namespace {
constexpr float f32_tolerance = 0.00004f;
constexpr double f64_tolerance = 0.00004;
}  // namespace

// MARK: - Interleaving conversions

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

        auto result = rav::audio_data::convert<
            format::int16, byte_order::le, interleaving::interleaved, format::int16, byte_order::le,
            interleaving::noninterleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
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

// MARK: - Endian conversions

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

// MARK: - Sample conversions

TEST_CASE("audio_data | sample conversions", "[audio_data]") {
    SECTION("Minimum value") {
        constexpr std::array<uint8_t, 2> sample {0x80, 0x0};
        std::array<uint8_t, 2> dst {};

        rav::audio_data::detail::convert_sample<format::int16, byte_order::be, format::int16, byte_order::le>(
            sample.data(), dst.data()
        );

        REQUIRE(dst[0] == 0x0);
        REQUIRE(dst[1] == 0x80);
    }

    SECTION("Max value") {
        constexpr std::array<uint8_t, 2> sample {0x7f, 0x0};
        std::array<uint8_t, 2> dst {};

        rav::audio_data::detail::convert_sample<format::int16, byte_order::be, format::int16, byte_order::le>(
            sample.data(), dst.data()
        );

        REQUIRE(dst[0] == 0x0);
        REQUIRE(dst[1] == 0x7f);
    }
}

// MARK: - int24_t

TEST_CASE("audio_data | int24_t", "[audio_data]") {
    SECTION("int32 to int24") {
        const detail::int24_t min(-8388608);
        REQUIRE(static_cast<int32_t>(min) == -8388608);

        const detail::int24_t max(8388607);
        REQUIRE(static_cast<int32_t>(max) == 8388607);

        const detail::int24_t zero(0);
        REQUIRE(static_cast<int32_t>(zero) == 0);
    }
}

// MARK: - Specific conversions

TEST_CASE("audio_data | uint8 to int8", "[audio_data]") {
    SECTION("Convert uint8 to int8 be to be") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x80, 0x81, 0x0};
        std::array<uint8_t, 4> dst {};

        auto result = rav::audio_data::convert<
            format::uint8, byte_order::be, interleaving::interleaved, format::int8, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 4> {0x7f, 0x0, 0x1, 0x80});
    }

    SECTION("Convert uint8 to int8 be to le") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x80, 0x81, 0x0};
        std::array<uint8_t, 4> dst {};

        auto result = rav::audio_data::convert<
            format::uint8, byte_order::be, interleaving::interleaved, format::int8, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 4> {0x7f, 0x0, 0x1, 0x80});
    }

    SECTION("Convert uint8 to int8 le to be") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x80, 0x81, 0x0};
        std::array<uint8_t, 4> dst {};

        auto result = rav::audio_data::convert<
            format::uint8, byte_order::le, interleaving::interleaved, format::int8, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 4> {0x7f, 0x0, 0x1, 0x80});
    }
}

TEST_CASE("audio_data | int8 to int16", "[audio_data]") {
    SECTION("Convert int8 to int16 be to be") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x7f, 0x81, 0x1};
        std::array<uint8_t, 8> dst {};

        auto result = rav::audio_data::convert<
            format::int8, byte_order::be, interleaving::interleaved, format::int16, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 8> {0xff, 0x0, 0x7f, 0x0, 0x81, 0x0, 0x1, 0x0});
    }

    SECTION("Convert int8 to int16 le to be") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x7f, 0x81, 0x1};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int8, byte_order::le, interleaving::interleaved, format::int16, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0xff, 0x0, 0x7f, 0x0, 0x81, 0x0, 0x1, 0x0});
    }

    SECTION("Convert int8 to int16 be to le") {
        constexpr std::array<uint8_t, 4> src {0xff, 0x7f, 0x81, 0x1};
        std::array<uint8_t, 8> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int8, byte_order::be, interleaving::interleaved, format::int16, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 8> {0x0, 0xff, 0x0, 0x7f, 0x0, 0x81, 0x0, 0x1});
    }
}

TEST_CASE("audio_data | int16 to int24", "[audio_data]") {
    SECTION("Convert int16 to int24 be to be") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::int24, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 12> {
                0x80, 0x0, 0x0,
                0x7f, 0xff, 0x0,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int24 le to be") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x80, 0xff, 0x7f, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::le, interleaving::interleaved, format::int24, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 12> {
                0x80, 0x0, 0x0,
                0x7f, 0xff, 0x0,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int24 be to le") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::int24, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 12> {
                0x0, 0x0, 0x80,
                0x0, 0xff, 0x7f,
            });  // Remaining bytes are zero
    }
}

TEST_CASE("audio_data | int16 to int24in32", "[audio_data]") {
    SECTION("Convert int16 to int24in32 be to be") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::int24in32, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 16> {
                0x80, 0x0, 0x0, 0x0,
                0x7f, 0xff, 0x0, 0x0,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int24in32 le to be") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x80, 0xff, 0x7f, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int24in32, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 16> {
                0x80, 0x0, 0x0, 0x0,
                0x7f, 0xff, 0x0, 0x0,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int24in32 be to le") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::be, interleaving::interleaved, format::int24in32, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 16> {
                0x0, 0x0, 0x0, 0x80,
                0x0, 0x0, 0xff, 0x7f,
            });  // Remaining bytes are zero
    }
}

TEST_CASE("audio_data | int16 to int32", "[audio_data]") {
    SECTION("Convert int16 to int32 be to be") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::int32, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 16> {
                0x80, 0x0, 0x0, 0x0,
                0x7f, 0xff,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int32 be to le") {
        constexpr std::array<uint8_t, 8> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::be, interleaving::interleaved, format::int32, byte_order::le,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 16> {
                0x0, 0x0, 0x0, 0x80,
                0x0, 0x0, 0xff, 0x7f,
            });  // Remaining bytes are zero
    }

    SECTION("Convert int16 to int32 le to be") {
        constexpr std::array<uint8_t, 8> src {0x0, 0x80, 0xff, 0x7f, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 16> dst {};

        REQUIRE(rav::audio_data::convert<
                format::int16, byte_order::le, interleaving::interleaved, format::int32, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

        REQUIRE(dst == std::array<uint8_t, 16> {
                0x80, 0x0, 0x0, 0x0,
                0x7f, 0xff,
            });  // Remaining bytes are zero
    }
}

TEST_CASE("audio_data | int16 to float", "[audio_data]") {
    SECTION("Convert int16 to float be to be") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f32, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 0), -1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 4), +1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 8), +0.f, f32_tolerance));
    }

    SECTION("Convert int16 to float be to le") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f32, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 0), -1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 4), +1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 8), +0.f, f32_tolerance));
    }

    SECTION("Convert int16 to float be to ne") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f32, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);

        if constexpr (rav::little_endian) {
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 0), -1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 4), +1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 8), +0.f, f32_tolerance));
        } else {
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 0), -1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 4), +1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 8), +0.f, f32_tolerance));
        }
    }
}

TEST_CASE("audio_data | int24 to float", "[audio_data]") {
    SECTION("Convert int24 to float be to be") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f32, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 0), -1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 4), +1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 8), +0.f, f32_tolerance));
    }

    SECTION("Convert int24 to float be to le") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f32, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 0), -1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 4), +1.f, f32_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 8), +0.f, f32_tolerance));
    }

    SECTION("Convert int24 to float be to ne") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 12> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f32, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);

        if constexpr (rav::little_endian) {
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 0), -1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 4), +1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(dst.data() + 8), +0.f, f32_tolerance));
        } else {
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 0), -1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 4), +1.f, f32_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 8), +0.f, f32_tolerance));
        }
    }
}

TEST_CASE("audio_data | int16 to double", "[audio_data]") {
    SECTION("Convert int16 to double be to be") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f64, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 0), -1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 8), +1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 16), +0.0, f64_tolerance));
    }

    SECTION("Convert int16 to double be to le") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f64, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 0), -1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 8), +1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 16), +0.0, f64_tolerance));
    }

    SECTION("Convert int16 to double be to ne") {
        constexpr std::array<uint8_t, 6> src {0x80, 0x0, 0x7f, 0xff, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int16, byte_order::be, interleaving::interleaved, format::f64, byte_order::ne,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);

        if constexpr (rav::little_endian) {
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 0), -1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 8), +1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 16), +0.0, f64_tolerance));
        } else {
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 0), -1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 8), +1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 16), +0.0, f64_tolerance));
        }
    }
}

TEST_CASE("audio_data | int24 to double", "[audio_data]") {
    SECTION("Convert int24 to double be to be") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f64, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 0), -1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 8), +1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 16), +0.0, f64_tolerance));
    }

    SECTION("Convert int24 to double be to le") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f64, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 0), -1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 8), +1.0, f64_tolerance));
        REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 16), +0.0, f64_tolerance));
    }

    SECTION("Convert int24 to double be to ne") {
        constexpr std::array<uint8_t, 9> src {0x80, 0x0, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
        std::array<uint8_t, 24> dst {};

        auto result = rav::audio_data::convert<
            format::int24, byte_order::be, interleaving::interleaved, format::f64, byte_order::ne,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);

        if constexpr (rav::little_endian) {
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 0), -1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 8), +1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(dst.data() + 16), +0.0, f64_tolerance));
        } else {
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 0), -1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 8), +1.0, f64_tolerance));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 16), +0.0, f64_tolerance));
        }
    }
}

TEST_CASE("audio_data | float to int16", "[audio_data]") {
    SECTION("Convert float to int16 be to be") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_be<float>(src.data(), -1.f);
        rav::byte_order::write_be<float>(src.data() + 4, 1.f);
        rav::byte_order::write_be<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 6> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::be, interleaving::interleaved, format::int16, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 0) == -32767);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 2) == 32767);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 4) == 0);
    }

    SECTION("Convert float to int16 be to le") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_be<float>(src.data(), -1.f);
        rav::byte_order::write_be<float>(src.data() + 4, 1.f);
        rav::byte_order::write_be<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 6> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::be, interleaving::interleaved, format::int16, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::byte_order::read_le<int16_t>(dst.data() + 0) == -32767);
        REQUIRE(rav::byte_order::read_le<int16_t>(dst.data() + 2) == 32767);
        REQUIRE(rav::byte_order::read_le<int16_t>(dst.data() + 4) == 0);
    }

    SECTION("Convert float to int16 le to be") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_le<float>(src.data(), -1.f);
        rav::byte_order::write_le<float>(src.data() + 4, 1.f);
        rav::byte_order::write_le<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 6> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::le, interleaving::interleaved, format::int16, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 0) == -32767);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 2) == 32767);
        REQUIRE(rav::byte_order::read_be<int16_t>(dst.data() + 4) == 0);
    }
}

TEST_CASE("audio_data | float to int24", "[audio_data]") {
    SECTION("Convert float to int24 be to be") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_be<float>(src.data(), -1.f);
        rav::byte_order::write_be<float>(src.data() + 4, 1.f);
        rav::byte_order::write_be<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 9> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::be, interleaving::interleaved, format::int24, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 9>{
            0x80, 0x0, 0x1,
            0x7f, 0xff, 0xff,
            0x0, 0x0, 0x0,
        });
    }

    SECTION("Convert float to int24 be to le") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_be<float>(src.data(), -1.f);
        rav::byte_order::write_be<float>(src.data() + 4, 1.f);
        rav::byte_order::write_be<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 9> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::be, interleaving::interleaved, format::int24, byte_order::le,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 9>{
            0x1, 0x0, 0x80,
            0xff, 0xff, 0x7f,
            0x0, 0x0, 0x0,
        });
    }

    SECTION("Convert float to int24 le to be") {
        std::array<uint8_t, 12> src {};

        // Min, max, zero
        rav::byte_order::write_le<float>(src.data(), -1.f);
        rav::byte_order::write_le<float>(src.data() + 4, 1.f);
        rav::byte_order::write_le<float>(src.data() + 8, 0.f);

        std::array<uint8_t, 9> dst {};

        auto result = rav::audio_data::convert<
            format::f32, byte_order::le, interleaving::interleaved, format::int24, byte_order::be,
            interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

        REQUIRE(result);
        REQUIRE(dst == std::array<uint8_t, 9>{
            0x80, 0x0, 0x1,
            0x7f, 0xff, 0xff,
            0x0, 0x0, 0x0,
        });
    }
}
