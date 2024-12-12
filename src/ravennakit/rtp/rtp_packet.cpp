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

void rav::rtp_packet::payload_type(const uint8_t value) {
    payload_type_ = value;
}

void rav::rtp_packet::sequence_number(const uint16_t value) {
    sequence_number_ = value;
}

uint16_t rav::rtp_packet::sequence_number_inc(const uint16_t value) {
    sequence_number_ += value;
    return sequence_number_;
}

void rav::rtp_packet::timestamp(const uint32_t value) {
    timestamp_ = value;
}

uint32_t rav::rtp_packet::timestamp_inc(const uint32_t value) {
    timestamp_ += value;
    return timestamp_;
}

void rav::rtp_packet::ssrc(const uint32_t value) {
    ssrc_ = value;
}

bool rav::rtp_packet::encode(const uint8_t* payload_data, const size_t payload_size, output_stream& stream) const {
    uint8_t v_p_x_cc = 0;
    v_p_x_cc |= 0b10000000;  // Version 2.
    v_p_x_cc |= 0b00000000;  // No padding.
    v_p_x_cc |= 0b00000000;  // No extension.
    v_p_x_cc |= 0b00000000;  // CSRC count of 0.
    if (stream.write_be(v_p_x_cc) != 1) {
        return false;
    }

    uint8_t m_pt = 0;
    m_pt |= 0b00000000;  // No marker bit.
    m_pt |= payload_type_ & 0b01111111;
    if (stream.write_be(m_pt) != 1) {
        return false;
    }

    // Sequence number
    if (stream.write_be(sequence_number_) != 2) {
        return false;
    }

    // Timestamp
    if (stream.write_be(timestamp_) != 4) {
        return false;
    }

    // SSRC
    if (stream.write_be(ssrc_) != 4) {
        return false;
    }

    // Payload
    if (stream.write(payload_data, payload_size) != payload_size) {
        return false;
    }

    return true;
}
