/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/platform/ByteOrder.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("byte_order :: swap_bytes()", "[byte_order]") {
    constexpr uint16_t u16 = 0x1234;
    constexpr uint32_t u32 = 0x12345678;
    constexpr uint64_t u64 = 0x1234567890abcdef;

    REQUIRE(rav::byte_order::swap_bytes(u16) == 0x3412);
    REQUIRE(rav::byte_order::swap_bytes(u32) == 0x78563412);
    REQUIRE(rav::byte_order::swap_bytes(u64) == 0xefcdab9078563412);
}

TEST_CASE("byte_order :: read()", "[byte_order]") {
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
}

TEST_CASE("byte_order :: write()", "[byte_order]") {
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
