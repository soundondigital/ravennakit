/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <array>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ravennakit/rtp/rtp_packet_view.hpp"

TEST_CASE("rtp_packet_view::rtp_packet_view()", "[rtp_packet_view]") {
    uint8_t data[] = {

        // v, p, x, cc
        0b10000000,
        // m, pt
        0b01100010,
        // sequence number
        0xab, 0xcd,
        // timestamp
        0xab, 0xcd, 0xef, 0x01,
        // ssrc
        0x01, 0x02, 0x03, 0x04
    };

    SECTION("A header with invalid data should result not pass validation") {
        rav::rtp::PacketView packet(data, sizeof(data) - 1);
        REQUIRE_FALSE(packet.validate());
    }

    SECTION("A header with more data should pass validation") {
        rav::rtp::PacketView packet(data, sizeof(data) + 1);
        REQUIRE(packet.validate());
    }

    rav::rtp::PacketView packet(data, sizeof(data));

    SECTION("Status should be ok") {
        REQUIRE(packet.validate());
    }

    SECTION("Version should be 2") {
        REQUIRE(packet.version() == 2);
    }

    SECTION("There should be no padding") {
        REQUIRE(packet.padding() == false);
    }

    SECTION("Extension should be false") {
        REQUIRE(packet.extension() == false);
    }

    SECTION("CSRC Count should be 0") {
        REQUIRE(packet.csrc_count() == 0);
    }

    SECTION("Marker bit should not be set") {
        REQUIRE(packet.marker_bit() == false);
    }

    SECTION("Payload type should be 98 (L24)") {
        REQUIRE(packet.payload_type() == 98);
    }

    SECTION("Sequence number should be 43981") {
        REQUIRE(packet.sequence_number() == 43981);
    }

    SECTION("Timestamp should be 2882400001") {
        REQUIRE(packet.timestamp() == 2882400001);
    }

    SECTION("SSRC should be 16909060") {
        REQUIRE(packet.ssrc() == 16909060);
    }

    SECTION("A version higher than should not pass validation") {
        data[0] = 0b11000000;
        REQUIRE_FALSE(packet.validate());
    }

    SECTION("Header to string should result in this string") {
        REQUIRE(
            packet.to_string()
            == "RTP Header: valid=true version=2 padding=false extension=false csrc_count=0 market_bit=false payload_type=98 sequence_number=43981 timestamp=2882400001 ssrc=16909060 payload_start_index=12"
        );
    }
}

TEST_CASE("rtp_packet_view::validate()", "[rtp_packet_view]") {
    rav::rtp::PacketView packet(nullptr, 0);

    SECTION("Validation should fail when the packet is too short") {
        REQUIRE_FALSE(packet.validate());
    }

    SECTION("Version should be 0") {
        REQUIRE(packet.version() == 0);
    }

    SECTION("There should be no padding") {
        REQUIRE(packet.padding() == false);
    }

    SECTION("Extension should be false") {
        REQUIRE(packet.extension() == false);
    }

    SECTION("CSRC Count should be 0") {
        REQUIRE(packet.csrc_count() == 0);
    }

    SECTION("Marker bit should not be set") {
        REQUIRE(packet.marker_bit() == false);
    }

    SECTION("Payload type should be 0") {
        REQUIRE(packet.payload_type() == 0);
    }

    SECTION("Sequence number should be 0") {
        REQUIRE(packet.sequence_number() == 0);
    }

    SECTION("Timestamp should be 0") {
        REQUIRE(packet.timestamp() == 0);
    }

    SECTION("SSRC should be 0") {
        REQUIRE(packet.ssrc() == 0);
    }

    SECTION("CSRC 1 (which does not exist) should be 0") {
        REQUIRE(packet.csrc(0) == 0);
    }

    SECTION("The buffer view should be invalid") {
        REQUIRE(packet.payload_data().data() == nullptr);
    }
}

TEST_CASE("rtp_packet_view::ssrc()", "[rtp_packet_view]") {
    const uint8_t data[] = {
        // v, p, x, cc
        0b10000010,
        // m, pt
        0b01100001,
        // sequence number
        0xab,
        0xcd,
        // timestamp
        0xab,
        0xcd,
        0xef,
        0x01,
        // ssrc
        0x01,
        0x02,
        0x03,
        0x04,
        // csrc 1
        0x05,
        0x06,
        0x07,
        0x08,
        // csrc 2
        0x09,
        0x10,
        0x11,
        0x12,
    };

    const rav::rtp::PacketView packet(data, sizeof(data) - sizeof(uint32_t) * 2);

    SECTION("Should not pass validation") {
        REQUIRE_FALSE(packet.validate());
    }

    SECTION("CSRC Count should be 2") {
        REQUIRE(packet.csrc_count() == 2);
    }

    SECTION("CSRC 1 should be 84281096") {
        REQUIRE(packet.csrc(0) == 84281096);
    }

    SECTION("CSRC 2 should be 152047890") {
        REQUIRE(packet.csrc(1) == 152047890);
    }

    SECTION("CSRC 3 (which does not exist) should be 0") {
        REQUIRE(packet.csrc(2) == 0);
    }
}

TEST_CASE("rtp_packet_view::get_header_extension_data()", "[rtp_packet_view]") {
    SECTION("Test header extension with csrc and extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010010,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // csrc 1
            0x05,
            0x06,
            0x07,
            0x08,
            // csrc 2
            0x09,
            0x10,
            0x11,
            0x12,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));

        const auto header_extension_data = packet.get_header_extension_data();
        REQUIRE(header_extension_data.size_bytes() == 8);
        REQUIRE(packet.get_header_extension_defined_by_profile() == 513);
        REQUIRE(header_extension_data.data() == data + 24);
        REQUIRE(std::memcmp(data + 24, header_extension_data.data(), 8) == 0);
    }

    SECTION("Test header extension without csrc and with extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));

        const auto header_extension_data = packet.get_header_extension_data();
        REQUIRE(header_extension_data.size_bytes() == 8);
        REQUIRE(packet.get_header_extension_defined_by_profile() == 513);
        REQUIRE(header_extension_data.data() == data + 16);
        REQUIRE(std::memcmp(header_extension_data.data(), data + 16, 8) == 0);
    }

    SECTION("Test header extension without csrc and without extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10000000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));

        const auto header_extension_data = packet.get_header_extension_data();
        REQUIRE(header_extension_data.size_bytes() == 0);
        REQUIRE(packet.get_header_extension_defined_by_profile() == 0);
        REQUIRE(header_extension_data.data() == nullptr);
    }
}

TEST_CASE("rtp_packet_view::header_total_length()", "[rtp_packet_view]") {
    SECTION("Test payload start index without csrc and without extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10000000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        REQUIRE(packet.header_total_length() == 12);
    }

    SECTION("Test payload start index without csrc and with extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        REQUIRE(packet.header_total_length() == 24);
    }

    SECTION("Test payload start index with csrc and extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010010,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // csrc 1
            0x05,
            0x06,
            0x07,
            0x08,
            // csrc 2
            0x09,
            0x10,
            0x11,
            0x12,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        REQUIRE(packet.header_total_length() == 32);
    }
}

TEST_CASE("rtp_packet_view::payload_data()", "[rtp_packet_view]") {
    SECTION("Test getting payload without csrc and without extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10000000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // payload data
            0x11,
            0x22,
            0x33,
            0x44,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        auto payload = packet.payload_data();
        REQUIRE(payload.size() == 4);
        REQUIRE(payload.size() == payload.size_bytes());
        REQUIRE(payload.data() == data + 12);
        REQUIRE(payload.data()[0] == 0x11);
        REQUIRE(payload.data()[1] == 0x22);
        REQUIRE(payload.data()[2] == 0x33);
        REQUIRE(payload.data()[3] == 0x44);
    }

    SECTION("Test getting payload without csrc and with extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010000,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
            // payload data
            0x11,
            0x22,
            0x33,
            0x44,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        auto payload = packet.payload_data();
        REQUIRE(payload.size() == 4);
        REQUIRE(payload.size() == payload.size_bytes());
        REQUIRE(payload.data() == data + 24);
        REQUIRE(payload.data()[0] == 0x11);
        REQUIRE(payload.data()[1] == 0x22);
        REQUIRE(payload.data()[2] == 0x33);
        REQUIRE(payload.data()[3] == 0x44);
    }

    SECTION("Test getting payload with csrc and extension") {
        const uint8_t data[] = {
            // v, p, x, cc
            0b10010010,
            // m, pt
            0b01100001,
            // sequence number
            0xab,
            0xcd,
            // timestamp
            0xab,
            0xcd,
            0xef,
            0x01,
            // ssrc
            0x01,
            0x02,
            0x03,
            0x04,
            // csrc 1
            0x05,
            0x06,
            0x07,
            0x08,
            // csrc 2
            0x09,
            0x10,
            0x11,
            0x12,
            // extension header defined by profile
            0x01,
            0x02,
            // extension header length (number of 32-bit words)
            0x00,
            0x02,
            // extension header data
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
            // payload data
            0x11,
            0x22,
            0x33,
            0x44,
        };

        const rav::rtp::PacketView packet(data, sizeof(data));
        auto payload = packet.payload_data();
        REQUIRE(payload.size() == 4);
        REQUIRE(payload.size() == payload.size_bytes());
        REQUIRE(payload.data() == data + 32);
        REQUIRE(payload.data()[0] == 0x11);
        REQUIRE(payload.data()[1] == 0x22);
        REQUIRE(payload.data()[2] == 0x33);
        REQUIRE(payload.data()[3] == 0x44);
    }

    SECTION("Getting payload from an invalid packet should return an invalid buffer view") {
        std::array<uint8_t, 28> data {
            0x90, 0x61, 0xab, 0xcd,  // v, p, x, cc | m, pt | sequence number
            0xab, 0xcd, 0xef, 0x01,  // timestamp
            0x01, 0x02, 0x03, 0x04,  // ssrc
            0x11, 0x22, 0x33, 0x44   // payload data
        };

        const rav::rtp::PacketView packet(data.data(), data.size() - 1);
        auto payload = packet.payload_data();
        REQUIRE(payload.data() == nullptr);
        REQUIRE(payload.empty());
    }
}
