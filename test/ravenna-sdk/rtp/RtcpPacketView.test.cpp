/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravenna-sdk/rtp/RtcpPacketView.hpp"

#include <asio/detail/socket_ops.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("RtcpPacketView | verify()", "[RtcpPacketView]") {
    uint8_t data[] = {
        0b10'0'10101,                   // v, p, rc
        201,          0, 0, 0, 0, 0, 0  // packet type
    };

    SECTION("Invalid pointer") {
        const rav::RtcpPacketView packet(nullptr, sizeof(data));
        REQUIRE(packet.verify() == rav::rtp::Result::InvalidPointer);
    }

    SECTION("Invalid header length") {
        const rav::RtcpPacketView packet(data, 0);
        REQUIRE(packet.verify() == rav::rtp::Result::InvalidHeaderLength);
    }

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Ok") {
        REQUIRE(packet.verify() == rav::rtp::Result::Ok);
    }

    SECTION("Valid version 0") {
        data[0] = 0;
        REQUIRE(packet.version() == 0);
        REQUIRE(packet.verify() == rav::rtp::Result::Ok);
    }

    SECTION("Valid version 1") {
        data[0] = 0b01000000;
        REQUIRE(packet.version() == 1);
        REQUIRE(packet.verify() == rav::rtp::Result::Ok);
    }

    SECTION("Valid version 2") {
        data[0] = 0b10000000;
        REQUIRE(packet.version() == 2);
        REQUIRE(packet.verify() == rav::rtp::Result::Ok);
    }

    SECTION("Invalid version 3") {
        data[0] = 0b11000000;
        REQUIRE(packet.version() == 3);
        REQUIRE(packet.verify() == rav::rtp::Result::InvalidVersion);
    }
}

TEST_CASE("RtcpPacketView | version()", "[RtcpPacketView]") {
    uint8_t data[] = {0b00'0'10101};

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Version 0") {
        REQUIRE(packet.version() == 0);
    }

    SECTION("Version 1") {
        data[0] = 0b01111111;
        REQUIRE(packet.version() == 1);
    }

    SECTION("Version 2") {
        data[0] = 0b10111111;
        REQUIRE(packet.version() == 2);
    }

    SECTION("Version 3") {
        data[0] = 0b11111111;
        REQUIRE(packet.version() == 3);
    }
}

TEST_CASE("RtcpPacketView | padding()", "[RtcpPacketView]") {
    uint8_t data[] = {0b11'0'11111};

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("No padding") {
        REQUIRE(packet.padding() == false);
    }

    SECTION("With padding") {
        data[0] = 0b11111111;
        REQUIRE(packet.padding() == true);
    }
}

TEST_CASE("RtcpPacketView | reception_report_count()", "[RtcpPacketView]") {
    uint8_t data[] = {0b11'1'00000};
    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Count 0") {
        REQUIRE(packet.reception_report_count() == 0);
    }

    SECTION("Count 15") {
        data[0] = {0b11'1'10101};
        REQUIRE(packet.reception_report_count() == 0b10101);
    }

    SECTION("Count 31 (max value)") {
        data[0] = {0b11'1'11111};
        REQUIRE(packet.reception_report_count() == 0b11111);
    }
}

TEST_CASE("RtcpPacketView | packet_type()", "[RtcpPacketView]") {
    uint8_t data[] = {0b11111111, 0};
    const rav::RtcpPacketView packet(data, sizeof(data));

    REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::Unknown);

    SECTION("Sender report") {
        data[1] = 200;
        REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::SenderReport);
    }

    SECTION("Receiver report") {
        data[1] = 201;
        REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::ReceiverReport);
    }

    SECTION("Source description items") {
        data[1] = 202;
        REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::SourceDescriptionItems);
    }

    SECTION("Bye") {
        data[1] = 203;
        REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::Bye);
    }

    SECTION("App specific") {
        data[1] = 204;
        REQUIRE(packet.packet_type() == rav::RtcpPacketView::PacketType::App);
    }
}

TEST_CASE("RtcpPacketView | length()", "[RtcpPacketView]") {
    uint8_t data[] = {0xff, 0xff, 0xab, 0xcd};
    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("0xABCD") {
        REQUIRE(packet.length() == 0xabce);  // Length is encoded minus one
    }

    SECTION("0x0") {
        data[2] = 0;
        data[3] = 0;
        REQUIRE(packet.length() == 0x1);  // Length is encoded minus one
    }

    SECTION("0xffff") {
        data[2] = 0xff;
        data[3] = 0xFE;
        REQUIRE(packet.length() == 0xffff);
    }
}

TEST_CASE("RtcpPacketView | ssrc()", "[RtcpPacketView]") {
    uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0x01, 0x02, 0x03, 0x04};
    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("0x01020304") {
        REQUIRE(packet.ssrc() == 0x01020304);
    }

    SECTION("0x0") {
        data[4] = 0;
        data[5] = 0;
        data[6] = 0;
        data[7] = 0;
        REQUIRE(packet.ssrc() == 0x0);
    }

    SECTION("0xffff") {
        data[4] = 0xff;
        data[5] = 0xff;
        data[6] = 0xff;
        data[7] = 0xff;
        REQUIRE(packet.ssrc() == 0xffffffff);
    }
}

TEST_CASE("RtcpPacketView | to_string()", "[RtcpPacketView]") {
    uint8_t data[] = {

        // v, p, rc
        0b10'0'10101,
        // packet type
        201,
        // length
        0x02, 0X03,
        // csrc
        0x04, 0x05, 0x06, 0x07
    };

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Test to_string method") {
        REQUIRE(
            packet.to_string()
            == "RTCP Packet: valid=true version=2 padding=false reception_report_count=21 packet_type=ReceiverReport length=516 ssrc=67438087"
        );
    }
}

TEST_CASE("RtcpPacketView | ntp_timestamp()", "[RtcpPacketView]") {
    uint8_t data[] = {

        // v, p, rc
        0b10'0'10101,
        // packet type
        200,
        // length
        0xab, 0XCD,
        // csrc
        0x01, 0x02, 0x03, 0x04,
        // NTP MSW
        0x01, 0x02, 0x03, 0x04,
        /// NTP LSW
        0x05, 0x06, 0x07, 0x08
    };

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Sender report") {
        const auto ts = packet.ntp_timestamp();
        REQUIRE(ts.integer() == 16909060);
        REQUIRE(ts.fraction() == 84281096);
    }

    SECTION("Receiver report") {
        data[1] = 201;
        const auto ts = packet.ntp_timestamp();
        REQUIRE(ts.integer() == 0);
        REQUIRE(ts.fraction() == 0);
    }
}

TEST_CASE("RtcpPacketView | rtp_timestamp()", "[RtcpPacketView]") {
    uint8_t data[] = {// v, p, rc
                      0b10'0'10101,
                      // packet type
                      200,
                      // length
                      0x02, 0X03,
                      // csrc
                      0x04, 0x05, 0x06, 0x07,
                      // NTP MSW
                      0x08, 0x09, 0x0a, 0x0b,
                      // NTP LSW
                      0x0c, 0x0d, 0x0e, 0x0f,
                      // RTP timestamp
                      0x10, 0x11, 0x12, 0x13
    };

    SECTION("Sender report with too little data") {
        const rav::RtcpPacketView packet(data, sizeof(data) - 1);
        REQUIRE(packet.rtp_timestamp() == 0);
    }

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Sender report") {
        REQUIRE(packet.rtp_timestamp() == 0x10111213);
    }

    SECTION("Receiver report") {
        data[1] = 201;
        REQUIRE(packet.rtp_timestamp() == 0);
    }
}

TEST_CASE("RtcpPacketView | packet_count()", "[RtcpPacketView]") {
    uint8_t data[] = {// v, p, rc
                      0b10'0'10101,
                      // packet type
                      200,
                      // length
                      0x02, 0X03,
                      // csrc
                      0x04, 0x05, 0x06, 0x07,
                      // NTP MSW
                      0x08, 0x09, 0x0a, 0x0b,
                      // NTP LSW
                      0x0c, 0x0d, 0x0e, 0x0f,
                      // RTP timestamp
                      0x10, 0x11, 0x12, 0x13,
                      // Senders packet count
                      0x14, 0x15, 0x16, 0x17
    };

    SECTION("Sender report with too little data") {
        const rav::RtcpPacketView packet(data, sizeof(data) - 1);
        REQUIRE(packet.packet_count() == 0);
    }

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Sender report") {
        REQUIRE(packet.packet_count() == 0x14151617);
    }

    SECTION("Receiver report") {
        data[1] = 201;
        REQUIRE(packet.packet_count() == 0);
    }
}

TEST_CASE("RtcpPacketView | octet_count()", "[RtcpPacketView]") {
    uint8_t data[] = {// v, p, rc
                      0b10'0'10101,
                      // packet type
                      200,
                      // length
                      0x02, 0X03,
                      // csrc
                      0x04, 0x05, 0x06, 0x07,
                      // NTP MSW
                      0x08, 0x09, 0x0a, 0x0b,
                      // NTP LSW
                      0x0c, 0x0d, 0x0e, 0x0f,
                      // RTP timestamp
                      0x10, 0x11, 0x12, 0x13,
                      // Senders packet count
                      0x14, 0x15, 0x16, 0x17,
                      // Senders octet count
                      0x18, 0x19, 0x1a, 0x1b
    };

    SECTION("Sender report with too little data") {
        const rav::RtcpPacketView packet(data, sizeof(data) - 1);
        REQUIRE(packet.octet_count() == 0);
    }

    const rav::RtcpPacketView packet(data, sizeof(data));

    SECTION("Sender report") {
        REQUIRE(packet.octet_count() == 0x18191a1b);
    }

    SECTION("Receiver report") {
        data[1] = 201;
        REQUIRE(packet.octet_count() == 0);
    }
}

TEST_CASE("RtcpPacketView | get_report_block()", "[RtcpPacketView]") {
    SECTION("A packet without report block should return an invalid report view") {
        constexpr std::array<uint8_t, 28> packet {
            0x80, 0xc8, 0x02, 0x03,  // v (2), p (false), rc (0), packet type (200), length (515)
            0x04, 0x05, 0x06, 0x07,  // csrc
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b   // Senders octet count
        };

        const rav::RtcpPacketView packet_view(packet.data(), packet.size());
        const auto view = packet_view.get_report_block(0);
        REQUIRE(view.is_valid() == false);
    }

    SECTION("A packet with report count but without the data should return an invalid report view") {
        constexpr std::array<uint8_t, 28> packet {
            // Header
            0x81, 0xc8, 0x02, 0X03,  // v, p, rc, packet type, length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b   // Senders octet count
        };

        const rav::RtcpPacketView packet_view(packet.data(), packet.size());
        const auto view = packet_view.get_report_block(0);
        REQUIRE(view.is_valid() == false);
    }

    SECTION("A packet with report count and with the data should return a valid report view") {
        constexpr std::array<uint8_t, 52> packet {
            // Header
            0x81, 0xc8, 0x02, 0X03,  // v, p, rc | packet type | length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
            // Report block 1
            0x01, 0x02, 0x03, 0x04,  // SSRC
            0x05, 0x06, 0x07, 0x08,  // fraction lost | cumulative number of packets lost
            0x09, 0x0a, 0x0b, 0x0c,  // extended highest sequence number received
            0x0d, 0x0e, 0x0f, 0x10,  // inter-arrival jitter
            0x11, 0x12, 0x13, 0x14,  // last SR timestamp
            0x15, 0x16, 0x17, 0x18   // delay since last SR
        };

        const rav::RtcpPacketView packet_view(packet.data(), packet.size());
        const auto view = packet_view.get_report_block(0);
        REQUIRE(view.is_valid() == true);
        REQUIRE(view.ssrc() == 0x01020304);
        REQUIRE(view.fraction_lost() == 0x05);
        REQUIRE(view.number_of_packets_lost() == 0x060708);
        REQUIRE(view.extended_highest_sequence_number_received() == 0x090a0b0c);
        REQUIRE(view.inter_arrival_jitter() == 0x0d0e0f10);
        REQUIRE(view.last_sr_timestamp().integer() == 0x1112);
        REQUIRE(view.last_sr_timestamp().fraction() == 0x13140000);
        REQUIRE(view.delay_since_last_sr() == 0x15161718);
        REQUIRE(view.data() == packet.data() + 28);
        REQUIRE(view.data_length() == packet.size() - 28);
    }

    SECTION("A packet with report count two and with the data should return a valid report view") {
        constexpr std::array<uint8_t, 76> packet {
            // Header
            0x82, 0xc8, 0x02, 0X03,  // v, p, rc | packet type | length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
            // Report block 1
            0x01, 0x02, 0x03, 0x04,  // SSRC
            0x05, 0x06, 0x07, 0x08,  // fraction lost | cumulative number of packets lost
            0x09, 0x0a, 0x0b, 0x0c,  // extended highest sequence number received
            0x0d, 0x0e, 0x0f, 0x10,  // inter-arrival jitter
            0x11, 0x12, 0x13, 0x14,  // last SR timestamp
            0x15, 0x16, 0x17, 0x18,  // delay since last SR
            // Report block 2
            0x21, 0x22, 0x23, 0x24,  // SSRC
            0x25, 0x26, 0x27, 0x28,  // fraction lost | cumulative number of packets lost
            0x29, 0x2a, 0x2b, 0x2c,  // extended highest sequence number received
            0x2d, 0x2e, 0x2f, 0x30,  // inter-arrival jitter
            0x31, 0x32, 0x33, 0x34,  // last SR timestamp
            0x35, 0x36, 0x37, 0x38   // delay since last SR
        };

        const rav::RtcpPacketView packet_view(packet.data(), packet.size());
        const auto report1 = packet_view.get_report_block(0);
        REQUIRE(report1.is_valid() == true);
        REQUIRE(report1.ssrc() == 0x01020304);
        REQUIRE(report1.fraction_lost() == 0x05);
        REQUIRE(report1.number_of_packets_lost() == 0x060708);
        REQUIRE(report1.extended_highest_sequence_number_received() == 0x090a0b0c);
        REQUIRE(report1.inter_arrival_jitter() == 0x0d0e0f10);
        REQUIRE(report1.last_sr_timestamp().integer() == 0x1112);
        REQUIRE(report1.last_sr_timestamp().fraction() == 0x13140000);
        REQUIRE(report1.delay_since_last_sr() == 0x15161718);
        REQUIRE(report1.data() == packet.data() + 28);
        REQUIRE(report1.data_length() == 24);

        const auto report2 = packet_view.get_report_block(1);
        REQUIRE(report2.is_valid() == true);
        REQUIRE(report2.ssrc() == 0x21222324);
        REQUIRE(report2.fraction_lost() == 0x25);
        REQUIRE(report2.number_of_packets_lost() == 0x262728);
        REQUIRE(report2.extended_highest_sequence_number_received() == 0x292a2b2c);
        REQUIRE(report2.inter_arrival_jitter() == 0x2d2e2f30);
        REQUIRE(report2.last_sr_timestamp().integer() == 0x3132);
        REQUIRE(report2.last_sr_timestamp().fraction() == 0x33340000);
        REQUIRE(report2.delay_since_last_sr() == 0x35363738);
        REQUIRE(report2.data() == packet.data() + 28 + 24);
        REQUIRE(report2.data_length() == 24);
    }
}

TEST_CASE("RtcpPacketView | packet_type_to_string()") {
    auto expect = [](const rav::RtcpPacketView::PacketType packet_type, const char* str) {
        return std::strcmp(rav::RtcpPacketView::packet_type_to_string(packet_type), str) == 0;
    };

    REQUIRE(expect(rav::RtcpPacketView::PacketType::SourceDescriptionItems, "SourceDescriptionItems"));
    REQUIRE(expect(rav::RtcpPacketView::PacketType::SenderReport, "SenderReport"));
    REQUIRE(expect(rav::RtcpPacketView::PacketType::ReceiverReport, "ReceiverReport"));
    REQUIRE(expect(rav::RtcpPacketView::PacketType::Unknown, "Unknown"));
    REQUIRE(expect(rav::RtcpPacketView::PacketType::Bye, "Bye"));
    REQUIRE(expect(rav::RtcpPacketView::PacketType::App, "App"));
}
