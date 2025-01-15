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
#include "ravennakit/ptp/messages/ptp_delay_resp_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_delay_resp_message") {
    SECTION("Unpack") {
        std::array<const uint8_t, 30> data {
            0x0,  0x1,  0x2,  0x3,  0x4,  0x5,               // ts seconds
            0x6,  0x7,  0x8,  0x9,                           // ts nanoseconds
            0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,  // clock identity
        };
        auto msg = rav::ptp_delay_resp_message::from_data({}, rav::buffer_view(data)).value();
        REQUIRE(msg.receive_timestamp.seconds() == 0x102030405);
        REQUIRE(msg.receive_timestamp.nanoseconds() == 0x06070809);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[0] == 0x10);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[1] == 0x20);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[2] == 0x30);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[3] == 0x40);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[4] == 0x50);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[5] == 0x60);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[6] == 0x70);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[7] == 0x80);
    }
}
