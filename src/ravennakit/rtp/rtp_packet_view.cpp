/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <fmt/core.h>

#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

namespace {
constexpr size_t kRtpHeaderBaseLengthOctets = 12;
constexpr size_t kHeaderExtensionLengthOctets = sizeof(uint16_t) * 2;
}  // namespace

rav::rtp_packet_view::rtp_packet_view(const uint8_t* data, const size_t size_bytes) :
    data_(data), size_bytes_(size_bytes) {}

bool rav::rtp_packet_view::validate() const {
    if (data_ == nullptr) {
        return false;
    }

    if (size_bytes_ < kRtpHeaderBaseLengthOctets || size_bytes_ < header_total_length()) {
        return false;
    }

    if (version() > 2) {
        return false;
    }

    return true;
}

bool rav::rtp_packet_view::marker_bit() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[1] & 0b10000000) >> 7 != 0;
}

uint8_t rav::rtp_packet_view::payload_type() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return data_[1] & 0b01111111;
}

uint16_t rav::rtp_packet_view::sequence_number() const {
    constexpr auto kOffset = 2;
    if (size_bytes_ < kOffset + sizeof(uint16_t)) {
        return 0;
    }

    return byte_order::read_be<uint16_t>(&data_[kOffset]);
}

uint32_t rav::rtp_packet_view::timestamp() const {
    constexpr auto kOffset = 4;
    if (size_bytes_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kOffset]);
}

uint32_t rav::rtp_packet_view::ssrc() const {
    constexpr auto kOffset = 8;
    if (size_bytes_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kOffset]);
}

uint8_t rav::rtp_packet_view::version() const {
    if (size_bytes_ < 1) {
        return 0;
    }
    return (data_[0] & 0b11000000) >> 6;
}

bool rav::rtp_packet_view::padding() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[0] & 0b00100000) >> 5 != 0;
}

bool rav::rtp_packet_view::extension() const {
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[0] & 0b00010000) >> 4 != 0;
}

uint32_t rav::rtp_packet_view::csrc_count() const {
    if (size_bytes_ < 1) {
        return 0;
    }
    return data_[0] & 0b00001111;
}

uint32_t rav::rtp_packet_view::csrc(const uint32_t index) const {
    if (index >= csrc_count()) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kRtpHeaderBaseLengthOctets + index * sizeof(uint32_t)]);
}

uint16_t rav::rtp_packet_view::get_header_extension_defined_by_profile() const {
    if (!extension()) {
        return 0;
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);
    uint16_t data;
    std::memcpy(&data, &data_[header_extension_start_index], sizeof(data));
    return data;
}

rav::buffer_view<const uint8_t> rav::rtp_packet_view::get_header_extension_data() const {
    if (!extension()) {
        return {};
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);

    const auto num_32bit_words = byte_order::read_be<uint16_t>(&data_[header_extension_start_index + sizeof(uint16_t)]);
    const auto data_start_index = header_extension_start_index + kHeaderExtensionLengthOctets;
    return {&data_[data_start_index], num_32bit_words * sizeof(uint32_t)};
}

size_t rav::rtp_packet_view::header_total_length() const {
    size_t extension_length_octets = 0;  // Including the extension header.
    if (extension()) {
        extension_length_octets = kHeaderExtensionLengthOctets;
        const auto extension = get_header_extension_data();
        extension_length_octets += extension.size_bytes();
    }
    return kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t) + extension_length_octets;
}

rav::buffer_view<const unsigned char> rav::rtp_packet_view::payload_data() const {
    if (data_ == nullptr) {
        return {};
    }

    if (size_bytes_ < header_total_length()) {
        return {};
    }

    return {data_ + header_total_length(), size_bytes_ - header_total_length()};
}

std::string rav::rtp_packet_view::to_string() const {
    return fmt::format(
        "RTP Header: valid={} version={} padding={} extension={} csrc_count={} market_bit={} payload_type={} sequence_number={} timestamp={} ssrc={} payload_start_index={}",
        validate(), version(), padding(), extension(), csrc_count(), marker_bit(), payload_type(),
        sequence_number(), timestamp(), ssrc(), header_total_length()
    );
}
