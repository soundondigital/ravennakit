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
#include "ravennakit/ptp/messages/ptp_sync_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_sync_message") {
    SECTION("Unpack") {
        rav::ptp_message_header header;
        rav::byte_stream stream;
        header.write_to(stream);



        constexpr std::array<const uint8_t, 30> data {
            0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90,
        };
        auto sync = rav::ptp_sync_message::from_data({}, rav::buffer_view(data)).value();
        REQUIRE(sync.origin_timestamp.seconds == 0x123456789012);
        REQUIRE(sync.origin_timestamp.nanoseconds == 0x34567890);
    }

    SECTION("Pack") {
        rav::ptp_sync_message sync;
        sync.origin_timestamp.seconds = 0x123456789012;
        sync.origin_timestamp.nanoseconds = 0x34567890;
        rav::byte_stream stream;
        sync.write_to(stream);
        REQUIRE(stream.size() == rav::ptp_sync_message::k_message_length);
        stream.skip(rav::ptp_message_header::k_header_size);
        REQUIRE(stream.read_be<rav::uint48_t>() == sync.origin_timestamp.seconds);
        REQUIRE(stream.read_be<uint32_t>() == sync.origin_timestamp.nanoseconds);
    }
}
