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
#include "ravennakit/ptp/messages/ptp_pdelay_req_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ptp::PdelayReqMessage") {
    SECTION("Unpack") {
        std::array<const uint8_t,30> data{
            0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90,
        };
        auto msg = rav::ptp::PdelayReqMessage::from_data(rav::BufferView(data)).value();
        REQUIRE(msg.origin_timestamp.raw_seconds() == 0x123456789012);
        REQUIRE(msg.origin_timestamp.raw_nanoseconds() == 0x34567890);
    }

    SECTION("Pack") {
        rav::ptp::PdelayReqMessage msg;
        msg.origin_timestamp  = rav::ptp::Timestamp(0x123456789012, 0x34567890);
        rav::ByteBuffer buffer;
        msg.write_to(buffer);
        REQUIRE(buffer.size() == 10);
        REQUIRE(buffer.data()[0] == 0x12);
        REQUIRE(buffer.data()[1] == 0x34);
        REQUIRE(buffer.data()[2] == 0x56);
        REQUIRE(buffer.data()[3] == 0x78);
        REQUIRE(buffer.data()[4] == 0x90);
        REQUIRE(buffer.data()[5] == 0x12);
        REQUIRE(buffer.data()[6] == 0x34);
        REQUIRE(buffer.data()[7] == 0x56);
        REQUIRE(buffer.data()[8] == 0x78);
        REQUIRE(buffer.data()[9] == 0x90);
    }
}
