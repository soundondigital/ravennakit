/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

namespace {
constexpr size_t kRtpHeaderBaseLengthOctets = 12;
constexpr size_t kHeaderExtensionLengthOctets = sizeof(uint16_t) * 2;
}  // namespace

rav::rtp::PacketView::PacketView(const uint8_t* data, const size_t size_bytes) :
    data_(data), size_bytes_(size_bytes) {}

bool rav::rtp::PacketView::validate() const {
    if (data_ == nullptr) {
        return false;
    }

    if (size_bytes_ < kRtpHeaderBaseLengthOctets || size_bytes_ < header_total_length()) {
        return false;
    }

    if (version() != 2) {
        return false;
    }



    return true;
}

bool rav::rtp::PacketView::marker_bit() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[1] & 0b10000000) >> 7 != 0;
}

uint8_t rav::rtp::PacketView::payload_type() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return false;
    }
    return data_[1] & 0b01111111;
}

uint16_t rav::rtp::PacketView::sequence_number() const {
    constexpr auto kOffset = 2;
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < kOffset + sizeof(uint16_t)) {
        return 0;
    }

    return read_be<uint16_t>(&data_[kOffset]);
}

uint32_t rav::rtp::PacketView::timestamp() const {
    constexpr auto kOffset = 4;
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return read_be<uint32_t>(&data_[kOffset]);
}

uint32_t rav::rtp::PacketView::ssrc() const {
    constexpr auto kOffset = 8;
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < kOffset + sizeof(uint32_t)) {
        return 0;
    }
    return read_be<uint32_t>(&data_[kOffset]);
}

uint8_t rav::rtp::PacketView::version() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return 0;
    }
    return (data_[0] & 0b11000000) >> 6;
}

bool rav::rtp::PacketView::padding() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[0] & 0b00100000) >> 5 != 0;
}

bool rav::rtp::PacketView::extension() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return false;
    }
    return (data_[0] & 0b00010000) >> 4 != 0;
}

uint32_t rav::rtp::PacketView::csrc_count() const {
    // TODO: This check should probably be skipped, assuming validate() is called before calling this method.
    if (size_bytes_ < 1) {
        return 0;
    }
    return data_[0] & 0b00001111;
}

uint32_t rav::rtp::PacketView::csrc(const uint32_t index) const {
    if (index >= csrc_count()) {
        return 0;
    }
    return read_be<uint32_t>(&data_[kRtpHeaderBaseLengthOctets + index * sizeof(uint32_t)]);
}

uint16_t rav::rtp::PacketView::get_header_extension_defined_by_profile() const {
    if (!extension()) {
        return 0;
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);
    uint16_t data;
    std::memcpy(&data, &data_[header_extension_start_index], sizeof(data));
    return data;
}

rav::buffer_view<const uint8_t> rav::rtp::PacketView::get_header_extension_data() const {
    if (!extension()) {
        return {};
    }

    const auto header_extension_start_index = kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t);

    const auto num_32bit_words = read_be<uint16_t>(&data_[header_extension_start_index + sizeof(uint16_t)]);
    const auto data_start_index = header_extension_start_index + kHeaderExtensionLengthOctets;
    return {&data_[data_start_index], num_32bit_words * sizeof(uint32_t)};
}

size_t rav::rtp::PacketView::header_total_length() const {
    size_t extension_length_octets = 0;  // Including the extension header.
    if (extension()) {
        extension_length_octets = kHeaderExtensionLengthOctets;
        const auto extension = get_header_extension_data();
        extension_length_octets += extension.size_bytes();
    }
    return kRtpHeaderBaseLengthOctets + csrc_count() * sizeof(uint32_t) + extension_length_octets;
}

rav::buffer_view<const uint8_t> rav::rtp::PacketView::payload_data() const {
    if (data_ == nullptr) {
        return {};
    }

    const auto header_length = header_total_length();
    if (size_bytes_ < header_length) {
        return {};
    }

    return {data_ + header_length, size_bytes_ - header_length};
}

size_t rav::rtp::PacketView::size() const {
    return size_bytes_;
}

const uint8_t* rav::rtp::PacketView::data() const {
    return data_;
}

std::string rav::rtp::PacketView::to_string() const {
    return fmt::format(
        "RTP Header: valid={} version={} padding={} extension={} csrc_count={} market_bit={} payload_type={} sequence_number={} timestamp={} ssrc={} payload_start_index={}",
        validate(), version(), padding(), extension(), csrc_count(), marker_bit(), payload_type(),
        sequence_number(), timestamp(), ssrc(), header_total_length()
    );
}
