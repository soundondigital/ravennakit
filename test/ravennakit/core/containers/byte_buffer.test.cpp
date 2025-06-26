/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/containers/byte_buffer.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ByteBuffer") {
    rav::ByteBuffer buffer;
    buffer.write_be<uint8_t>(0x12);
    buffer.write_be<uint16_t>(0x3456);
    buffer.write_be<uint32_t>(0x789ABCDE);
    buffer.write_be<uint64_t>(0xFEDCBA9876543210);
    REQUIRE(buffer.size() == 15);
    REQUIRE(buffer.data()[0] == 0x12);
    REQUIRE(buffer.data()[1] == 0x34);
    REQUIRE(buffer.data()[2] == 0x56);
    REQUIRE(buffer.data()[3] == 0x78);
    REQUIRE(buffer.data()[4] == 0x9A);
    REQUIRE(buffer.data()[5] == 0xBC);
    REQUIRE(buffer.data()[6] == 0xDE);
    REQUIRE(buffer.data()[7] == 0xFE);
    REQUIRE(buffer.data()[8] == 0xDC);
    REQUIRE(buffer.data()[9] == 0xBA);
    REQUIRE(buffer.data()[10] == 0x98);
    REQUIRE(buffer.data()[11] == 0x76);
    REQUIRE(buffer.data()[12] == 0x54);
    REQUIRE(buffer.data()[13] == 0x32);
    REQUIRE(buffer.data()[14] == 0x10);
}
