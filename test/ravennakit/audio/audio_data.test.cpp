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

#include "ravennakit/core/util.hpp"

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

TEST_CASE("audio_data | specific conversions", "[audio_data]") {
    SECTION("uint8 to int8") {
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

    SECTION("int8 to int16") {
        SECTION("Convert int8 to int16 be to be") {
            constexpr std::array<uint8_t, 4> src {0xff, 0x7f, 0x81, 0x1};
            std::array<uint8_t, 8> dst {};

            REQUIRE(rav::audio_data::convert<
                    format::int8, byte_order::be, interleaving::interleaved, format::int16, byte_order::be,
                    interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

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

    SECTION("int16 to int24") {
        SECTION("Convert int16 to int24 be to be") {
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
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
            constexpr std::array<uint8_t, 8> src {0x00, 0x80, 0xff, 0x7f, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
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
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
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

    SECTION("int16 to int24in32") {
        SECTION("Convert int16 to int24in32 be to be") {
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
            std::array<uint8_t, 16> dst {};

            REQUIRE(rav::audio_data::convert<
                    format::int16, byte_order::be, interleaving::interleaved, format::int24in32, byte_order::be,
                    interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

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
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
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

    SECTION("int16 to int32") {
        SECTION("Convert int16 to int32 be to be") {
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
            std::array<uint8_t, 16> dst {};

            REQUIRE(rav::audio_data::convert<
                    format::int16, byte_order::be, interleaving::interleaved, format::int32, byte_order::be,
                    interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 2));

            REQUIRE(dst == std::array<uint8_t, 16> {
                0x80, 0x0, 0x0, 0x0,
                0x7f, 0xff,
            });  // Remaining bytes are zero
        }

        SECTION("Convert int16 to int32 be to le") {
            constexpr std::array<uint8_t, 8> src {0x80, 0x00, 0x7f, 0xff, 0x0, 0x0, 0x0, 0x0};  // Min, max, zero
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

    SECTION("int24 to float") {
        SECTION("Convert int24 to float be to be") {
            constexpr std::array<uint8_t, 9> src {0x80, 0x00, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
            std::array<uint8_t, 12> dst {};

            auto result = rav::audio_data::convert<
                format::int24, byte_order::be, interleaving::interleaved, format::f32, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

            REQUIRE(result);
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 0), -1.f, 0.000001f));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 4), +1.f, 0.000001f));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(dst.data() + 8), +0.f, 0.000001f));
        }
    }

    SECTION("int24 to double") {
        SECTION("Convert int24 to double be to be") {
            constexpr std::array<uint8_t, 9> src {0x80, 0x00, 0x0, 0x7f, 0xff, 0xff, 0x0, 0x0, 0x0};  // Min, max, zero
            std::array<uint8_t, 24> dst {};

            auto result = rav::audio_data::convert<
                format::int24, byte_order::be, interleaving::interleaved, format::f64, byte_order::be,
                interleaving::interleaved>(src.data(), src.size(), dst.data(), dst.size(), 1);

            auto f1 = rav::byte_order::read_be<double>(dst.data() + 0);
            auto f2 = rav::byte_order::read_be<double>(dst.data() + 8);
            auto f3 = rav::byte_order::read_be<double>(dst.data() + 16);

            REQUIRE(result);
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 0), -1.0, 0.000001));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 8), +1.0, 0.000001));
            REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(dst.data() + 16), +0.0, 0.000001));
        }
    }
}
