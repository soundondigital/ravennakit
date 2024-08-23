/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravenna-sdk/rtp/RtpPacketView.hpp"

#include <fmt/core.h>

#include <asio/detail/socket_ops.hpp>

#include "ravenna-sdk/platform/ByteOrder.hpp"

namespace {
constexpr size_t kRtpHeaderBaseLengthOctets = 12;
constexpr size_t kHeaderExtensionLengthOctets = sizeof(uint16_t) * 2;
}  // namespace

rav::RtpPacketView::RtpPacketView(const uint8_t* data, const size_t data_length) :
    data_(data), data_length_(data_length) {}

rav::rtp::Result rav::RtpPacketView::verify() const {
    if (data_ == nullptr) {
        return rtp::Result::InvalidPointer;
    }

    if (data_length_ < kRtpHeaderBaseLengthOctets || data_length_ < header_total_length()) {
        return rtp::Result::InvalidHeaderLength;
    }

    if (version() > 2) {
        return rtp::Result::InvalidVersion;
    }

    return rtp::Result::Ok;
}

bool rav::RtpPacketView::marker_bit() const {
    if (data_length_ < 1) {
        return false;
    }
    return (data_[1] & 0b10000000) >> 7 != 0;
}

uint8_t rav::RtpPacketView::payload_type() const {
    if (data_length_ < 1) {
        return false;
    }
    return data_[1] & 0b01111111;
}

uint16_t rav::RtpPacketView::sequence_number() const {
    constexpr auto kOffset = 2;
    if (data_length_ < kOffset + sizeof(uint16_t)) {
        return 0;
    }

    return byte_order::read_be<uint16_t>(&data_[kOffset]);
}

uint32_t rav::RtpPacketView::timestamp() const {
    constexpr auto kOffset = 4;
    if (data_length_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kOffset]);
}

uint32_t rav::RtpPacketView::ssrc() const {
    constexpr auto kOffset = 8;
    if (data_length_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kOffset]);
}

uint8_t rav::RtpPacketView::version() const {
    if (data_length_ < 1) {
        return 0;
    }
    return (data_[0] & 0b11000000) >> 6;
}

bool rav::RtpPacketView::padding() const {
    if (data_length_ < 1) {
        return false;
    }
    return (data_[0] & 0b00100000) >> 5 != 0;
}

bool rav::RtpPacketView::extension() const {
    if (data_length_ < 1) {
        return false;
    }
    return (data_[0] & 0b00010000) >> 4 != 0;
}

uint32_t rav::RtpPacketView::csrc_count() const {
    if (data_length_ < 1) {
        return 0;
    }
    return data_[0] & 0b00001111;
}

uint32_t rav::RtpPacketView::csrc(const uint32_t index) const {
    if (index >= csrc_count()) {
        return 0;
    }
    return byte_order::read_be<uint32_t>(&data_[kRtpHeaderBaseLengthOctets + index * sizeof(uint32_t)]);
}

uint16_t rav::RtpPacketView::get_header_extension_defined_by_profile() const {
    if (!extension()) {
        return 0;
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);
    uint16_t data;
    std::memcpy(&data, &data_[header_extension_start_index], sizeof(data));
    return data;
}

rav::BufferView<const uint8_t> rav::RtpPacketView::get_header_extension_data() const {
    if (!extension()) {
        return {};
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);

    const auto num_32bit_words = byte_order::read_be<uint16_t>(&data_[header_extension_start_index + sizeof(uint16_t)]);
    const auto data_start_index = header_extension_start_index + kHeaderExtensionLengthOctets;
    return {&data_[data_start_index], num_32bit_words * sizeof(uint32_t)};
}

size_t rav::RtpPacketView::header_total_length() const {
    size_t extension_length_octets = 0;  // Including the extension header.
    if (extension()) {
        extension_length_octets = kHeaderExtensionLengthOctets;
        const auto extension = get_header_extension_data();
        extension_length_octets += extension.size_bytes();
    }
    return kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t) + extension_length_octets;
}

rav::BufferView<const unsigned char> rav::RtpPacketView::payload_data() const {
    if (data_ == nullptr) {
        return {};
    }

    if (data_length_ < header_total_length()) {
        return {};
    }

    return {data_ + header_total_length(), data_length_ - header_total_length()};
}

std::string rav::RtpPacketView::to_string() const {
    return fmt::format(
        "RTP Header: valid={} version={} padding={} extension={} csrc_count={} market_bit={} payload_type={} sequence_number={} timestamp={} ssrc={} payload_start_index={}",
        verify() == rtp::Result::Ok, version(), padding(), extension(), csrc_count(), marker_bit(), payload_type(),
        sequence_number(), timestamp(), ssrc(), header_total_length()
    );
}
