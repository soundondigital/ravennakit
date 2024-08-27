/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <ravennakit/rtp/RtcpReportBlockView.h>

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

TEST_CASE("RtcpReportBlockView | is_valid()", "[RtcpReportBlockView]") {
    SECTION("Valid when the view points to data") {
        const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
        REQUIRE(report.is_valid());
    }

    SECTION("Also valid when pointing to data with a size of 0") {
        const rav::RtcpReportBlockView report(default_packet.data(), 0);
        REQUIRE(report.is_valid());
    }

    SECTION("Not valid when pointing to nullptr and no size") {
        const rav::RtcpReportBlockView report(nullptr, 0);
        REQUIRE_FALSE(report.is_valid());
    }

    SECTION("Also not valid when pointing to nullptr but with size") {
        const rav::RtcpReportBlockView report(nullptr, 1);
        REQUIRE_FALSE(report.is_valid());
    }
}

TEST_CASE("RtcpReportBlockView | validate()", "[RtcpReportBlockView]") {
    SECTION("Validation should fail when the view doesn't point to data") {
        const rav::RtcpReportBlockView report(nullptr, 0);
        REQUIRE(rav::rtp::Result::InvalidPointer == report.validate());
    }

    SECTION("Validation should fail when the packet is too short") {
        const rav::RtcpReportBlockView report(default_packet.data(), 23);
        REQUIRE(rav::rtp::Result::InvalidReportBlockLength == report.validate());
    }

    SECTION("Validation should fail when the packet is too long") {
        const rav::RtcpReportBlockView report(default_packet.data(), 25);
        REQUIRE(rav::rtp::Result::InvalidReportBlockLength == report.validate());
    }

    SECTION("Or else validation should pass") {
        const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
        REQUIRE(rav::rtp::Result::Ok == report.validate());
    }
}

TEST_CASE("RtcpReportBlockView | ssrc()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.ssrc() == 0x00010203);
}

TEST_CASE("RtcpReportBlockView | fraction_lost()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.fraction_lost() == 0x04);
}

TEST_CASE("RtcpReportBlockView | number_of_packets_lost()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.number_of_packets_lost() == 0x00050607);
}

TEST_CASE("RtcpReportBlockView | extended_highest_sequence_number_received()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.extended_highest_sequence_number_received() == 0x08090a0b);
}

TEST_CASE("RtcpReportBlockView | inter_arrival_jitter()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.inter_arrival_jitter() == 0x0c0d0e0f);
}

TEST_CASE("RtcpReportBlockView | last_sr_timestamp()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    const auto ts = report.last_sr_timestamp();
    REQUIRE(ts.integer() == 0x1011);
    REQUIRE(ts.fraction() == 0x12130000);
}

TEST_CASE("RtcpReportBlockView | delay_since_last_sr()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.delay_since_last_sr() == 0x14151617);
}

TEST_CASE("RtcpReportBlockView | data()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.data() == default_packet.data());
}

TEST_CASE("RtcpReportBlockView | data_length()", "[RtcpReportBlockView]") {
    const rav::RtcpReportBlockView report(default_packet.data(), default_packet.size());
    REQUIRE(report.data_length() == default_packet.size());
}
