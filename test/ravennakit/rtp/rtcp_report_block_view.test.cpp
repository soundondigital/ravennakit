/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <ravennakit/rtp/rtcp_report_block_view.hpp>

#include <catch2/catch_all.hpp>

namespace {
std::array<uint8_t, 24> default_packet {
    0x00, 0x01, 0x02, 0x03,  // SSRC
    0x04, 0x05, 0x06, 0x07,  // Fraction lost | cumulative number of packets lost
    0x08, 0x09, 0x0a, 0x0b,  // Extended highest sequence number received
    0x0c, 0x0d, 0x0e, 0x0f,  // Inter arrival jitter
    0x10, 0x11, 0x12, 0x13,  // Last SR (LSR)
    0x14, 0x15, 0x16, 0x17,  // Delay since last SR (DLSR)
};
}

TEST_CASE("rtcp_report_block_view::validate()", "[rtcp_report_block_view]") {
    SECTION("Validation should fail when the view doesn't point to data") {
        const rav::rtcp_report_block_view report(nullptr, 0);
        REQUIRE_FALSE(report.validate());
    }

    SECTION("Validation should fail when the packet is too short") {
        const rav::rtcp_report_block_view report(default_packet.data(), 23);
        REQUIRE_FALSE(report.validate());
    }

    SECTION("Validation should fail when the packet is too long") {
        const rav::rtcp_report_block_view report(default_packet.data(), 25);
        REQUIRE_FALSE(report.validate());
    }

    SECTION("Or else validation should pass") {
        const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
        REQUIRE(report.validate());
    }
}

TEST_CASE("rtcp_report_block_view::ssrc()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.ssrc() == 0x00010203);
}

TEST_CASE("rtcp_report_block_view::fraction_lost()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.fraction_lost() == 0x04);
}

TEST_CASE("rtcp_report_block_view::number_of_packets_lost()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.number_of_packets_lost() == 0x00050607);
}

TEST_CASE("rtcp_report_block_view::extended_highest_sequence_number_received()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.extended_highest_sequence_number_received() == 0x08090a0b);
}

TEST_CASE("rtcp_report_block_view::inter_arrival_jitter()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.inter_arrival_jitter() == 0x0c0d0e0f);
}

TEST_CASE("rtcp_report_block_view::last_sr_timestamp()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    const auto ts = report.last_sr_timestamp();
    REQUIRE(ts.integer() == 0x1011);
    REQUIRE(ts.fraction() == 0x12130000);
}

TEST_CASE("rtcp_report_block_view::delay_since_last_sr()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.delay_since_last_sr() == 0x14151617);
}

TEST_CASE("rtcp_report_block_view::data()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.data() == default_packet.data());
}

TEST_CASE("rtcp_report_block_view::size()", "[rtcp_report_block_view]") {
    const rav::rtcp_report_block_view report(default_packet.data(), default_packet.size());
    REQUIRE(report.size() == default_packet.size());
}
