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
#include "ravennakit/ptp/messages/ptp_announce_message.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_announce_message") {
    SECTION("Unpack from data") {
        constexpr std::array<const uint8_t, 30> data = {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // origin_timestamp.seconds
            0x07, 0x08, 0x09, 0x0a,                          // origin_timestamp.nanoseconds
            0x0b, 0x0c,                                      // current_utc_offset
            0x00,                                            // reserved
            0x0d,                                            // grandmaster_priority1
            0x0e, 0x20, 0x10, 0x11,                          // grandmaster_clock_quality
            0x12,                                            // grandmaster_priority2
            0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,  // grandmaster_identity
            0x1b, 0x1c,                                      // steps_removed
            0x40,                                            // time source
        };

        auto announce = rav::ptp_announce_message::from_data(rav::buffer_view(data));
        REQUIRE(announce);
        REQUIRE(announce->origin_timestamp.seconds == 0x010203040506);
        REQUIRE(announce->origin_timestamp.nanoseconds == 0x0708090a);
        REQUIRE(announce->current_utc_offset == 0x0b0c);
        REQUIRE(announce->grandmaster_priority1 == 0x0d);
        REQUIRE(announce->grandmaster_clock_quality.clock_class == 0x0e);
        REQUIRE(announce->grandmaster_clock_quality.clock_accuracy == rav::ptp_clock_accuracy::lt_25_ns);
        REQUIRE(announce->grandmaster_clock_quality.offset_scaled_log_variance == 0x1011);
        REQUIRE(announce->grandmaster_priority2 == 0x12);
        REQUIRE(announce->grandmaster_identity.data[0] == 0x13);
        REQUIRE(announce->grandmaster_identity.data[1] == 0x14);
        REQUIRE(announce->grandmaster_identity.data[2] == 0x15);
        REQUIRE(announce->grandmaster_identity.data[3] == 0x16);
        REQUIRE(announce->grandmaster_identity.data[4] == 0x17);
        REQUIRE(announce->grandmaster_identity.data[5] == 0x18);
        REQUIRE(announce->grandmaster_identity.data[6] == 0x19);
        REQUIRE(announce->grandmaster_identity.data[7] == 0x1a);
        REQUIRE(announce->steps_removed == 0x1b1c);
        REQUIRE(announce->time_source == rav::ptp_time_source::ptp);
    }

    SECTION("Pack into stream") {
        rav::ptp_announce_message announce;
        announce.origin_timestamp.seconds = 0x010203040506;
        announce.origin_timestamp.nanoseconds = 0x0708090a;
        announce.current_utc_offset = 0x0b0c;
        announce.grandmaster_priority1 = 0x0d;
        announce.grandmaster_clock_quality.clock_class = 0x0e;
        announce.grandmaster_clock_quality.clock_accuracy = rav::ptp_clock_accuracy::lt_25_ns;
        announce.grandmaster_clock_quality.offset_scaled_log_variance = 0x1011;
        announce.grandmaster_priority2 = 0x12;
        announce.grandmaster_identity.data[0] = 0x13;
        announce.grandmaster_identity.data[1] = 0x14;
        announce.grandmaster_identity.data[2] = 0x15;
        announce.grandmaster_identity.data[3] = 0x16;
        announce.grandmaster_identity.data[4] = 0x17;
        announce.grandmaster_identity.data[5] = 0x18;
        announce.grandmaster_identity.data[6] = 0x19;
        announce.grandmaster_identity.data[7] = 0x1a;
        announce.steps_removed = 0x1b1c;
        announce.time_source = rav::ptp_time_source::ptp;

        rav::byte_stream stream;
        announce.write_to(stream);

        REQUIRE(stream.read_be<rav::uint48_t>().value() == 0x010203040506);  // origin_timestamp.seconds
        REQUIRE(stream.read_be<uint32_t>().value() == 0x0708090a);           // origin_timestamp.nanoseconds
        REQUIRE(stream.read_be<uint16_t>().value() == 0x0b0c);               // current_utc_offset
        REQUIRE(stream.read_be<uint8_t>().value() == 0x00);                  // reserved, should be 0
        REQUIRE(stream.read_be<uint8_t>().value() == 0x0d);                  // grandmaster_priority1
        REQUIRE(stream.read_be<uint8_t>().value() == 0x0e);                  // grandmaster_clock_quality.clock_class
        REQUIRE(stream.read_be<uint8_t>().value() == 0x20);                  // grandmaster_clock_quality.clock_accuracy
        REQUIRE(stream.read_be<uint16_t>().value() == 0x1011);  // grandmaster_clock_quality.offset_scaled_log_variance
        REQUIRE(stream.read_be<uint8_t>().value() == 0x12);     // grandmaster_priority2
        REQUIRE(stream.read_be<uint64_t>().value() == 0x131415161718191a);  // grandmaster_identity
        REQUIRE(stream.read_be<uint16_t>().value() == 0x1b1c);              // steps_removed
        REQUIRE(stream.read_be<uint8_t>().value() == 0x40);                 // time source
    }
}
