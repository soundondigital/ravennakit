/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_packet.hpp"

void rav::rtp::Packet::payload_type(const uint8_t value) {
    payload_type_ = value;
}

void rav::rtp::Packet::sequence_number(const uint16_t value) {
    sequence_number_ = value;
}

rav::wrapping_uint<unsigned short> rav::rtp::Packet::sequence_number_inc(const uint16_t value) {
    sequence_number_ += value;
    return sequence_number_;
}

void rav::rtp::Packet::timestamp(const uint32_t value) {
    timestamp_ = value;
}

rav::wrapping_uint<unsigned> rav::rtp::Packet::timestamp_inc(const uint32_t value) {
    timestamp_ += value;
    return timestamp_;
}

void rav::rtp::Packet::ssrc(const uint32_t value) {
    ssrc_ = value;
}

void rav::rtp::Packet::encode(const uint8_t* payload_data, const size_t payload_size, byte_buffer& buffer) const {
    uint8_t v_p_x_cc = 0;
    v_p_x_cc |= 0b10000000;  // Version 2.
    v_p_x_cc |= 0b00000000;  // No padding.
    v_p_x_cc |= 0b00000000;  // No extension.
    v_p_x_cc |= 0b00000000;  // CSRC count of 0.
    buffer.write_be(v_p_x_cc);

    uint8_t m_pt = 0;
    m_pt |= 0b00000000;  // No marker bit.
    m_pt |= payload_type_ & 0b01111111;
    buffer.write_be(m_pt);

    // Sequence number
    buffer.write_be(sequence_number_);

    // Timestamp
    buffer.write_be(timestamp_);

    // SSRC
    buffer.write_be(ssrc_);

    // Payload
    buffer.write(payload_data, payload_size);
}
