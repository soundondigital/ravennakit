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
#include "ravennakit/ptp/messages/ptp_sync_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ptp::SyncMessage") {
    SECTION("Unpack") {
        rav::ByteStream stream;
        constexpr std::array<const uint8_t, 30> data {
            0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90,
        };
        auto sync = rav::ptp::SyncMessage::from_data({}, rav::BufferView(data)).value();
        REQUIRE(sync.origin_timestamp.raw_seconds() == 0x123456789012);
        REQUIRE(sync.origin_timestamp.raw_nanoseconds() == 0x34567890);
    }

    SECTION("Pack") {
        rav::ptp::SyncMessage sync;
        sync.origin_timestamp = rav::ptp::Timestamp(0x123456789012, 0x34567890);
        rav::ByteBuffer buffer;
        sync.write_to(buffer);

        rav::InputStreamView buffer_view(buffer);
        REQUIRE(buffer_view.size() == rav::ptp::SyncMessage::k_message_length);
        REQUIRE(buffer_view.skip(rav::ptp::MessageHeader::k_header_size));
        REQUIRE(buffer_view.read_be<rav::uint48_t>() == sync.origin_timestamp.raw_seconds());
        REQUIRE(buffer_view.read_be<uint32_t>() == sync.origin_timestamp.raw_nanoseconds());
    }
}
