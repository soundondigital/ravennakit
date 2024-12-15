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
#include "ravennakit/ptp/messages/ptp_pdelay_resp_follow_up_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_pdelay_resp_follow_up_message") {
    SECTION("Unpack") {
        std::array<const uint8_t, 30> data {
            0x12, 0x34, 0x56, 0x78, 0x90, 0x12,              // ts seconds
            0x34, 0x56, 0x78, 0x90,                          // ts nanoseconds
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,  // port identity
            0x99, 0xaa                                       // Port number
        };
        auto msg = rav::ptp_pdelay_resp_follow_up_message::from_data(rav::buffer_view(data)).value();
        REQUIRE(msg.response_origin_timestamp.seconds == 0x123456789012);
        REQUIRE(msg.response_origin_timestamp.nanoseconds == 0x34567890);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[0] == 0x11);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[1] == 0x22);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[2] == 0x33);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[3] == 0x44);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[4] == 0x55);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[5] == 0x66);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[6] == 0x77);
        REQUIRE(msg.requesting_port_identity.clock_identity.data[7] == 0x88);
    }

    SECTION("Pack") {
        rav::ptp_pdelay_resp_follow_up_message msg;
        msg.response_origin_timestamp.seconds = 0x123456789012;
        msg.response_origin_timestamp.nanoseconds = 0x34567890;
        msg.requesting_port_identity.clock_identity.data[0] = 0x11;
        msg.requesting_port_identity.clock_identity.data[1] = 0x22;
        msg.requesting_port_identity.clock_identity.data[2] = 0x33;
        msg.requesting_port_identity.clock_identity.data[3] = 0x44;
        msg.requesting_port_identity.clock_identity.data[4] = 0x55;
        msg.requesting_port_identity.clock_identity.data[5] = 0x66;
        msg.requesting_port_identity.clock_identity.data[6] = 0x77;
        msg.requesting_port_identity.clock_identity.data[7] = 0x88;
        msg.requesting_port_identity.port_number = 0x99aa;
        rav::byte_stream stream;
        msg.write_to(stream);
        REQUIRE(stream.size() == 20);
        REQUIRE(stream.read_be<rav::uint48_t>() == 0x123456789012);  // seconds
        REQUIRE(stream.read_be<uint32_t>() == 0x34567890);           // nanoseconds
        REQUIRE(stream.read_be<uint8_t>() == 0x11);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x22);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x33);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x44);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x55);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x66);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x77);                  // clock identity
        REQUIRE(stream.read_be<uint8_t>() == 0x88);                  // clock identity
        REQUIRE(stream.read_be<uint16_t>() == 0x99aa);               // port number
    }
}
