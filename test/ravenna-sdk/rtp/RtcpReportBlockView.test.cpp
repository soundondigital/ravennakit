/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>

TEST_CASE("RtcpReportBlockView", "[RtcpReportBlockView]") {
    std::array<uint8_t, 24> packet {
        // SSRC
        0x00,
        0x01,
        0x02,
        0x03,
        // Fraction lost
        0x04,
        // Cumulative number of packets lost
        0x05,
        0x06,
        0x07,
        // Extended highest sequence number received
        0x08,
        0x09,
        0x0A,
        0x0B,
        // Inter arrival jitter
        0x0C,
        0x0D,
        0x0E,
        0x0F,
        // Last SR (LSR)
        0x10,
        0x11,
        0x12,
        0x13,
        // Delay since last SR (DLSR)
        0x14,
        0x15,
        0x16,
        0x17,
    };

    
}
