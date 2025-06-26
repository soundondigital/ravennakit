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
#include "ravennakit/ptp/messages/ptp_follow_up_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ptp::FollowUpMessage") {
    SECTION("Unpack") {
        constexpr std::array<const uint8_t, 30> data {
            0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90,
        };
        auto follow = rav::ptp::FollowUpMessage::from_data({}, rav::BufferView(data)).value();
        REQUIRE(follow.precise_origin_timestamp.raw_seconds() == 0x123456789012);
        REQUIRE(follow.precise_origin_timestamp.raw_nanoseconds() == 0x34567890);
    }
}
