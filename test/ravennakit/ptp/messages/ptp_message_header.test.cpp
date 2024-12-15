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
#include "ravennakit/ptp/messages/ptp_message_header.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_message_header") {
    SECTION("Unpack from data") {
        constexpr std::array<const uint8_t, 300> data {
            0xfd,                                            // majorSdoId & messageType
            0x12,                                            // minorVersionPTP & versionPTP
            0x01, 0x2c,                                      // messageLength (300)
            0x01,                                            // domainNumber
            0x22,                                            // minorSdoId
            0x00, 0xff,                                      // flags
            0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00,  // correctionField
            0x12, 0x34, 0x56, 0x78,                          // message type specific (ignored)
            0x12, 0x34, 0x56, 0x78, 0x00, 0x02, 0x80, 0x00,  // sourcePortIdentity.clockIdentity
            0xab, 0xcd,                                      // sourcePortIdentity.portNumber
            0x11, 0x22,                                      // sequenceId
            0xff,                                            // controlField (ignored)
            0x81,                                            // logMessageInterval
        };

        auto header = rav::ptp_message_header::from_data(rav::buffer_view(data));

        REQUIRE(header.has_value());
        REQUIRE(header->sdo_id == 0xf22);
        REQUIRE(header->message_type == rav::ptp_message_type::management);
        REQUIRE(header->version.major == 0x2);
        REQUIRE(header->version.minor == 0x1);
        REQUIRE(header->message_length == 300);
        REQUIRE(header->domain_number == 1);

        REQUIRE(header->flags.alternate_master_flag == false);
        REQUIRE(header->flags.two_step_flag == false);
        REQUIRE(header->flags.unicast_flag == false);
        REQUIRE(header->flags.profile_specific_1 == false);
        REQUIRE(header->flags.profile_specific_2 == false);
        REQUIRE(header->flags.leap61 == true);
        REQUIRE(header->flags.leap59 == true);
        REQUIRE(header->flags.current_utc_offset_valid == true);
        REQUIRE(header->flags.ptp_timescale == true);
        REQUIRE(header->flags.time_traceable == true);
        REQUIRE(header->flags.frequency_traceable == true);
        REQUIRE(header->flags.synchronization_uncertain == true);

        REQUIRE(header->correction_field == 0x28000);
        REQUIRE(header->source_port_identity.clock_identity.data[0] == 0x12);
        REQUIRE(header->source_port_identity.clock_identity.data[1] == 0x34);
        REQUIRE(header->source_port_identity.clock_identity.data[2] == 0x56);
        REQUIRE(header->source_port_identity.clock_identity.data[3] == 0x78);
        REQUIRE(header->source_port_identity.clock_identity.data[4] == 0x00);
        REQUIRE(header->source_port_identity.clock_identity.data[5] == 0x02);
        REQUIRE(header->source_port_identity.clock_identity.data[6] == 0x80);
        REQUIRE(header->source_port_identity.clock_identity.data[7] == 0x00);
        REQUIRE(header->source_port_identity.port_number == 0xabcd);
        REQUIRE(header->sequence_id == 0x1122);
        REQUIRE(header->log_message_interval == -127);

        SECTION("Test flags") {}
    }

    SECTION("Pack to stream") {
        rav::byte_stream stream;
        rav::ptp_message_header header;
        header.sdo_id = 0xf22;
        header.message_type = rav::ptp_message_type::management;
        header.version.major = 0x2;
        header.version.minor = 0x1;
        header.message_length = 300;
        header.domain_number = 1;
        header.correction_field = 0x28000;
        header.source_port_identity.clock_identity.data[0] = 0x12;
        header.source_port_identity.clock_identity.data[1] = 0x34;
        header.source_port_identity.clock_identity.data[2] = 0x56;
        header.source_port_identity.clock_identity.data[3] = 0x78;
        header.source_port_identity.clock_identity.data[4] = 0x00;
        header.source_port_identity.clock_identity.data[5] = 0x02;
        header.source_port_identity.clock_identity.data[6] = 0x80;
        header.source_port_identity.clock_identity.data[7] = 0x00;
        header.source_port_identity.port_number = 0xabcd;
        header.sequence_id = 0x1122;
        header.log_message_interval = -127;
        header.write_to(stream);

        REQUIRE(stream.read_be<uint8_t>().value() == 0xfd);                 // majorSdoId & messageType
        REQUIRE(stream.read_be<uint8_t>().value() == 0x12);                 // minorVersionPTP & versionPTP
        REQUIRE(stream.read_be<uint16_t>().value() == 300);                 // messageLength
        REQUIRE(stream.read_be<uint8_t>().value() == 1);                    // domainNumber
        REQUIRE(stream.read_be<uint8_t>().value() == 0x22);                 // minorSdoId
        REQUIRE(stream.read_be<uint16_t>().value() == 0x00);                // flags
        REQUIRE(stream.read_be<int64_t>().value() == 0x28000);              // correctionField
        REQUIRE(stream.read_be<uint32_t>().value() == 0);                   // message type specific (ignored)
        REQUIRE(stream.read_be<uint64_t>().value() == 0x1234567800028000);  // sourcePortIdentity.clockIdentity
        REQUIRE(stream.read_be<uint16_t>().value() == 0xabcd);              // sourcePortIdentity.portNumber
        REQUIRE(stream.read_be<uint16_t>().value() == 0x1122);              // sequenceId
        REQUIRE(stream.read_be<uint8_t>().value() == 0x0);                  // controlField (ignored)
        REQUIRE(stream.read_be<int8_t>().value() == -127);                  // logMessageInterval
    }
}

TEST_CASE("ptp_message_header::flag_field") {
    SECTION("Unpack to octets") {
        uint8_t octet1 = 0;
        uint8_t octet2 = 0;

        auto flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.alternate_master_flag == false);
        REQUIRE(flags.two_step_flag == false);
        REQUIRE(flags.unicast_flag == false);
        REQUIRE(flags.profile_specific_1 == false);
        REQUIRE(flags.profile_specific_2 == false);
        REQUIRE(flags.leap61 == false);
        REQUIRE(flags.leap59 == false);
        REQUIRE(flags.current_utc_offset_valid == false);
        REQUIRE(flags.ptp_timescale == false);
        REQUIRE(flags.time_traceable == false);
        REQUIRE(flags.frequency_traceable == false);
        REQUIRE(flags.synchronization_uncertain == false);

        octet1 = 1 << 0;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.alternate_master_flag == true);

        octet1 = 1 << 1;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.two_step_flag == true);

        octet1 = 1 << 2;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.unicast_flag == true);

        octet1 = 1 << 5;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.profile_specific_1 == true);

        octet1 = 1 << 6;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.profile_specific_2 == true);

        octet2 = 1 << 0;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.leap61 == true);

        octet2 = 1 << 1;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.leap59 == true);

        octet2 = 1 << 2;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.current_utc_offset_valid == true);

        octet2 = 1 << 3;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.ptp_timescale == true);

        octet2 = 1 << 4;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.time_traceable == true);

        octet2 = 1 << 5;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.frequency_traceable == true);

        octet2 = 1 << 6;
        flags = rav::ptp_message_header::flag_field::from_octets(octet1, octet2);
        REQUIRE(flags.synchronization_uncertain == true);
    }

    SECTION("Pack to octets") {
        rav::ptp_message_header::flag_field flags;

        SECTION("Leap 61") {
            flags.leap61 = true;
            REQUIRE(flags.to_octets() == 0b00000000'00000001);
        }

        SECTION("Leap 59") {
            flags.leap59 = true;
            REQUIRE(flags.to_octets() == 0b00000000'00000010);
        }

        SECTION("Current UTC offset valid") {
            flags.current_utc_offset_valid = true;
            REQUIRE(flags.to_octets() == 0b00000000'00000100);
        }

        SECTION("PTP timescale") {
            flags.ptp_timescale = true;
            REQUIRE(flags.to_octets() == 0b00000000'00001000);
        }

        SECTION("Time traceable") {
            flags.time_traceable = true;
            REQUIRE(flags.to_octets() == 0b00000000'00010000);
        }

        SECTION("Frequency traceable") {
            flags.frequency_traceable = true;
            REQUIRE(flags.to_octets() == 0b00000000'00100000);
        }

        SECTION("Synchronization uncertain") {
            flags.synchronization_uncertain = true;
            REQUIRE(flags.to_octets() == 0b00000000'01000000);
        }

        SECTION("Alternate master flag") {
            flags.alternate_master_flag = true;
            REQUIRE(flags.to_octets() == 0b00000001'00000000);
        }

        SECTION("Two step flag") {
            flags.two_step_flag = true;
            REQUIRE(flags.to_octets() == 0b00000010'00000000);
        }

        SECTION("Unicast flag") {
            flags.unicast_flag = true;
            REQUIRE(flags.to_octets() == 0b00000100'00000000);
        }

        SECTION("Profile specific 1") {
            flags.profile_specific_1 = true;
            REQUIRE(flags.to_octets() == 0b00100000'00000000);
        }

        SECTION("Profile specific 2") {
            flags.profile_specific_2 = true;
            REQUIRE(flags.to_octets() == 0b01000000'00000000);
        }
    }

    SECTION("Pack with all fields set, make sure reserved fields are zero") {
        rav::ptp_message_header::flag_field flags;
        flags.alternate_master_flag = true;
        flags.two_step_flag = true;
        flags.unicast_flag = true;
        flags.profile_specific_1 = true;
        flags.profile_specific_2 = true;
        flags.leap61 = true;
        flags.leap59 = true;
        flags.current_utc_offset_valid = true;
        flags.ptp_timescale = true;
        flags.time_traceable = true;
        flags.frequency_traceable = true;
        flags.synchronization_uncertain = true;
        REQUIRE(flags.to_octets() == 0b01100111'01111111);
    }
}
