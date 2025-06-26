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
#include "ravennakit/core/streams/input_stream_view.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::rtp::Packet") {
    SECTION("Encode an RTP packet 2 times") {
        rav::rtp::Packet packet;
        packet.payload_type(0xff);
        packet.sequence_number(0x0012);
        packet.set_timestamp(0x00003456);
        packet.ssrc(0x0000789a);

        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};

        rav::ByteBuffer buffer;
        packet.encode(payload.data(), payload.size(), buffer);
        REQUIRE(buffer.size() == 17);

        SECTION("Test first encoding") {
            rav::InputStreamView stream(buffer);
            REQUIRE(stream.size().value() == 17);
            REQUIRE(stream.read_be<uint8_t>() == 0x80);         // v=2, p=0, x=0, cc=0
            REQUIRE(stream.read_be<uint8_t>() == 0x7f);         // m=0, pt=0xff
            REQUIRE(stream.read_be<uint16_t>() == 0x0012);      // Sequence number
            REQUIRE(stream.read_be<uint32_t>() == 0x00003456);  // Timestamp
            REQUIRE(stream.read_be<uint32_t>() == 0x0000789a);  // SSRC
            REQUIRE(stream.read_be<uint8_t>() == 0x01);         // Payload
            REQUIRE(stream.read_be<uint8_t>() == 0x02);
            REQUIRE(stream.read_be<uint8_t>() == 0x03);
            REQUIRE(stream.read_be<uint8_t>() == 0x04);
            REQUIRE(stream.read_be<uint8_t>() == 0x05);
            REQUIRE(stream.exhausted());
        }

        payload = {0x06, 0x07, 0x08, 0x09};
        packet.sequence_number_inc(1);
        packet.inc_timestamp(2);

        buffer.clear();
        packet.encode(payload.data(), payload.size(), buffer);
        REQUIRE(buffer.size() == 16);

        SECTION("Test second encoding") {
            rav::InputStreamView stream(buffer);
            REQUIRE(stream.size().value() == 16);
            REQUIRE(stream.read_be<uint8_t>() == 0x80);         // v=2, p=0, x=0, cc=0
            REQUIRE(stream.read_be<uint8_t>() == 0x7f);         // Payload type remains the same
            REQUIRE(stream.read_be<uint16_t>() == 0x0013);      // Sequence number increased by 1
            REQUIRE(stream.read_be<uint32_t>() == 0x00003458);  // Timestamp increased by 500
            REQUIRE(stream.read_be<uint32_t>() == 0x0000789a);  // SSRC remains the same
            REQUIRE(stream.read_be<uint8_t>() == 0x06);         // Payload
            REQUIRE(stream.read_be<uint8_t>() == 0x07);
            REQUIRE(stream.read_be<uint8_t>() == 0x08);
            REQUIRE(stream.read_be<uint8_t>() == 0x09);
            REQUIRE(stream.exhausted());
        }
    }
}
