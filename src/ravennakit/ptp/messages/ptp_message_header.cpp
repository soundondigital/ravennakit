/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_message_header.hpp"
#include "ravennakit/core/byte_order.hpp"

#include <bitset>

bool rav::operator==(const ptp_version& lhs, const ptp_version& rhs) {
    return lhs.major == rhs.major && lhs.minor == rhs.minor;
}

bool rav::operator!=(const ptp_version& lhs, const ptp_version& rhs) {
    return !(lhs == rhs);
}

rav::ptp_message_header::flag_field
rav::ptp_message_header::flag_field::from_octets(const uint8_t octet1, const uint8_t octet2) {
    flag_field flags_field;

    std::bitset<8> octet1_bits(octet1);
    flags_field.alternate_master_flag = octet1_bits[0];
    flags_field.two_step_flag = octet1_bits[1];
    flags_field.unicast_flag = octet1_bits[2];
    std::ignore = octet1_bits[3];
    std::ignore = octet1_bits[4];
    flags_field.profile_specific_1 = octet1_bits[5];
    flags_field.profile_specific_2 = octet1_bits[6];
    std::ignore = octet1_bits[7];

    std::bitset<8> octet2_bits(octet2);
    flags_field.leap61 = octet2_bits[0];
    flags_field.leap59 = octet2_bits[1];
    flags_field.current_utc_offset_valid = octet2_bits[2];
    flags_field.ptp_timescale = octet2_bits[3];
    flags_field.time_traceable = octet2_bits[4];
    flags_field.frequency_traceable = octet2_bits[5];
    flags_field.synchronization_uncertain = octet2_bits[6];
    std::ignore = octet2_bits[7];

    return flags_field;
}

uint16_t rav::ptp_message_header::flag_field::to_octets() const {
    uint16_t octets = 0;
    octets |= profile_specific_2 << 14;
    octets |= profile_specific_1 << 13;
    octets |= unicast_flag << 10;
    octets |= two_step_flag << 9;
    octets |= alternate_master_flag << 8;
    octets |= synchronization_uncertain << 6;
    octets |= frequency_traceable << 5;
    octets |= time_traceable << 4;
    octets |= ptp_timescale << 3;
    octets |= current_utc_offset_valid << 2;
    octets |= leap59 << 1;
    octets |= leap61 << 0;
    return octets;
}

auto rav::ptp_message_header::flag_field::tie_members() const {
    return std::tie(
        alternate_master_flag, two_step_flag, unicast_flag, profile_specific_1, profile_specific_2, leap61, leap59,
        current_utc_offset_valid, ptp_timescale, time_traceable, frequency_traceable, synchronization_uncertain
    );
}

tl::expected<rav::ptp_message_header, rav::ptp_error>
rav::ptp_message_header::from_data(buffer_view<const uint8_t> data) {
    if (data.empty()) {
        return tl::unexpected(ptp_error::invalid_data);
    }

    ptp_message_header header;
    header.message_length = data.read_be<uint16_t>(2);
    if (data.size() != header.message_length) {
        return tl::unexpected(ptp_error::invalid_message_length);
    }

    header.sdo_id.major = (data[0] & 0b11110000) >> 4;
    header.sdo_id.minor = data[5];
    header.message_type = static_cast<ptp_message_type>(data[0] & 0b00001111);
    header.version.major = data[1] & 0b00001111;
    header.version.minor = (data[1] & 0b11110000) >> 4;
    header.domain_number = data[4];
    header.flags = flag_field::from_octets(data[6], data[7]);
    header.correction_field = data.read_be<int64_t>(8);
    // Type specific octets are ignored (4 octets)
    std::memcpy(header.source_port_identity.clock_identity.data.data(), data.data() + 20, 8);
    header.source_port_identity.port_number = data.read_be<uint16_t>(28);
    header.sequence_id = data.read_be<uint16_t>(30);
    // Control field is ignored (1 byte)
    header.log_message_interval = static_cast<int8_t>(data[33]);

    return header;
}

void rav::ptp_message_header::write_to(byte_buffer& buffer) const {
    // major sdo id + message type (left shift by multiplication to avoid type promotion)
    buffer.write_be<uint8_t>(((sdo_id.major & 0b00001111) * 16) | (static_cast<uint8_t>(message_type) & 0b00001111));
    // minor version ptp + version ptp (left shift by multiplication to avoid type promotion)
    buffer.write_be<uint8_t>((version.minor * 16) | (version.major & 0b00001111));
    buffer.write_be<uint16_t>(message_length);
    buffer.write_be<uint8_t>(domain_number);
    buffer.write_be<uint8_t>(sdo_id.minor);
    buffer.write_be<uint16_t>(flags.to_octets());
    buffer.write_be<int64_t>(correction_field);
    buffer.write_be<uint32_t>(0);  // Ignored
    source_port_identity.write_to(buffer);
    buffer.write_be<uint16_t>(sequence_id.value());
    buffer.write_be<uint8_t>(0);  // Ignored
    buffer.write_be<int8_t>(log_message_interval);
}

std::string rav::ptp_message_header::to_string() const {
    return fmt::format(
        "PTP {}: sdo_id={} version={}.{} domain_number={} sequence_id={} source_port_identity={}.{}",
        rav::to_string(message_type), sdo_id.to_string(), version.major, version.minor, domain_number,
        sequence_id.value(), source_port_identity.clock_identity.to_string(), source_port_identity.port_number
    );
}

auto rav::ptp_message_header::tie() const {
    return std::tie(
        sdo_id, message_type, version, message_length, domain_number, flags, correction_field, source_port_identity,
        sequence_id, log_message_interval
    );
}

bool rav::ptp_message_header::matches(const ptp_message_header& other) const {
    return source_port_identity == other.source_port_identity && sequence_id == other.sequence_id;
}

bool rav::operator==(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs) {
    return lhs.tie_members() == rhs.tie_members();
}

bool rav::operator!=(const ptp_message_header::flag_field& lhs, const ptp_message_header::flag_field& rhs) {
    return lhs.tie_members() != rhs.tie_members();
}

bool rav::operator==(const ptp_message_header& lhs, const ptp_message_header& rhs) {
    return lhs.tie() == rhs.tie();
}

bool rav::operator!=(const ptp_message_header& lhs, const ptp_message_header& rhs) {
    return lhs.tie() != rhs.tie();
}
