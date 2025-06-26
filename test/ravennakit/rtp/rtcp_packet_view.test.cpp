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

#include "ravennakit/rtp/rtcp_packet_view.hpp"

TEST_CASE("rav::rtcp::PacketView") {
    SECTION("rtcp_packet_view::validate()", "[rtcp_packet_view]") {
        std::array<uint8_t, 28> data {
            // Header
            0x82, 0xc8, 0xc8, 0x14,  // v, p, rc | packet type | length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
        };

        SECTION("Validation should fail when the view doesn't point to data") {
            const rav::rtcp::PacketView packet(nullptr, data.size());
            REQUIRE_FALSE(packet.validate());
        }

        SECTION("Validation should fail when passing an invalid size") {
            const rav::rtcp::PacketView packet(data.data(), 0);
            REQUIRE_FALSE(packet.validate());
        }

        const rav::rtcp::PacketView packet(data.data(), data.size());

        SECTION("At this point validation should pass") {
            REQUIRE(packet.validate());
        }

        SECTION("Validation should fail when the version is not 2") {
            data[0] = 0;
            REQUIRE(packet.version() == 0);
            REQUIRE_FALSE(packet.validate());
        }

        SECTION("Validation should fail when there is no room for sender info") {
            const rav::rtcp::PacketView short_packet(data.data(), data.size() - 1);
            REQUIRE_FALSE(short_packet.validate());
        }
    }

    SECTION("rtcp_packet_view::version()", "[rtcp_packet_view]") {
        uint8_t data[] = {0b00'0'10101};

        const rav::rtcp::PacketView packet(data, sizeof(data));

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

        SECTION("Version should be zero on a packet of size zero") {
            const rav::rtcp::PacketView zero_packet(data, 0);
            REQUIRE(zero_packet.version() == 0);
        }
    }

    SECTION("rtcp_packet_view::padding()", "[rtcp_packet_view]") {
        uint8_t data[] = {0b11'0'11111};

        const rav::rtcp::PacketView packet(data, sizeof(data));

        SECTION("No padding") {
            REQUIRE(packet.padding() == false);
        }

        SECTION("With padding") {
            data[0] = 0b11111111;
            REQUIRE(packet.padding() == true);
        }

        SECTION("Padding should be false on a packet of size zero") {
            const rav::rtcp::PacketView zero_packet(data, 0);
            REQUIRE(zero_packet.padding() == false);
        }
    }

    SECTION("rtcp_packet_view::reception_report_count()", "[rtcp_packet_view]") {
        uint8_t data[] = {0b11'1'00000};
        const rav::rtcp::PacketView packet(data, sizeof(data));

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

        SECTION("Reception report count should be false on a packet of size zero") {
            const rav::rtcp::PacketView zero_packet(data, 0);
            REQUIRE(zero_packet.reception_report_count() == 0);
        }
    }

    SECTION("rtcp_packet_view::packet_type()", "[rtcp_packet_view]") {
        uint8_t data[] = {0b11111111, 0};
        const rav::rtcp::PacketView packet(data, sizeof(data));

        REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::unknown);

        SECTION("Sender report") {
            data[1] = 200;
            REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::sender_report_report);
        }

        SECTION("Receiver report") {
            data[1] = 201;
            REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::receiver_report_report);
        }

        SECTION("Source description items") {
            data[1] = 202;
            REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::source_description_items_items);
        }

        SECTION("Bye") {
            data[1] = 203;
            REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::bye);
        }

        SECTION("App specific") {
            data[1] = 204;
            REQUIRE(packet.type() == rav::rtcp::PacketView::PacketType::app);
        }

        SECTION("Packet type should be unknown when the data is too small") {
            const rav::rtcp::PacketView zero_packet(data, 1);
            REQUIRE(zero_packet.type() == rav::rtcp::PacketView::PacketType::unknown);
        }
    }

    SECTION("rtcp_packet_view::length()", "[rtcp_packet_view]") {
        uint8_t data[] = {0xff, 0xff, 0xab, 0xcd};
        const rav::rtcp::PacketView packet(data, sizeof(data));

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

        SECTION("Length should be 0 on a too small packet") {
            const rav::rtcp::PacketView zero_packet(data, 2);
            REQUIRE(zero_packet.length() == 0);
        }
    }

    SECTION("rtcp_packet_view::ssrc()", "[rtcp_packet_view]") {
        uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0x01, 0x02, 0x03, 0x04};
        const rav::rtcp::PacketView packet(data, sizeof(data));

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

        SECTION("SSRC should be 0 on a too small packet") {
            const rav::rtcp::PacketView zero_packet(data, 4);
            REQUIRE(zero_packet.ssrc() == 0);
        }
    }

    SECTION("rtcp_packet_view::to_string()", "[rtcp_packet_view]") {
        std::array<uint8_t, 168> data {
            // Header
            0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
        };

        const rav::rtcp::PacketView packet(data.data(), data.size());
        std::ignore = packet.to_string();
        data[1] = 201;
        std::ignore = packet.to_string();
    }

    SECTION("rtcp_packet_view::ntp_timestamp()", "[rtcp_packet_view]") {
        std::array<uint8_t, 16> data = {

            // v, p, rc
            0b10'0'10101,
            // packet type
            200,
            // length
            0xab, 0xCD,
            // csrc
            0x01, 0x02, 0x03, 0x04,
            // NTP MSW
            0x01, 0x02, 0x03, 0x04,
            /// NTP LSW
            0x05, 0x06, 0x07, 0x08
        };

        const rav::rtcp::PacketView packet(data.data(), data.size());

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

        SECTION("Timestamp should be 0 on a too small packet") {
            const rav::rtcp::PacketView zero_packet(data.data(), data.size() - 1);
            REQUIRE(zero_packet.ntp_timestamp() == rav::ntp::Timestamp {});
        }
    }

    SECTION("rtcp_packet_view::rtp_timestamp()", "[rtcp_packet_view]") {
        uint8_t data[] = {// v, p, rc
            0b10'0'10101,
            // packet type
            200,
            // length
            0x02, 0x03,
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
            const rav::rtcp::PacketView packet(data, sizeof(data) - 1);
            REQUIRE(packet.rtp_timestamp() == 0);
        }

        const rav::rtcp::PacketView packet(data, sizeof(data));

        SECTION("Sender report") {
            REQUIRE(packet.rtp_timestamp() == 0x10111213);
        }

        SECTION("Receiver report") {
            data[1] = 201;
            REQUIRE(packet.rtp_timestamp() == 0);
        }
    }

    SECTION("rtcp_packet_view::packet_count()", "[rtcp_packet_view]") {
        uint8_t data[] = {// v, p, rc
            0b10'0'10101,
            // packet type
            200,
            // length
            0x02, 0x03,
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
            const rav::rtcp::PacketView packet(data, sizeof(data) - 1);
            REQUIRE(packet.packet_count() == 0);
        }

        const rav::rtcp::PacketView packet(data, sizeof(data));

        SECTION("Sender report") {
            REQUIRE(packet.packet_count() == 0x14151617);
        }

        SECTION("Receiver report") {
            data[1] = 201;
            REQUIRE(packet.packet_count() == 0);
        }
    }

    SECTION("rtcp_packet_view::octet_count()", "[rtcp_packet_view]") {
        uint8_t data[] = {// v, p, rc
            0b10'0'10101,
            // packet type
            200,
            // length
            0x02, 0x03,
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
            const rav::rtcp::PacketView packet(data, sizeof(data) - 1);
            REQUIRE(packet.octet_count() == 0);
        }

        const rav::rtcp::PacketView packet(data, sizeof(data));

        SECTION("Sender report") {
            REQUIRE(packet.octet_count() == 0x18191a1b);
        }

        SECTION("Receiver report") {
            data[1] = 201;
            REQUIRE(packet.octet_count() == 0);
        }
    }

    SECTION("rtcp_packet_view::data()", "[RtcpReportBlockView]") {
        constexpr std::array<uint8_t, 28> data {
            // Header
            0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
            0x04, 0x05, 0x06, 0x07,  // csrc
            // Sender info
            0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
            0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
            0x10, 0x11, 0x12, 0x13,  // RTP timestamp
            0x14, 0x15, 0x16, 0x17,  // Senders packet count
            0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
        };

        SECTION("Data and size should match the packet above") {
            const rav::rtcp::PacketView view(data.data(), data.size());
            REQUIRE(view.data() == data.data());
            REQUIRE(view.size() == 28);
        }

        SECTION("An empty view should return nullptr and 0") {
            constexpr rav::rtcp::PacketView view;
            REQUIRE(view.data() == nullptr);
            REQUIRE(view.size() == 0);
        }
    }

    SECTION("rtcp_packet_view::get_report_block()", "[rtcp_packet_view]") {
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

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            const auto view = packet_view.get_report_block(0);
            REQUIRE(view.data() == nullptr);
        }

        SECTION("A packet with report count but without the data should return an invalid report view") {
            constexpr std::array<uint8_t, 28> packet {
                // Header
                0x81, 0xc8, 0x02, 0x03,  // v, p, rc, packet type, length
                0x04, 0x05, 0x06, 0x07,  // csrc
                // Sender info
                0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
                0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
                0x10, 0x11, 0x12, 0x13,  // RTP timestamp
                0x14, 0x15, 0x16, 0x17,  // Senders packet count
                0x18, 0x19, 0x1a, 0x1b   // Senders octet count
            };

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            const auto view = packet_view.get_report_block(0);
            REQUIRE(view.data() == nullptr);
        }

        SECTION("A packet with report count and with the data should return a valid report view") {
            constexpr std::array<uint8_t, 52> packet {
                // Header
                0x81, 0xc8, 0x02, 0x03,  // v, p, rc | packet type | length
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

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            const auto view = packet_view.get_report_block(0);
            REQUIRE(view.data() != nullptr);
            REQUIRE(view.ssrc() == 0x01020304);
            REQUIRE(view.fraction_lost() == 0x05);
            REQUIRE(view.number_of_packets_lost() == 0x060708);
            REQUIRE(view.extended_highest_sequence_number_received() == 0x090a0b0c);
            REQUIRE(view.inter_arrival_jitter() == 0x0d0e0f10);
            REQUIRE(view.last_sr_timestamp().integer() == 0x1112);
            REQUIRE(view.last_sr_timestamp().fraction() == 0x13140000);
            REQUIRE(view.delay_since_last_sr() == 0x15161718);
            REQUIRE(view.data() == packet.data() + 28);
            REQUIRE(view.size() == packet.size() - 28);
        }

        SECTION("A packet with report count two and with the data should return a valid report view") {
            constexpr std::array<uint8_t, 76> packet {
                // Header
                0x82, 0xc8, 0x02, 0x03,  // v, p, rc | packet type | length
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

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            const auto report1 = packet_view.get_report_block(0);
            REQUIRE(report1.data() != nullptr);
            REQUIRE(report1.ssrc() == 0x01020304);
            REQUIRE(report1.fraction_lost() == 0x05);
            REQUIRE(report1.number_of_packets_lost() == 0x060708);
            REQUIRE(report1.extended_highest_sequence_number_received() == 0x090a0b0c);
            REQUIRE(report1.inter_arrival_jitter() == 0x0d0e0f10);
            REQUIRE(report1.last_sr_timestamp().integer() == 0x1112);
            REQUIRE(report1.last_sr_timestamp().fraction() == 0x13140000);
            REQUIRE(report1.delay_since_last_sr() == 0x15161718);
            REQUIRE(report1.data() == packet.data() + 28);
            REQUIRE(report1.size() == 24);

            const auto report2 = packet_view.get_report_block(1);
            REQUIRE(report2.data() != nullptr);
            REQUIRE(report2.ssrc() == 0x21222324);
            REQUIRE(report2.fraction_lost() == 0x25);
            REQUIRE(report2.number_of_packets_lost() == 0x262728);
            REQUIRE(report2.extended_highest_sequence_number_received() == 0x292a2b2c);
            REQUIRE(report2.inter_arrival_jitter() == 0x2d2e2f30);
            REQUIRE(report2.last_sr_timestamp().integer() == 0x3132);
            REQUIRE(report2.last_sr_timestamp().fraction() == 0x33340000);
            REQUIRE(report2.delay_since_last_sr() == 0x35363738);
            REQUIRE(report2.data() == packet.data() + 28 + 24);
            REQUIRE(report2.size() == 24);
        }

        SECTION("Get report blocks from a receiver report packet") {
            constexpr std::array<uint8_t, 56> packet {
                // Header
                0x82, 0xc9, 0x02, 0x03,  // v, p, rc | packet type | length
                0x04, 0x05, 0x06, 0x07,  // csrc
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

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            const auto report1 = packet_view.get_report_block(0);
            REQUIRE(report1.data() != nullptr);
            REQUIRE(report1.ssrc() == 0x01020304);
            REQUIRE(report1.fraction_lost() == 0x05);
            REQUIRE(report1.number_of_packets_lost() == 0x060708);
            REQUIRE(report1.extended_highest_sequence_number_received() == 0x090a0b0c);
            REQUIRE(report1.inter_arrival_jitter() == 0x0d0e0f10);
            REQUIRE(report1.last_sr_timestamp().integer() == 0x1112);
            REQUIRE(report1.last_sr_timestamp().fraction() == 0x13140000);
            REQUIRE(report1.delay_since_last_sr() == 0x15161718);
            REQUIRE(report1.data() == packet.data() + 8);
            REQUIRE(report1.size() == 24);

            const auto report2 = packet_view.get_report_block(1);
            REQUIRE(report2.data() != nullptr);
            REQUIRE(report2.ssrc() == 0x21222324);
            REQUIRE(report2.fraction_lost() == 0x25);
            REQUIRE(report2.number_of_packets_lost() == 0x262728);
            REQUIRE(report2.extended_highest_sequence_number_received() == 0x292a2b2c);
            REQUIRE(report2.inter_arrival_jitter() == 0x2d2e2f30);
            REQUIRE(report2.last_sr_timestamp().integer() == 0x3132);
            REQUIRE(report2.last_sr_timestamp().fraction() == 0x33340000);
            REQUIRE(report2.delay_since_last_sr() == 0x35363738);
            REQUIRE(report2.data() == packet.data() + 8 + 24);
            REQUIRE(report2.size() == 24);
        }
    }

    SECTION("rtcp_packet_view::get_profile_specific_extension()", "[rtcp_packet_view]") {
        SECTION("A packet with report count two and with the data should return a valid report view") {
            constexpr std::array<uint8_t, 76> packet {
                // Header
                0x82, 0xc8, 0x00, 0x12,  // v, p, rc | packet type | length
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

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());

            REQUIRE(packet_view.length() == 0x13);

            const auto ext = packet_view.get_profile_specific_extension();
            REQUIRE(ext.data() == nullptr);
            REQUIRE(ext.empty());
        }

        SECTION("A packet with report count two and with the data should return a valid report view") {
            constexpr std::array<uint8_t, 84> packet {
                // Header
                0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
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
                0x35, 0x36, 0x37, 0x38,  // delay since last SR
                // Profile specific extension
                0x41, 0x42, 0x43, 0x44,  // data
                0x45, 0x46, 0x47, 0x48,  // data
            };

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());

            REQUIRE(packet_view.length() == 0x15);

            const auto ext = packet_view.get_profile_specific_extension();
            REQUIRE(ext.data() != nullptr);
            REQUIRE_FALSE(ext.empty());
            REQUIRE(ext.size() == 8);
            REQUIRE(ext.size_bytes() == 8);
            REQUIRE(ext.data()[0] == 0x41);
            REQUIRE(ext.data()[1] == 0x42);
            REQUIRE(ext.data()[2] == 0x43);
            REQUIRE(ext.data()[3] == 0x44);
            REQUIRE(ext.data()[4] == 0x45);
            REQUIRE(ext.data()[5] == 0x46);
            REQUIRE(ext.data()[6] == 0x47);
            REQUIRE(ext.data()[7] == 0x48);
        }

        SECTION("Get an empty buffer view on an empty rtcp_packet_view") {
            rav::rtcp::PacketView view;
            auto ext = view.get_profile_specific_extension();
            REQUIRE(ext.data() == nullptr);
            REQUIRE(ext.empty());
            REQUIRE(ext.size_bytes() == 0);
        }

        SECTION("Check reported length") {
            constexpr std::array<uint8_t, 36> packet {
                // Header
                0x80, 0xc8, 0x00, 0x09,  // v, p, rc | packet type | length
                0x04, 0x05, 0x06, 0x07,  // csrc
                // Sender info
                0x08, 0x09, 0x0a, 0x0b,  // NTP MSW
                0x0c, 0x0d, 0x0e, 0x0f,  // NTP LSW
                0x10, 0x11, 0x12, 0x13,  // RTP timestamp
                0x14, 0x15, 0x16, 0x17,  // Senders packet count
                0x18, 0x19, 0x1a, 0x1b,  // Senders octet count
                // Profile specific extension
                0x41, 0x42, 0x43, 0x44,  // data
                0x45, 0x46, 0x47, 0x48,  // data
            };

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());
            REQUIRE(packet_view.reception_report_count() == 0);
            auto ext = packet_view.get_profile_specific_extension();
            REQUIRE(ext.data() == nullptr);
        }

        SECTION("Get profile specific extension from a receiver report packet") {
            constexpr std::array<uint8_t, 64> packet {
                // Header
                0x82, 0xc9, 0x00, 0x0f,  // v, p, rc | packet type | length
                0x04, 0x05, 0x06, 0x07,  // csrc
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
                0x35, 0x36, 0x37, 0x38,  // delay since last SR
                // Profile specific extension
                0x41, 0x42, 0x43, 0x44,  // data
                0x45, 0x46, 0x47, 0x48,  // data
            };

            const rav::rtcp::PacketView packet_view(packet.data(), packet.size());

            REQUIRE(packet_view.length() == 0x10);

            const auto ext = packet_view.get_profile_specific_extension();
            REQUIRE(ext.data() != nullptr);
            REQUIRE_FALSE(ext.empty());
            REQUIRE(ext.size() == 8);
            REQUIRE(ext.size_bytes() == 8);
            REQUIRE(ext.data()[0] == 0x41);
            REQUIRE(ext.data()[1] == 0x42);
            REQUIRE(ext.data()[2] == 0x43);
            REQUIRE(ext.data()[3] == 0x44);
            REQUIRE(ext.data()[4] == 0x45);
            REQUIRE(ext.data()[5] == 0x46);
            REQUIRE(ext.data()[6] == 0x47);
            REQUIRE(ext.data()[7] == 0x48);
        }
    }

    SECTION("rtcp_packet_view::get_next_packet()", "[rtcp_packet_view]") {
        SECTION("A single packet should not return a valid next packet") {
            constexpr std::array<uint8_t, 84> data {
                // Header
                0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
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
                0x35, 0x36, 0x37, 0x38,  // delay since last SR
                // Profile specific extension
                0x41, 0x42, 0x43, 0x44,  // data
                0x45, 0x46, 0x47, 0x48,  // data
            };

            const rav::rtcp::PacketView packet_view(data.data(), data.size());
            const auto packet_view2 = packet_view.get_next_packet();
            REQUIRE(packet_view2.data() == nullptr);
        }

        SECTION("A single packet should not return a valid next packet") {
            constexpr std::array<uint8_t, 168> data {
                // Header
                0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
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
                0x35, 0x36, 0x37, 0x38,  // delay since last SR
                // Profile specific extension
                0x41, 0x42, 0x43, 0x44,  // data
                0x45, 0x46, 0x47, 0x48,  // data

                // Header
                0x82, 0xc8, 0x00, 0x14,  // v, p, rc | packet type | length
                0x58, 0x59, 0x5a, 0x5b,  // csrc
                // Sender info
                0x5c, 0x5d, 0x5e, 0x5f,  // NTP MSW
                0x60, 0x61, 0x62, 0x63,  // NTP LSW
                0x64, 0x65, 0x66, 0x67,  // RTP timestamp
                0x68, 0x69, 0x6a, 0x6b,  // Senders packet count
                0x6c, 0x6d, 0x6e, 0x6f,  // Senders octet count
                // Report block 1
                0x55, 0x56, 0x57, 0x58,  // SSRC
                0x59, 0x5a, 0x5b, 0x5c,  // fraction lost | cumulative number of packets lost
                0x5d, 0x5e, 0x5f, 0x60,  // extended highest sequence number received
                0x61, 0x62, 0x63, 0x64,  // inter-arrival jitter
                0x65, 0x66, 0x67, 0x68,  // last SR timestamp
                0x69, 0x6a, 0x6b, 0x6c,  // delay since last SR
                // Report block 2
                0x75, 0x76, 0x77, 0x78,  // SSRC
                0x79, 0x7a, 0x7b, 0x7c,  // fraction lost | cumulative number of packets lost
                0x7d, 0x7e, 0x7f, 0x80,  // extended highest sequence number received
                0x81, 0x82, 0x83, 0x84,  // inter-arrival jitter
                0x85, 0x86, 0x87, 0x88,  // last SR timestamp
                0x89, 0x8a, 0x8b, 0x8c,  // delay since last SR
                // Profile specific extension
                0x95, 0x96, 0x97, 0x98,  // data
                0x99, 0x9a, 0x9b, 0x9c,  // data
            };

            const rav::rtcp::PacketView packet_view(data.data(), data.size());
            const auto packet_view2 = packet_view.get_next_packet();
            REQUIRE(packet_view2.data() != nullptr);
            REQUIRE(packet_view2.data() == data.data() + 84);
            REQUIRE(packet_view2.size() == 84);

            auto ext = packet_view2.get_profile_specific_extension();
            REQUIRE(ext.data() == data.data() + 160);
            REQUIRE(ext.size() == 8);
            REQUIRE(ext.size_bytes() == 8);
            REQUIRE(ext.data()[0] == 0x95);
            REQUIRE(ext.data()[7] == 0x9c);
        }

        SECTION("Getting a next packet from an invalid packet should not lead to undefined behavior") {
            rav::rtcp::PacketView invalid;
            auto next_packet = invalid.get_next_packet();
            REQUIRE(next_packet.data() == nullptr);
        }
    }

    SECTION("rtcp_packet_view::packet_type_to_string()") {
        auto expect = [](const rav::rtcp::PacketView::PacketType packet_type, const char* str) {
            return std::strcmp(rav::rtcp::PacketView::packet_type_to_string(packet_type), str) == 0;
        };

        REQUIRE(expect(rav::rtcp::PacketView::PacketType::source_description_items_items, "SourceDescriptionItems"));
        REQUIRE(expect(rav::rtcp::PacketView::PacketType::sender_report_report, "SenderReport"));
        REQUIRE(expect(rav::rtcp::PacketView::PacketType::receiver_report_report, "ReceiverReport"));
        REQUIRE(expect(rav::rtcp::PacketView::PacketType::unknown, "Unknown"));
        REQUIRE(expect(rav::rtcp::PacketView::PacketType::bye, "Bye"));
        REQUIRE(expect(rav::rtcp::PacketView::PacketType::app, "App"));

        // Force an invalid packet type to pass the default branch
        auto max = std::numeric_limits<std::underlying_type_t<rav::rtcp::PacketView::PacketType>>::max();
        REQUIRE(expect(static_cast<rav::rtcp::PacketView::PacketType>(max), ""));
    }
}
