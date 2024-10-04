/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/byte_order.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("byte_order | swap_bytes()", "[byte_order]") {
    SECTION("Types") {
        constexpr uint16_t u16 = 0x1234;
        constexpr uint32_t u32 = 0x12345678;
        constexpr uint64_t u64 = 0x1234567890abcdef;
        constexpr float f32 = -1.1f;
        constexpr double f64 = -1.1;

        REQUIRE(rav::byte_order::swap_bytes(u16) == 0x3412);
        REQUIRE(rav::byte_order::swap_bytes(u32) == 0x78563412);
        REQUIRE(rav::byte_order::swap_bytes(u64) == 0xefcdab9078563412);

        auto swapped_float = rav::byte_order::swap_bytes(f32);
        auto swapped_float_data = reinterpret_cast<const unsigned char*>(&swapped_float);
        #if RAV_LITTLE_ENDIAN
        REQUIRE(swapped_float_data[0] == 0xbf);
        REQUIRE(swapped_float_data[1] == 0x8c);
        REQUIRE(swapped_float_data[2] == 0xcc);
        REQUIRE(swapped_float_data[3] == 0xcd);
        #else
        #warning "Big endian systems have not been tested yet"
        REQUIRE(swapped_float_data[0] == 0xcd);
        REQUIRE(swapped_float_data[1] == 0xcc);
        REQUIRE(swapped_float_data[2] == 0x8c);
        REQUIRE(swapped_float_data[3] == 0xbf);
        #endif

        auto swapped_double = rav::byte_order::swap_bytes(f64);
        auto swapped_double_data = reinterpret_cast<const unsigned char*>(&swapped_double);
        #if RAV_LITTLE_ENDIAN
        REQUIRE(swapped_double_data[0] == 0xbf);
        REQUIRE(swapped_double_data[1] == 0xf1);
        REQUIRE(swapped_double_data[2] == 0x99);
        REQUIRE(swapped_double_data[3] == 0x99);
        REQUIRE(swapped_double_data[4] == 0x99);
        REQUIRE(swapped_double_data[5] == 0x99);
        REQUIRE(swapped_double_data[6] == 0x99);
        REQUIRE(swapped_double_data[7] == 0x9a);
        #else
        #warning "Big endian systems have not been tested yet"
        REQUIRE(swapped_double_data[0] == 0x9a);
        REQUIRE(swapped_double_data[1] == 0x99);
        REQUIRE(swapped_double_data[2] == 0x99);
        REQUIRE(swapped_double_data[3] == 0x99);
        REQUIRE(swapped_double_data[4] == 0x99);
        REQUIRE(swapped_double_data[5] == 0x99);
        REQUIRE(swapped_double_data[6] == 0xf1);
        REQUIRE(swapped_double_data[7] == 0xbf);
        #endif

        // Swap back and forth and check if the result is within a tolerance of the original value
        REQUIRE(rav::util::is_within(rav::byte_order::swap_bytes(rav::byte_order::swap_bytes(f32)), f32, 0.f));
        REQUIRE(rav::util::is_within(rav::byte_order::swap_bytes(rav::byte_order::swap_bytes(f64)), f64, 0.0));
    }

    SECTION("24 bit swapping") {
        std::array<uint8_t, 3> u24 = {0x12, 0x34, 0x56};
        rav::byte_order::swap_bytes(u24.data(), u24.size());
        REQUIRE(u24 == std::array<uint8_t, 3>{0x56, 0x34, 0x12});
    }

    SECTION("24 bit swapping") {
        std::array<uint8_t, 3> u24{0x12, 0x34, 0x56};
        REQUIRE(sizeof(u24) == 3);
        u24 = rav::byte_order::swap_bytes(u24);
        REQUIRE(u24[0] == 0x56);
        REQUIRE(u24[1] == 0x34);
        REQUIRE(u24[2] == 0x12);
    }

    SECTION("32 bit swapping") {
        std::array<uint8_t, 4> u24 = {0x12, 0x34, 0x56, 0x78};
        rav::byte_order::swap_bytes(u24.data(), u24.size());
        REQUIRE(u24 == std::array<uint8_t, 4>{0x78, 0x56, 0x34, 0x12});
    }
}

TEST_CASE("byte_order | read()", "[byte_order]") {
    constexpr uint8_t u16be[] = {0x12, 0x34};
    constexpr uint8_t u16le[] = {0x34, 0x12};

    REQUIRE(rav::byte_order::read_be<uint16_t>(u16be) == 0x1234);
    REQUIRE(rav::byte_order::read_le<uint16_t>(u16le) == 0x1234);

    constexpr uint8_t u32be[] = {0x12, 0x34, 0x56, 0x78};
    constexpr uint8_t u32le[] = {0x78, 0x56, 0x34, 0x12};

    REQUIRE(rav::byte_order::read_be<uint32_t>(u32be) == 0x12345678);
    REQUIRE(rav::byte_order::read_le<uint32_t>(u32le) == 0x12345678);

    constexpr uint8_t u64be[] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef};
    constexpr uint8_t u64le[] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12};

    REQUIRE(rav::byte_order::read_be<uint64_t>(u64be) == 0x1234567890abcdef);
    REQUIRE(rav::byte_order::read_le<uint64_t>(u64le) == 0x1234567890abcdef);

    constexpr uint8_t f32_be[] = {0xbf, 0x8c, 0xcc, 0xcd};  // -1.1f (big endian)
    constexpr uint8_t f32_le[] = {0xcd, 0xcc, 0x8c, 0xbf};  // -1.1f (little endian)
    REQUIRE(rav::util::is_within(rav::byte_order::read_be<float>(f32_be), -1.1f, 0.f));
    REQUIRE(rav::util::is_within(rav::byte_order::read_le<float>(f32_le), -1.1f, 0.f));

    constexpr uint8_t f64_be[] = {0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0xbf};  // -1.1 (big endian)
    constexpr uint8_t f64_le[] = {0xbf, 0xf1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9a};  // -1.1 (little endian)
    REQUIRE(rav::util::is_within(rav::byte_order::read_be<double>(f64_le), -1.1, 0.0));
    REQUIRE(rav::util::is_within(rav::byte_order::read_le<double>(f64_be), -1.1, 0.0));
}

TEST_CASE("byte_order | write()", "[byte_order]") {
    uint8_t u16[] = {0x0, 0x0};

    rav::byte_order::write_be<uint16_t>(u16, 0x1234);
    REQUIRE(u16[0] == 0x12);
    REQUIRE(u16[1] == 0x34);

    rav::byte_order::write_le<uint16_t>(u16, 0x1234);
    REQUIRE(u16[0] == 0x34);
    REQUIRE(u16[1] == 0x12);

    uint8_t u32[] = {0x0, 0x0, 0x0, 0x0};

    rav::byte_order::write_be<uint32_t>(u32, 0x12345678);
    REQUIRE(u32[0] == 0x12);
    REQUIRE(u32[1] == 0x34);
    REQUIRE(u32[2] == 0x56);
    REQUIRE(u32[3] == 0x78);

    rav::byte_order::write_le<uint32_t>(u32, 0x12345678);
    REQUIRE(u32[0] == 0x78);
    REQUIRE(u32[1] == 0x56);
    REQUIRE(u32[2] == 0x34);
    REQUIRE(u32[3] == 0x12);

    uint8_t u64[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    rav::byte_order::write_be<uint64_t>(u64, 0x1234567890abcdef);
    REQUIRE(u64[0] == 0x12);
    REQUIRE(u64[1] == 0x34);
    REQUIRE(u64[2] == 0x56);
    REQUIRE(u64[3] == 0x78);
    REQUIRE(u64[4] == 0x90);
    REQUIRE(u64[5] == 0xab);
    REQUIRE(u64[6] == 0xcd);
    REQUIRE(u64[7] == 0xef);

    rav::byte_order::write_le<uint64_t>(u64, 0x1234567890abcdef);
    REQUIRE(u64[0] == 0xef);
    REQUIRE(u64[1] == 0xcd);
    REQUIRE(u64[2] == 0xab);
    REQUIRE(u64[3] == 0x90);
    REQUIRE(u64[4] == 0x78);
    REQUIRE(u64[5] == 0x56);
    REQUIRE(u64[6] == 0x34);
    REQUIRE(u64[7] == 0x12);
}
